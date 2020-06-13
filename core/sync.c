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
	// PRINT("sem count is - ");
	// printf("%d\n", (*sem).count);
	if ((*sem).count-- > 0){
		// PRINT("sem decrement - so, %d\n", (*sem).count);
		eos_enable_interrupt();
		return 1;
	}
	// PRINT("sem decrement - so, %d\n", (*sem).count);
	eos_enable_interrupt();

	if (timeout == -1) {
		(*sem).count++;
		// PRINT("sem increment - so, %d\n", (*sem).count);
		return 0;
	} 

	while(1){
		eos_tcb_t* cur_task = eos_get_current_task();
		(*cur_task).state = WAITING;
		// PRINT("queue type - %d\n", (*sem).queue_type);
		if ((*sem).queue_type == FIFO){
			_os_add_node_tail(&(*sem).wait_queue, &(*cur_task).node_of_queue);
		} else if ((*sem).queue_type == PRIORITY){
			_os_add_node_priority(&(*sem).wait_queue, &(*cur_task).node_of_queue);
		}
		if (timeout > 0) {
			// timeout 동안만 대기
			eos_sleep(0); // TODO: eos set alarm대신 이를 사용했는데, 괜찮은지 확인. unset ready부분이 어처피 들어가더라고.
		} else { // timeout == 0
			// 다른 태스크 반환될 때 까지 wait queue에서 대기
			eos_schedule();
		}
		eos_disable_interrupt();
		// wait하고 돌아오면 다시 체크
		// PRINT("sem count %d\n", (*sem).count);
		if ((*sem).count >= 0){
			// PRINT("GET Semaphore!\n");
			eos_enable_interrupt();
			return 1;
		}
		eos_enable_interrupt();
	}
}

void eos_release_semaphore(eos_semaphore_t *sem) {
	eos_disable_interrupt();
	// PRINT("Befo add sem count - %d\n", (*sem).count);
	// (*sem).count++;
	// if ((_os_node_t*)(*sem).wait_queue != NULL) {
	if ((*sem).count++ < 0) {
		// PRINT("After add sem count - %d\n", (*sem).count);
		eos_enable_interrupt();
		// wait queue에서 해당 task를 제거
		_os_node_t* wait_queue = (_os_node_t*)(*sem).wait_queue;
		eos_tcb_t* wait_task = (eos_tcb_t*)(*wait_queue).ptr_data;
		PRINT("\n");
		for(int i = 0; i < 5; i++){
			printf("%d ", (wait_task -> node_of_queue).priority);
		}
		printf("\n");
		_os_remove_node(&sem->wait_queue, &(*wait_task).node_of_queue);

		// ready Queue에서 확인되도록, task의 priority를 set ready
		// PRINT("Befo Wakeup\n");
		_os_wakeup_sleeping_task(wait_task);
		// PRINT("After Wakeup\n");
		// PRINT("Made the priority ready %d\n", priority);
	} else {
		// PRINT("After add sem count - %d\n", (*sem).count);
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
