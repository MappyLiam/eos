Question:
1. 시스템초기화, 아직 구현되지 않은 부분은 추상적으로 놔두는지.
2. _deliver_irq에 resume_eip: 부분 뜻이 무엇인지. 인터넷에서 찾아도 쉽지 않음.

#TODO:
1. _os_initialization 내부의 여러 함수들 분석
2. c 콜 

유의- 
mov 의 사용방식, **memory location vs register value를 잘 구분하고 외어버리자.**
(%esp) 는 C의 *로 생각하자. 포인터 값 주소의 값으로 생각하면 된다.

본 프로젝트 과제는 다음의 항목으로 이루어진다.
1. EOS initialization
2. Interrupt Reqeust
3. Interrupt Management Module
4. C code 분석
아래 main함수에서 vector table의 reset entry로 jump후 초기화 완료시까지의 코드를 분석해보겠다.

##########################
### 1. EOS initializer ###
##########################

### hal/linux/emulator/vector.c ###
/* jump to reset vector. esp := 0 */
int main(int argc, char **argv) {  // 메인함수 진입
	PRINT("reset\n");              // 'reset'을 알리는 프린트
	_eflags = 0;                   // status register를 0으로 하여 interrupt를 disable한다
	__asm__ __volatile__ ("\       // assembly code를 실행하기위한 구문, auto optimization없이 그대로 실행하도록 하였다.
		mov $0x0, %%esp;\          // 스택포인터 esp레지스터에 0을 입력
		jmp *%0"                   // 첫 번째 operand를 %0위치로 치환하고 해당 절대주소 값으로 jump
		:: "r"(_vector[0]));       // 범용 레지스터에 _vector[0]을 할당하여 위 assembler template에서 읽음. %0으로 접근됨.
	/* never return here */
	return 0;                      // 위 assembly code에서 jump하므로 실행되지 않음
}

### hal/linux/entry.S ###
_vector:                           // 아래 코드 section의 이름이 _vector이다.
	.long _os_reset_handler        // _vector[0]은 이 routine의 주소를 가리킨다.
	.long _os_reset_handler        // _vector[1]은 이 routine의 주소를 가리킨다.
	.long _os_reset_handler        // _vector[2]은 이 routine의 주소를 가리킨다.
	.long _os_irq_handler          // _vector[3]은 이 routine의 주소를 가리킨다.


.text                              // 아래 부분을 text section으로 컴파일하기에 executable 부분이 된다.
.global _os_reset_handler          // _os_reset_handler는 linked된 다른 프로그램 부분에 availble해진다.
_os_reset_handler:                 // 위 벡터테이블에 따라 이 rountine으로 이동한다.
	_CLI                           // disable interrupt 역할. 이는 emulator_asm.h에 정의되어있는 매크로이다.
                                   // movl $0, _eflags;로 치환되며, 이는 상수0을 _eflags에 넣어 interrupt를 disable. 
	lea _os_init_stack_end, %esp   // _os_init_stack_end 주소를 스택포인터 esp 레지스터에 할당 
	call _os_initialization        // core/main.c에 정의된 _os_initialization을 call한다. 
                                   // 이때 다음 명령어의 주소와 local variables들을 stack에 push한다.
	jmp _os_reset_handler          // ************** _os_initialization 에서 돌아오지 않지만, 이 명령어는 다시 함수로 점프한다.

### core/main.c ###
void _os_initialization() {
	_os_multitasking = 0;          // bool type의 os_multitasking을 0으로 한다. 멀티태스킹을 disable하는 것.
	eos_disable_interrupt();       // _eflags를 0으로 세팅하여 interrupt를 disable

	// Initialize subsystems.
	_os_init_hal();                // 하드웨어 dependent한 부분 초기화. 내부에서는 Interval timer를 초기화하고 unmasking을 통해 enable해준다.
	_os_init_icb_table();          // interrupt control block을 초기화한다. irqnum에 index를, 핸들러에 Null을 할당하는 것이다.
	_os_init_scheduler();          // scheduler모듈을 초기화한다. ready_group을 0으로 하고, _os_ready_table를 모두 0으로 초기화한다. 또한 _os_scheduler_lock을 Unlock[숫자 1]한다.
	_os_init_task();               // task모듈을 초기화한다. _os_current_task을 Null로 하고, _os_ready_queue도 모두 0으로 초기화.
	_os_init_timer();              // system_timer를 초기화하고, icb table에 timer interrupt를 등록한다

	// Create idle task.
	PRINT("creating idle task.\n");
	eos_create_task(&idle_task, (int32u_t *)idle_stack, 8096, _os_idle_task, NULL, LOWEST_PRIORITY);                                                        
                                   // 위 명령어에서는 지금은 아무것도 하지 않는다.
	// After finishing initializations by kernel,                                                       
	// give users a chance to do application specific initializations.                                                      
	extern void eos_user_main();   // 외부함수인 eos_user_main의 사용 선언
	eos_user_main();               // 유저가 specific initialization을 할 수 있는 함수.

	// Start multitasking.
	PRINT("finishing initialization. starting multitasking.\n");                                                        
	_os_multitasking = 1;          // 멀티태스킹을 enable한다.
	eos_enable_interrupt();        // 인터럽트를 enable한다. 아래 Interrupt Management Module에서 자세히 분석하겠다.

	eos_schedule();                // 아직 구현되지 않았다.

	// After finishing all initializations, OS enters loop.                                                     
	while(1) {}                    // 프로그램으로서 OS가 종료되어선 안되기에 무한루프를 통해 프로그램은 유지된다.
}                                                       



############################
### 2. Interrupt Reqeust ###
############################

### hal/linux/emulator/intr.c ###
void _gen_irq(int8u_t irq) {           // Interrupt Reqeust를 generate하는 함수
	_irq_pending |= (0x1 << irq);      // parameter로 request번호를 4bit의 pending변수에 OR연산으로 bit masking한다.
									   // _irq_pending는 one-hot encoding방식으로 pending된 irq를 나타낸다.
	_deliver_irq();                    // irq를 cpu에 전달하는 루틴을 호출한다.
}


### hal/linux/emulator/vector.c ###
/* deliver an irq to CPU */
void _deliver_irq() {                  // 
	//PRINT("_eflags: %d, _irq_pending: 0x%x, _irq_mask: 0x%x\n", _eflags, _irq_pending, _irq_mask);
	if (_irq_pending & ~_irq_mask) {   // pending된 irq가 enable되어있는지 확인한다.
		if (_eflags == 1) {            // interrupt가 enable인지 확인한다.
			_eflags = 0;               // interrupt시작하기 앞서, 다른 interrupt 발생을 막기위해 0으로 reset
			_eflags_saved = 1;         // enable상태의 eflag를 변수에 저장해둔다. 후에 _os_irq_handler에서 사용한다.
			//PRINT("interrupted\n");  // 
			__asm__ __volatile__ ("\   // assembly code를 실행하기위한 구문, auto optimization없이 그대로 실행하도록 하였다.
				push $resume_eip;\     // resume_eip label 주소를 memory stack에 push한다.
									   // 이때 esp의 값은 원래 값에서 -4 된다.
				jmp *%0;\              // 첫 번째 operand를 %0위치로 치환하고 해당 절대주소 값으로 jump. _os_irq_handler로 점프한다.
			resume_eip:"               // asm label로, 본 label 주소를 위에서 push했다.
				:: "r"(_vector[3]));   // 범용 레지스터에 _vector[3]을 할당하여 위 assembler template에서 읽음. %0으로 접근됨.
		}                              // 위 벡터의 3번째 원소는 _os_irq_handler의 루틴 주소를 가리킨다.
	}
}

### hal/linux/entry.S ###
.global _os_irq_handler                // _os_irq_handler는 linked된 다른 프로그램 부분에 availble해진다.
_os_irq_handler:                       // 위에서 벡터테이블에 따라 이 루틴으로 이동한다.
	pusha                              // push all
	push _eflags_saved                 // 위에서 저장한 _eflags_saved를 stack에 push한다.
	call _os_common_interrupt_handler  // _os_common_interrupt_handler 루틴을 호출한다. 자세한건 아래에서 다룬다.
	add $0x4,%esp                      // 돌아온 후, esp(stackpointer)에 4를 더한다.
	popa                               // pop all
	_IRET                              // stack에서 instruction pointer를 pop한다. 아래 매크로로 정의되어 있다.

### hal/linux/emulator_asm.h ###
/* interrupt return. */
#define _IRET \
	call _deliver_irq;\                // _deliver_irq를 call하여 실행한다.
	ret;                               // Stack에 pop하여 return address를 얻고, jump한다.


### core/interrupt.c ###
void _os_common_interrupt_handler(int32u_t flag) {
	/* get the irq number */
	int32u_t irq_num = eos_get_irq();  // 실행할 irq number를 가져오는 함수 호출. Interrupt Management Module에서 구체적으로 다룬다.
	if (irq_num == -1) { return; }     // -1이면 실행할 irq가 _irq_pending register에 없는 것이므로 return한다.
	
	/* acknowledge the irq */          // 
	eos_ack_irq(irq_num);              // _irq_pending register에서 해당 irq bit를 clear한다.
	
	/* restore the _eflags */          // 
	eos_restore_interrupt(flag);       // **************인자를 어떻게 받는지.

	/* dispatch the handler and call it */                 
	_os_icb_t *p = &_os_icb_table[irq_num]; // irq num에 해당하는 ICB Block을 가리키는 포인터 설정 
	if (p->handler != NULL) {          		// 해당 ICB에 핸들러가 있는지 확인
		//PRINT("entering irq handler 0x%x\n", (int32u_t)(p->handler));
		p->handler(irq_num, p->arg);   		// 있다면 핸들러에 irq넘버와 argments를 넣고 호출
		//PRINT("exiting irq handler 0x%x\n", (int32u_t)(p->handler));
	}
}

######################################
### 3. Interrupt Management Module ###
######################################
### hal/linux/interrupt.c ###

/* ack the specified irq */
void eos_ack_irq(int32u_t irq) {
	/* clear the corresponding bit in _irq_pending register */
	_irq_pending &= ~(0x1<<irq);	// _irq_pending register에서 해당 irq bit를 clear한다.
}

/* get the irq number */
int32s_t eos_get_irq() {															
	/* get the highest bit position in the _irq_pending register */
	int i;
	for(i=31; i>=0; i--) {			// MSB부터 LSB방향으로 하나씩 검토한다
		if (_irq_pending & ~_irq_mask & (0x1<<i)) { // pending되어있으면서 enable인 irq를 찾는다.
			return i;				// 먼저 찾아지는 irq가 있다면, 이 irq number를 리턴한다.
		}							// 
	}								// 
	return -1;						// 위 조건에 해당하는 irq number가 없으면 -1을 리턴한다.
}

/* mask an irq */
void eos_disable_irq_line(int32u_t irq) {
	/* turn on the corresponding bit */
	_irq_mask |= (0x1<<irq);		// 해당 irq number를 _irq_mask에 set하여 disable한다.
}

/* unmask an irq */
void eos_enable_irq_line(int32u_t irq) {
	/* turn off the corresponding bit */
	_irq_mask &= ~(0x1<<irq);		// 해당 irq number를 _irq_mask에 reset하여 enable한다.
}

### hal/linux/interrupt_asm.S ###

/* disable irq and return previous status */
.global eos_disable_interrupt       // eos_disable_interrupt는 linked된 다른 프로그램 부분에 availble해진다.
eos_disable_interrupt:              // 아래 코드는 eos_disable_interrupt 루틴의 assembly code이다.
	mov _eflags, %eax               // _eflags값을 eax register에 넣는다.
	_CLI                            // 매크로이다. movl $0, _eflags;로 치환되며, 이는 상수0을 _eflags에 넣어 interrupt를 disable. 
	ret                             // Stack에 pop하여 return address를 얻고, jump한다.

/* enable irq by force */
.global eos_enable_interrupt        // eos_enable_interrupt는 linked된 다른 프로그램 부분에 availble해진다.
eos_enable_interrupt:               // 아래 코드는 eos_enable_interrupt 루틴의 assembly code이다.
	_STI                            // 매크로이다. _eflags를 set하여 interrupt를 enable하고, _deliver_irq를 call한다.
	ret                             // Stack에 pop하여 return address를 얻고, jump한다.

/* restore irq status */
.global eos_restore_interrupt       // eos_restore_interrupt는 linked된 다른 프로그램 부분에 availble해진다.
eos_restore_interrupt:              // 아래 코드는 eos_restore_interrupt 루틴의 assembly code이다.
	mov 0x4(%esp), %eax             // esp레지스터가 가리키는 메모리주소 +4의 값, 즉 parameter로 들어온 flag를 eax레지스터에 넣는다.
	mov %eax, _eflags               // eax레지스터의 값을 _eflags에 넣는다.
	ret                             // Stack에 pop하여 return address를 얻고, jump한다.


######################################
### 4. C Code Subroutine Analysis  ###
######################################

아래 코드 및 분석은 main label부터 읽음을 가정하겠다.

	.file	"project1.c"				// project1.c
	.text								//
	.globl	add							// add routine은 global availble해진다
	.type	add, @function				// 
add:									// add routine
.LFB0:									// 
	.cfi_startproc						// 
	pushl	%ebp						// ebp레지스터를 push한다.
	.cfi_def_cfa_offset 8				// 
	.cfi_offset 5, -8					// 
	movl	%esp, %ebp					// esp의 값을 ebp에 복사한다.
	.cfi_def_cfa_register 5				// 
	subl	$16, %esp					// esp가 가리키는 주소값에 16을 빼고 저장한다.
	movl	8(%ebp), %edx				// ebp가 가리키는 주소에 8을 더한 곳의 값을 edx에 넣는다.(즉, 첫 번째 parameter)
	movl	12(%ebp), %eax				// ebp가 가리키는 주소에 12를 더한 곳의 값을 eax에 넣는다.(즉, 두 번째 parameter)
	addl	%edx, %eax					// edx 레지스터의 값을 eax 레지스터 값에 더하여 eax에 저장한다.
	movl	%eax, -4(%ebp)				// eax 레지스터의 값을 ebp 레지스터가 가리키는 주소의 -4인 곳에 넣는다.
	movl	-4(%ebp), %eax				// ebp 레지스터가 가리키는 주소의 -4인 곳의 값을 eax레지스터에 넣는다.
	leave								// 현재 rountine의 stack을 초기화한다. 자세한 설명은 main에서 다루었으므로 생략한다.
	.cfi_restore 5						// 
	.cfi_def_cfa 4, 4					// 
	ret									// 여기서 return address를 pop하여 eip에 넣고 jump한다.
	.cfi_endproc						// 
.LFE0:									// 
	.size	add, .-add					// 
	.globl	mul							// 
	.type	mul, @function				// 
mul:									// mul routine
.LFB1:									// 
	.cfi_startproc						// 
	pushl	%ebp						// ebp레지스터를 push한다.
	.cfi_def_cfa_offset 8				// 
	.cfi_offset 5, -8					// 
	movl	%esp, %ebp					// esp레지스터의 값을 ebp에 복사한다.
	.cfi_def_cfa_register 5				// 
	movl	8(%ebp), %eax				// ebp레지스터가 가리키는 주소에 8을 더한 곳의 값을 eax에 넣는다.(즉, 첫 번째 parameter)
	imull	12(%ebp), %eax				// ebp레지스터가 가리키는 주소에 12를 더한 곳의 값에 eax레지스터의 값을 곱하고 eax에 넣는다.(즉, 두 번째 parameter)
	popl	%ebp						// stack의 top 데이터를 ebp로 pop한다. 
										// 즉, 이전 ebp의 값을 다시 ebp레지스터에 넣는다. 이때, esp는 4만큼 증가한다.
										// 다른 루틴에서 leave를 사용한 반면 여기서는 이와같이 한 이유는,
										// 본 루틴에서는 local variable사용 등으로 인한 esp의 이동이 발생하지 않았기에 이것으로 충분하였다.
	.cfi_restore 5						// 
	.cfi_def_cfa 4, 4					// 
	ret									// 여기서 return address를 pop하여 eip에 넣고 jump한다. 
	.cfi_endproc						// 
.LFE1:									// 
	.size	mul, .-mul					// 
	.globl	main						// 
	.type	main, @function				// 
main:									// 
.LFB2:									// local label이다. 이후 설명을 생략한다.
	.cfi_startproc						// CFI(call frame information)은 exception handling을 위해 사용된다. 
										// 아래 cfi들은 마찬가지이므로 설명을 생략한다.
	pushl	%ebp						// ebp레지스터를 stack에 push
	.cfi_def_cfa_offset 8				// 
	.cfi_offset 5, -8					// 
	movl	%esp, %ebp					// esp레지스터의 값을 ebp레지스터에 복사한다.
	.cfi_def_cfa_register 5				// 
	subl	$16, %esp					// esp레지스터의 값에서 16을 빼고 저장한다.
	movl	$10, -16(%ebp)				// ebp레지스터가 가리키는 주소에서 -16인 곳에 10을 넣는다. 변수 a
	movl	$10, -12(%ebp)				// ebp레지스터가 가리키는 주소에서 -12인 곳에 10을 넣는다. 변수 b
	movl	$5, -8(%ebp)				// ebp레지스터가 가리키는 주소에서 -8인 곳에 5을 넣는다. 변수 c
	pushl	-8(%ebp)					// ebp레지스터가 가리키는 주소에서 -8인 변수 c를 push한다. 
										// push시 esp가 가리키는 주소에 값을 넣고 esp는 4만큼 감소하는 동작을 한다.
	pushl	-12(%ebp)					// ebp레지스터가 가리키는 주소에서 -12인 변수 b를 push한다.
	call	add							// add routine을 call한다. 위의 push된 b, c는 parameter로 사용된다.
	addl	$8, %esp					// esp레지스터의 값에서 8을 더하고 저장한다.
	pushl	%eax						// eax레지스터의 값을 push한다. 이는 add routine의 return value이다.
	pushl	-16(%ebp)					// ebp레지스터가 가리키는 주소에서 -16인 변수 a를 push한다.
	call	mul							// mul routine을 call한다. 위의 push된 eax와 a는 parameter로 사용된다.
	addl	$8, %esp					// esp레지스터의 값에서 8을 더하고 저장한다.
	movl	%eax, %edx					// eax레지스터의 값을 edx레지스터에 저장한다. 여기서 eax레지스터의 값은 mul 루틴의 return value이다.
	movl	-4(%ebp), %eax				// ebp레지스터가 가리키는 주소에서 -4인 공간의 주소를 eax레지스터에 복사한다.
	movl	%edx, (%eax)				// edx레지스터의 값을 eax레지스터가 가리키는 주소의 값에 edx레지스터의 값을 복사한다.
	movl	$0, %eax					// eax레지스터의 값에 0을 넣는다.
	leave								// 현재 rountine의 stack을 초기화한다.
										// 현재 ebp가 가리키는 주소를 esp에 넣고, stack에 있는 이전 ebp의 값을 pop하여 ebp레지스터에 넣는다.
										// 즉, 이전 stack frame에서 return address를 top으로 갖는 ebp, esp상태가 된다.
	.cfi_restore 5						// 
	.cfi_def_cfa 4, 4					// 
	ret									// 여기서 return address를 pop하여 eip에 넣고 jump한다. 
	.cfi_endproc						// 
.LFE2:									// 
	.size	main, .-main				// 
	.ident	"GCC: (Ubuntu 5.4.0-6ubuntu1~16.04.11) 5.4.0 20160609"
	.section	.note.GNU-stack,"",@progbits
