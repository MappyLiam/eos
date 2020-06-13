/********************************************************
 * Filename: core/task.c
 * 
 * Author: parkjy, RTOSLab. SNU.
 * 
 * Description: task management.
 ********************************************************/
#include <core/eos.h>
#include <core/eos_internal.h>

#define READY		1
#define RUNNING		2
#define WAITING		3

/*
 * Queue (list) of tasks that are ready to run.
 */
static _os_node_t *_os_ready_queue[LOWEST_PRIORITY + 1];

/*
 * Pointer to TCB of running task
 */
static eos_tcb_t *_os_current_task;

int32u_t eos_create_task(eos_tcb_t *task, addr_t sblock_start, size_t sblock_size, void (*entry)(void *arg), void *arg, int32u_t priority) {
	addr_t stack_ptr = _os_create_context(sblock_start, sblock_size, entry, arg);
	(*task).state = READY;
	(*task).stack_ptr = stack_ptr;
	(*task).stack_base = sblock_start;
	(*task).stack_size = sblock_size;
	(*task).start_time = eos_get_system_timer()->tick;
	(*task).entry = entry;
	(*task).arg = arg;

	_os_node_t * node_of_ready_queue = &(*task).node_of_queue;
    (*node_of_ready_queue).ptr_data = task;
	(*node_of_ready_queue).priority = priority;
	
	_os_node_t * alarm_queue_node = &(*task).alarm.alarm_queue_node;
	(*alarm_queue_node).ptr_data = &(*task).alarm;
	(*alarm_queue_node).previous = NULL;
	(*alarm_queue_node).next = NULL;

	eos_set_period(task, 0);
	_os_add_node_tail(&(_os_ready_queue[priority]), &(*task).node_of_queue);
	_os_set_ready(priority);

	return 0;
}

int32u_t eos_destroy_task(eos_tcb_t *task) {
}

void eos_schedule() {
	if (eos_get_current_task()){ // if current task is specified
		addr_t saved_stack_ptr = _os_save_context();
		if (saved_stack_ptr != NULL){
			(*_os_current_task).stack_ptr = saved_stack_ptr;
			if ((*_os_current_task).state == WAITING){
				// If it's sleep now, don't set ready
				// PRINT("It's waiting. So not into ready queue\n");
			} else {
				(*_os_current_task).state = READY;
				(*_os_current_task).start_time = eos_get_system_timer()->tick;
				// PRINT("Add to ready queue\n");
				_os_set_ready((_os_current_task -> node_of_queue).priority);
				_os_add_node_tail(&_os_ready_queue[(*_os_current_task).node_of_queue.priority], &((*_os_current_task).node_of_queue));
			}
		} else { // if it's right after the context is restored
			return;
		}
	}
	// PRINT("befo get priority\n");
	int32u_t priority = _os_get_highest_priority();
	// PRINT("PRIORITY - %d\n", priority);
	_os_node_t * first_node_in_queue = _os_ready_queue[priority];
	// PRINT("Befo Go to remove\n");
	_os_remove_node(&_os_ready_queue[priority], first_node_in_queue);
	// PRINT("After Go to Remove\n");

	// if there's no more node in queue, then unset
	if (_os_ready_queue[priority] == NULL) {
		// PRINT("Unser Ready priority - %d\n", priority);
		_os_unset_ready(priority);
	}
	
	_os_current_task = (*first_node_in_queue).ptr_data;
	(*_os_current_task).state = RUNNING;
	_os_restore_context((*_os_current_task).stack_ptr);
}

eos_tcb_t *eos_get_current_task() {
	return _os_current_task;
}

void eos_change_priority(eos_tcb_t *task, int32u_t priority) {
}

int32u_t eos_get_priority(eos_tcb_t *task) {
}

void eos_set_period(eos_tcb_t *task, int32u_t period){
	(*task).period = period;
}

int32u_t eos_get_period(eos_tcb_t *task) {
}

int32u_t eos_suspend_task(eos_tcb_t *task) {
}

int32u_t eos_resume_task(eos_tcb_t *task) {
}

void eos_sleep(int32u_t tick) {
	// PRINT("sleep start\n");
	// eos_alarm_t alarm;
	// alarm.alarm_queue_node.next = (_os_node_t *)NULL;
	// alarm.alarm_queue_node.previous = (_os_node_t *)NULL;
	eos_tcb_t * cur_task = eos_get_current_task();
	(*cur_task).state = WAITING;
	int32u_t timeout = (*cur_task).period + (*cur_task).start_time;
	// int32u_t priority = (*cur_task).node_of_queue.priority;
	
	// _os_node_t * cur_node = &((*cur_task).node_of_queue);
	// if (cur_node->next == cur_node-> previous){
	// 	// Thus, if this node is head
	// 	// Oh.. It's a bit wrong. There's no logic for unsetting ready of unhead node
	// 	// PRINT("Unset ready\n");
	// 	_os_unset_ready(priority);
	// }
	// PRINT("befo set alarm\n");
	eos_set_alarm(eos_get_system_timer(), &(*cur_task).alarm, timeout, _os_wakeup_sleeping_task, cur_task);
	// PRINT("after set alarm\n");
	eos_schedule();
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

_os_node_t * _get_os_ready_queue() {
	return _os_ready_queue;
}

void _os_wakeup_sleeping_task(void *arg) {
	eos_tcb_t * task = (eos_tcb_t *) arg;
	int32u_t priority = (task -> node_of_queue).priority;
	_os_set_ready(priority);
	(*task).start_time = eos_get_system_timer()->tick;
	(*task).state = READY;
	_os_add_node_tail(&_os_ready_queue[priority], &(*task).node_of_queue);
}
