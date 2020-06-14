/********************************************************
 * Filename: core/timer.c
 *
 * Author: wsyoo, RTOSLab. SNU.
 * 
 * Description: 
 ********************************************************/
#include <core/eos.h>

static eos_counter_t system_timer;

int8u_t eos_init_counter(eos_counter_t *counter, int32u_t init_value) {
	counter->tick = init_value;
	counter->alarm_queue = NULL;
	return 0;
}

void eos_set_alarm(eos_counter_t* counter, eos_alarm_t* alarm, int32u_t timeout, void (*entry)(void *arg), void *arg) {
	// PRINT("In set alarm\n");
	_os_remove_node(&(*counter).alarm_queue, &(*alarm).alarm_queue_node); // remove alarm from alarm queue
	// PRINT("done remove node\n");
	if (timeout == 0 || entry == NULL) {
		return;
	}
	// PRINT("Set alarm timeout - %d\n", timeout);
	(*alarm).timeout = timeout;
	(*alarm).handler = entry;
	(*alarm).arg = arg;
	(*alarm).alarm_queue_node.ptr_data = alarm;
	(*alarm).alarm_queue_node.priority = timeout; // use timeout as priority
	_os_add_node_priority(&(*counter).alarm_queue, &(*alarm).alarm_queue_node);
}

eos_counter_t* eos_get_system_timer() {
	return &system_timer;
}

void eos_trigger_counter(eos_counter_t* counter) {
	(*counter).tick++;
	PRINT("tick - %d\n", (*counter).tick);
	while (1) {
		_os_node_t * alarm_queue = (counter -> alarm_queue);
		// PRINT("is alarm queue null?\n");
		if (alarm_queue != NULL){
			// PRINT("Not null\n");

			// If alarm queue is not empty,
			eos_alarm_t* cur_alarm = (alarm_queue -> ptr_data);
			// PRINT("Smallest timeout is - %d\n", cur_alarm->timeout);
			if ((cur_alarm -> timeout) == (*counter).tick){
				// Make all timeout task READY
				// PRINT("TIMEOUT!\n");
				eos_set_alarm(counter, cur_alarm, 0, NULL, NULL); // remove alarm from alarm queue
				(cur_alarm -> handler)(cur_alarm -> arg); // call _os_wakeup_sleeping_task
			} else break;
		} else break;
	}
	eos_schedule();
}

/* Timer interrupt handler */
static void timer_interrupt_handler(int8s_t irqnum, void *arg) {
	/* trigger alarms */
	eos_trigger_counter(&system_timer);
}

void _os_init_timer() {
	eos_init_counter(&system_timer, 0);

	/* register timer interrupt handler */
	eos_set_interrupt_handler(IRQ_INTERVAL_TIMER0, timer_interrupt_handler, NULL);
}
