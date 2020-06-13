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
	eos_disable_interrupt();
	PRINT("sem count is - ");
	printf("%d\n", (*sem).count);
	if ((*sem).count-- > 0){
		PRINT("sem decrement - so, ")
		printf("%d\n", (*sem).count);

		eos_enable_interrupt();
		return 1;
	}
	eos_enable_interrupt();

	if (timeout == -1) {
		(*sem).count++;
		return 0;
	} else {
		while(1){
			eos_tcb_t* cur_task = eos_get_current_task();
			(*cur_task).state = WAITING;
			PRINT("queue type - %d\n", (*sem).queue_type);
			if ((*sem).queue_type == FIFO){
				_os_add_node_tail(&(*sem).wait_queue, &(*cur_task).node_of_queue);
			} else if ((*sem).queue_type == PRIORITY){
				_os_add_node_priority(&(*sem).wait_queue, &(*cur_task).node_of_queue);
			}
			PRINT("AFTER add node\n");

			if (timeout > 0) {
				// timeout 동안만 대기
				// eos_alarm_t alarm;
				// eos_set_alarm(eos_get_system_timer(), &alarm, timeout, _os_wakeup_sleeping_task, cur_task);
				// TODO: 현재의 workload에서는 timeout = 0이라서 이 부분 무관. 이후 체크할 것
				eos_sleep(0); // TODO: eos set alarm대신 이를 사용했는데, 괜찮은지 확인. unset ready부분이 어처피 들어가더라고.
			} else { // timeout == 0
				// 다른 태스크 반환될 때 까지 wait queue에서 대기
				PRINT("timeout 0!\n");
				// TODO: Check Below
				eos_tcb_t * cur_task = eos_get_current_task();
				int32u_t priority = (*cur_task).node_of_queue.priority;
				(*cur_task).state = WAITING;
				_os_node_t * cur_node = &((*cur_task).node_of_queue);
				if (cur_node->next == cur_node-> previous){
					// Thus, if this node is head
					// Oh.. It's a bit wrong. There's no logic for unsetting ready of unhead node
					PRINT("Unset ready\n");
					_os_unset_ready(priority);
				}
				eos_schedule();
			}
			// TODO: 얘가 release에서만 하면 될지 체크.
			// _os_remove_node(&(*sem).wait_queue, &(*cur_task).node_of_queue);

			eos_disable_interrupt();
			// wait하고 돌아오면 다시 체크
			if ((*sem).count > 0){
				// --(*sem).count;
				PRINT("GET Semaphore!\n");
				eos_enable_interrupt();
				return 1;
			}
			eos_enable_interrupt();
		}
	}
}

void eos_release_semaphore(eos_semaphore_t *sem) {
	eos_disable_interrupt();
	if ((*sem).count++ < 0) {
		eos_enable_interrupt();
		// wait queue에서 해당 task를 제거
		_os_node_t* wait_queue = (_os_node_t*)(*sem).wait_queue;
		eos_tcb_t* wait_task = (eos_tcb_t*)(*wait_queue).ptr_data;
		_os_remove_node(&wait_queue, &(*wait_task).node_of_queue);

		// ready Queue에서 확인되도록, task의 priority를 set ready
		(*wait_task).state = READY;
		(*wait_task).start_time = eos_get_system_timer()->tick;
		int32u_t priority = (wait_task -> node_of_queue).priority;
		_os_node_t * _os_ready_queue = _get_os_ready_queue();
		_os_add_node_tail(&_os_ready_queue[priority], &(*wait_task).node_of_queue);
		_os_set_ready(priority);
		PRINT("Made the priority ready %d\n", priority);
	} else {
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
