#include <core/eos.h>
#include <core/eos_internal.h>
#include "emulator_asm.h"

typedef struct _os_context {
	/* low address */
	int32u_t edi;
	int32u_t esi;
	int32u_t ebx;
	int32u_t edx;
	int32u_t ecx;
	int32u_t eax;
	int32u_t _eflags;
	int32u_t eip;
	/* high address */	
} _os_context_t;

void print_context(addr_t context) {
	if(context == NULL) return;
	_os_context_t *ctx = (_os_context_t *)context;
	//PRINT("reg1  =0x%x\n", ctx->reg1);
	//PRINT("reg2  =0x%x\n", ctx->reg2);
	//PRINT("reg3  =0x%x\n", ctx->reg3);
	//...`
}

// void (*entry)(void *) -> function pointer

addr_t _os_create_context(addr_t stack_base, size_t stack_size, void (*entry)(void *), void *arg) {
	int32u_t stack_ptr = (int32u_t *)((int8u_t *)stack_base + stack_size); // pointer연산 시, pointer자료형에 따라 더해지는 값이 달라진다. 
																	 // stack_size는 byte(8bit)단위로 보기 위해 int8u_t으로 형변환하여 연산해준다.
	// The reason of setting stack_ptr to int32u_t is to use increment/decrement address by 32bit(4byte) 
	*(--stack_ptr) = *(int32u_t *) arg;
	*(--stack_ptr) = NULL;
	*(--stack_ptr) = (int32u_t *) entry;
	*(--stack_ptr) = 1;
	for(int8u_t i = 0; i < 6; i++){
		// register save, but the value is don't care, which is NULL
		*(--stack_ptr) = NULL;
	}
	return (addr_t)stack_ptr;
}

void _os_restore_context(addr_t sp) {
	__asm__ __volatile__ ("\
		mov %[sp] %%esp
		pop %%edi;\
		pop %%esi;\
		pop %%ebx;\
		pop %%edx;\
		pop %%ecx;\
		pop %%eax;\
		pop _eflags;\
		mov 4(%%esp) %%ebp
		ret;
		"
		:: [sp] "m" (sp));
}

addr_t _os_save_context() {
	// eax에 esp를 넣는 부분이, return 값을 넣어주는 부분이다. 
	__asm__ __volatile__ ("\
		push $resume_eip;\
		push _eflags;\
		push %%eax;\
		push %%ecx;\
		push %%edx;\
		push %%ebx;\
		push %%esi;\
		push %%edi;\
		mov %%esp %%eax;\
		pushl 4(%%ebp);\
		pushl 0(%%ebp);\
		mov %%esp %%ebp;
		resume_eip:
		leave
		ret"
		::);
}
