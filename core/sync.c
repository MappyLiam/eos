/********************************************************
 * Filename: core/sync.c
 * 
 * Author: wsyoo, RTOSLab. SNU.
 * 
 * Description: semaphore, condition variable management.
 ********************************************************/
#include <core/eos.h>

#define READY		1
#define RUNNING		2
#define WAITING		3

void eos_init_semaphore(eos_semaphore_t *sem, int32u_t initial_count, int8u_t queue_type) {
	/* initialization */
	(*sem).queue_type = queue_type;
	(*sem).count = (int32s_t)initial_count;
	(*sem).wait_queue = NULL;
}

int32u_t eos_acquire_semaphore(eos_semaphore_t *sem, int32s_t timeout) {
    // PRINT("In acquire semaphore\n");
	eos_disable_interrupt();
	if ((*sem).count > 0){
		(*sem).count--;
		eos_enable_interrupt();
		return 1;
	}
	eos_enable_interrupt();

	if (timeout == -1) {
		return 0;
	} 

	while(1){
		// PRINT("Loop! \n");
		eos_tcb_t* cur_task = eos_get_current_task();
		(*cur_task).state = WAITING;
		if ((*sem).queue_type == FIFO){
			// PRINT("Add node FIFO\n");
			_os_add_node_tail(&(*sem).wait_queue, &(*cur_task).node_of_queue);
		} else if ((*sem).queue_type == PRIORITY){
			// PRINT("Add node Priority\n");
			_os_add_node_priority(&(*sem).wait_queue, &(*cur_task).node_of_queue);
		}
		if (timeout > 0) {
			// timeout 동안만 대기
			// PRINT("Timeout > 0 !\n");
			eos_tcb_t * cur_task = eos_get_current_task();
			(*cur_task).state = WAITING;
			int32u_t alarm_timeout = timeout + (*cur_task).start_time;
			wakeup_args_t wakeup_args;
			wakeup_args.task = cur_task;
			wakeup_args.sem = sem;
			eos_set_alarm(eos_get_system_timer(), &(*cur_task).alarm, alarm_timeout, _os_wakeup_sleeping_task_in_waiting_queue, &wakeup_args);
			eos_schedule();
		} else { // timeout == 0
			// 다른 태스크 반환될 때 까지 wait queue에서 대기
			// PRINT("Timeout 0 !\n");
			eos_schedule();
		}
		// PRINT("Go again\n");
		eos_disable_interrupt();
		// wait하고 돌아오면 다시 체크
		if ((*sem).count > 0){
			(*sem).count--;
			eos_enable_interrupt();
			return 1;
		}
		eos_enable_interrupt();
	}
}

void eos_release_semaphore(eos_semaphore_t *sem) {
	// PRINT("In release semaphore\n");
	eos_disable_interrupt();
	(*sem).count++;
	if (sem->wait_queue != NULL) {
		eos_enable_interrupt();
		// wait queue에서 해당 task를 제거
		// PRINT("Check here\n");
		_os_node_t* wait_queue = (_os_node_t*)(*sem).wait_queue;
		// PRINT("Or here\n");
		eos_tcb_t* wait_task = (eos_tcb_t*)(*wait_queue).ptr_data;
		// PRINT("Befo remove wait node\n");
		_os_remove_node(&sem->wait_queue, &(*wait_task).node_of_queue);
		// PRINT("After remove wait node\n");
		// ready Queue에서 확인되도록, task의 priority를 set ready
		_os_wakeup_sleeping_task(wait_task);
	} else {
		// PRINT("nothing waiting\n");
		eos_enable_interrupt();
	}
}

void eos_init_condition(eos_condition_t *cond, int32u_t queue_type) {
	/* initialization */
	cond->wait_queue = NULL;
	cond->queue_type = queue_type;
}

void eos_wait_condition(eos_condition_t *cond, eos_semaphore_t *mutex) {
	/* release acquired semaphore */
	eos_release_semaphore(mutex);
	/* wait on condition's wait_queue */
	_os_wait(&cond->wait_queue);
	/* acquire semaphore before return */
	eos_acquire_semaphore(mutex, 0);
}

void eos_notify_condition(eos_condition_t *cond) {
	/* select a task that is waiting on this wait_queue */
	_os_wakeup_single(&cond->wait_queue, cond->queue_type);
}
