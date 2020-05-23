#include <core/eos.h>

/* mapping table */
int8u_t const _os_map_table[] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
int8u_t const _os_unmap_table[] = {
    0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
	4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0
};

int8u_t _os_lock_scheduler() {
	int32u_t flag = eos_disable_interrupt();
	int8u_t temp = _os_scheduler_lock;
	_os_scheduler_lock = LOCKED;
	eos_restore_interrupt(flag);
	return temp;
}

void _os_restore_scheduler(int8u_t scheduler_state) {
	int32u_t flag = eos_disable_interrupt();
	_os_scheduler_lock = scheduler_state;
	eos_restore_interrupt(flag);
	eos_schedule();
}

void _os_init_scheduler() {
	PRINT("initializing scheduler module.\n");
	
	/* initialize ready_group */
	_os_ready_group = 0;

	/* initialize ready_table */
	int8u_t i;
	for (i=0; i<READY_TABLE_SIZE; i++) {
		_os_ready_table[i] = 0;
	}

	/* initialize scheduler lock */
	_os_scheduler_lock = UNLOCKED;
}

int32u_t _os_get_highest_priority() {
	/****
	 * un map table은 8bit의 ready group을 넣으면, 가장 하위의 비트의 index를 리턴
	 * 즉, 가장 높은 우선순위의 인덱스를 줌
	 * y는 최우선순위 group번호가 되는 거고,
	 * 그 아래줄에서 다시, readytable[y]를 통해 그 그룹 내의 우선순위를 나타내는 8비트 숫자가 리턴되며
	 * 그 숫자를 다시 unmap table에 넣어서 그 그룹의 priority중 가장 우선순위 낮은 index를 리턴한다.
	 * y << 3 는 이제 우선순위그룹이 원래 priority숫자에서 어느정도였는지 나타내며,(가령, 3번쨰 그룹이면 8x3=24가 3그룹의 첫 priority숫자)
	 * y <<3에 아까 구한 index를 더하면 원래 priority가 된다.
	 **/
	int8u_t y = _os_unmap_table[_os_ready_group];
	return (int32u_t)((y << 3) + _os_unmap_table[_os_ready_table[y]]);
}

void _os_set_ready(int8u_t priority) {
	/****
	 * 동작 방식은 다음과 같다.
	 * priority는 0-63까지 있다. 이를 8그룹으로 나눠서 본다(os ready group 8bit)
	 * priority >> 3에 의해 이 priority가 어떤 그룹이냐가 나옴.
	 * _os_map_table[그룹번호] 하면, 그 그룹번호를 나타내는 one hot vector가 나옴 
	 * 이를 os ready group에 or 연산. 즉, os ready group은 준비가된 ready group을 각 bit로 나타냄
	 * 그 밑줄에 priority & 0x07은 하위3비트를 보며 group내의 우선순위를 나타냄
	 * os ready table은 8개 element가 있고, 각각 한 group을 나타내며, 8bit 인트가 그 그룹 내의 ready bit를 나타냄
	 * 
	 **/
	/* set corresponding bit of ready_group to 1 */
	_os_ready_group |= _os_map_table[priority >> 3];

	/* set corresponding bit of ready_table to 1 */
	_os_ready_table[priority >> 3] |= _os_map_table[priority & 0x07];
}

void _os_unset_ready(int8u_t priority) {
	/* set corresponding bit of ready_table to 0 */
	if ((_os_ready_table[priority >> 3] &= ~_os_map_table[priority & 0x07]) == 0) {
		/* if no ready task exists in the priority group, set corresponding bit of ready_group to 0 */
		_os_ready_group &= ~_os_map_table[priority >> 3];
	}
}
