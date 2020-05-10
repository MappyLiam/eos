/********************************************************
 * Filename: core/task.c
 * 
 * Author: parkjy, RTOSLab. SNU.
 * 
 * Description: task management.
 ********************************************************/
#include <core/eos.h>
#include <core/eos_internal.h>

// NEW, TERMINATED는 내가 만듦
// #define NEW			0 
#define READY		1
#define RUNNING		2
#define WAITING		3
// #define TERMINATED	4

/*
 * Queue (list) of tasks that are ready to run.
 */
static _os_node_t *_os_ready_queue[LOWEST_PRIORITY + 1];

/*
 * Pointer to TCB of running task
 */
static eos_tcb_t *_os_current_task;

int32u_t eos_create_task(eos_tcb_t *task, addr_t sblock_start, size_t sblock_size, void (*entry)(void *arg), void *arg, int32u_t priority) {
	// PRINT("task: 0x%x, priority: %d\n", (int32u_t)task, priority);
    // PRINT("stack base: 0x%x \n", (int32u_t *)sblock_start);
	addr_t stack_ptr = _os_create_context(sblock_start, sblock_size, entry, arg);
    // PRINT("stack ptr is 0x%x \n", (int32u_t *)stack_ptr);

	(*task).stack_ptr = stack_ptr;
	PRINT("Initial saved_stack_ptr : 0x%x \n", (int32u_t *)(*task).stack_ptr);	
	(*task).state = READY;
	// (*task).priority = priority;
    // PRINT("before ready queue \n");
	_os_node_t * node_in_ready_queue = &(*task).node_in_ready_queue;
    (*node_in_ready_queue).ptr_data = task;
	(*node_in_ready_queue).priority = priority;
	PRINT("task : 0x%x \n", (int32u_t *)task);
    // PRINT("after ready queue \n");
    // PRINT("task in node_in_ready_queue : 0x%x \n", (int32u_t *)(*node_in_ready_queue).ptr_data);

	_os_add_node_tail(&(_os_ready_queue[priority]), &(*task).node_in_ready_queue);

    // PRINT("head address : 0x%x \n", (_os_ready_queue[priority]));
    // PRINT("task address : 0x%x \n", (int32u_t *)(task));
    // PRINT("task in first of queue : 0x%x \n", (int32u_t *)(*_os_ready_queue[priority]).ptr_data);
    // PRINT("task in node_in_ready_queue : 0x%x \n", (int32u_t *)(*node_in_ready_queue).ptr_data);
	return 0;
}

int32u_t eos_destroy_task(eos_tcb_t *task) {
}

void eos_schedule() {
	eos_tcb_t * current_task = eos_get_current_task();
	int32u_t priority = _os_get_highest_priority();
    priority = 0;
    // PRINT("priority is %d \n", priority);
	// TODO: 만약 위 priority안통하면, 
	// 	처음에 priority에 맞는 위치 찾는 걸 _os_add_node_priority로 구현하자.
	if (current_task){ // if current task is specified
        // PRINT("current task is specified \n");
		PRINT("restored stack ptr : 0x%x \n", (int32u_t *)(*current_task).stack_ptr);
		addr_t saved_stack_ptr = _os_save_context();
		// PRINT("saved_stack_ptr : 0x%x \n", (int32u_t *)saved_stack_ptr);
		if (saved_stack_ptr != NULL){
			(*current_task).stack_ptr = saved_stack_ptr;
			(*current_task).state = READY;
			// 왜 이 아래 PRINT만 있으면 잘 돌지? 
			PRINT("saved_stack_ptr : 0x%x \n", (int32u_t *)saved_stack_ptr);
			_os_node_t * first_node_in_queue = _os_ready_queue[priority];
			if (first_node_in_queue){
				eos_tcb_t * selected_task = (*first_node_in_queue).ptr_data;
				(*selected_task).state = RUNNING;
				_os_current_task = selected_task;
				PRINT("selected task : 0x%x \n", (int32u_t *)selected_task);
				addr_t stack_ptr = (*selected_task).stack_ptr;
				PRINT("selected saved stack_ptr : 0x%x \n", (int32u_t *)stack_ptr);
				_os_remove_node(&_os_ready_queue[priority], first_node_in_queue);
				_os_add_node_tail(&_os_ready_queue[priority], first_node_in_queue);
				// PRINT("stack_ptr before restore_context: 0x%x \n", (int32u_t *)stack_ptr);
				_os_restore_context((*selected_task).stack_ptr);
				// PRINT("Done restore \n")
			} else { // this case, there is no ready task in ready queue
				return;
			}
		} else { // if it's right after the context is restored
			return;
		}
	} else { // if current task is not specified (thus, it's initialization.)
        // PRINT("first schedule \n");
		_os_node_t * first_node_in_queue = _os_ready_queue[priority];
		if (first_node_in_queue){
			eos_tcb_t * selected_task = (*first_node_in_queue).ptr_data;
			(*selected_task).state = RUNNING;
			_os_current_task = selected_task;
			PRINT("selected task : 0x%x \n", (int32u_t *)selected_task);
			addr_t stack_ptr = (*selected_task).stack_ptr;
			PRINT("selected saved stack_ptr : 0x%x \n", (int32u_t *)stack_ptr);
			_os_remove_node(&_os_ready_queue[priority], first_node_in_queue);
			// PRINT("done remove node \n");
			_os_add_node_tail(&_os_ready_queue[priority], first_node_in_queue);
			// PRINT("done add node \n");
			_os_restore_context((*selected_task).stack_ptr);
			// PRINT("Done restore \n")
		} else { // this case, there is no ready task in ready queue
			return;
		}
	}
}

eos_tcb_t *eos_get_current_task() {
	return _os_current_task;
}

void eos_change_priority(eos_tcb_t *task, int32u_t priority) {
}

int32u_t eos_get_priority(eos_tcb_t *task) {
}

void eos_set_period(eos_tcb_t *task, int32u_t period){
}

int32u_t eos_get_period(eos_tcb_t *task) {
}

int32u_t eos_suspend_task(eos_tcb_t *task) {
}

int32u_t eos_resume_task(eos_tcb_t *task) {
}

void eos_sleep(int32u_t tick) {
}

void _os_init_task() {
	PRINT("initializing task module.\n");

	/* init current_task */
	_os_current_task = NULL;

	/* init multi-level ready_queue */
	int32u_t i;
	for (i = 0; i < LOWEST_PRIORITY; i++) {
		_os_ready_queue[i] = NULL;
	}
}

void _os_wait(_os_node_t **wait_queue) {
}

void _os_wakeup_single(_os_node_t **wait_queue, int32u_t queue_type) {
}

void _os_wakeup_all(_os_node_t **wait_queue, int32u_t queue_type) {
}

void _os_wakeup_sleeping_task(void *arg) {
}
