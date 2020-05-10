#include <core/eos.h>
#define STACK_SIZE 8096

static eos_tcb_t tcb1;
static eos_tcb_t tcb2;
static int8u_t stack1[STACK_SIZE];
static int8u_t stack2[STACK_SIZE];

/* task1 function - print number 1 to 20 repeatedly */
static void print_number(void *arg) {
	int i = 0; 
	while(++i) {
		// PRINT("num! \n");
		// PRINT("%d \n", i);
		printf("%d", i); 
		eos_schedule();
		if (i == 20) { i = 0; }
	}
}

/* task2 function - print alphabet a to z repeatedly */
static void print_alphabet(void *arg) {
	int i = 96; 
	while(++i) {
		// PRINT("alpha! \n");
		// PRINT("%c \n", i);
		printf("%c", i); 
		eos_schedule();
		if (i == 122) { i = 96; }
	}
}

void eos_user_main() {
	eos_create_task(&tcb1, (addr_t)stack1, STACK_SIZE, print_number, NULL, 0);
	eos_create_task(&tcb2, (addr_t)stack2, STACK_SIZE, print_alphabet, NULL, 0);
}
