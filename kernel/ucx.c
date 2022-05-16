/* file:          ucx.c
 * description:   UCX/OS kernel
 * date:          04/2021
 * author:        Sergio Johann Filho <sergio.johann@acad.pucrs.br>
 */

#include <ucx.h>

struct kcb_s kernel_state;
struct kcb_s *kcb_p = &kernel_state;
uint16_t task_count = 0;
uint32_t dispatch_count = 0;

/* kernel auxiliary functions */

static void krnl_guard_check(void)
{
	uint32_t check = 0x33333333;
	
	if (*kcb_p->tcb_p->guard_addr != check) {
		ucx_hexdump((void *)kcb_p->tcb_p->guard_addr, kcb_p->tcb_p->guard_sz);
		printf("\n*** HALT - task %d, guard %08x (%d) check failed\n", kcb_p->tcb_p->id,
			(size_t)kcb_p->tcb_p->guard_addr, (size_t)kcb_p->tcb_p->guard_sz);
		for (;;);
	}
		
}

static void krnl_delay_update(void)
{
	struct tcb_s *tcb_ptr = kcb_p->tcb_first;
	
	for (;;	tcb_ptr = tcb_ptr->tcb_next) {
		if (tcb_ptr->state == TASK_BLOCKED && tcb_ptr->delay > 0) {
			tcb_ptr->delay--;
			if (tcb_ptr->delay == 0)
				tcb_ptr->state = TASK_READY;
		}
		if (tcb_ptr->tcb_next == kcb_p->tcb_first) break;
	}
}

static void krnl_sched_init(int32_t preemptive)
{
	kcb_p->tcb_p = kcb_p->tcb_first;
	if (preemptive) {
		_timer_enable();
	}
	(*kcb_p->tcb_p->task)();
}


/* task scheduler and dispatcher */

uint16_t krnl_schedule(void)
{
	do {
		do {
			kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;
		} while (kcb_p->tcb_p->state != TASK_READY || kcb_p->tcb_p->is_periodic);
	} while (--kcb_p->tcb_p->priority & 0xff);
	kcb_p->tcb_p->priority |= (kcb_p->tcb_p->priority >> 8) & 0xff;

	return kcb_p->tcb_p->id;
}

// Set current task to READY
// Consume current task's capacity if it is periodic
// Tick period and deadline for all
// For tasks with remaining period <= 0
//      reset remaining period
//      reset remaining capacity
//      reset remaining deadline
// For tasks with deadline <= 0
//      set capacity = 0
// Find periodic task with remaining capacity and earliest deadline
// If found, select for execution
// If not
//      find non-periodic with the highest priority
//      select for execution

void tick_period_and_deadline() {
	for(uint16_t i = 0; i < task_count; i++) {
		kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;

		if(!kcb_p->tcb_p->is_periodic) {
			continue;
		}

		kcb_p->tcb_p->remaining_period_ticks--;
		kcb_p->tcb_p->remaining_deadline_ticks--;
	}
}

void handle_period_resets() {
	for(uint16_t i = 0; i < task_count; i++) {
		kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;

		if(!kcb_p->tcb_p->is_periodic) {
			continue;
		}

		if(kcb_p->tcb_p->remaining_period_ticks <= 0) {
			kcb_p->tcb_p->remaining_period_ticks = kcb_p->tcb_p->period;
			kcb_p->tcb_p->remaining_deadline_ticks = kcb_p->tcb_p->deadline;
			kcb_p->tcb_p->remaining_capacity_ticks = kcb_p->tcb_p->capacity;
		}
	}
}

void drop_tasks_with_missed_deadlines() {
	for(uint16_t i = 0; i < task_count; i++) {
		kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;

		if(!kcb_p->tcb_p->is_periodic) {
			continue;
		}

		if(kcb_p->tcb_p->remaining_capacity_ticks <= 0) {
			continue;
		}

		if(kcb_p->tcb_p->remaining_deadline_ticks <= 0) {
			printf("dm:%d\n", kcb_p->tcb_p->id);
			kcb_p->tcb_p->remaining_capacity_ticks = 0;
			kcb_p->deadline_misses++;
		}
	}
}

uint16_t find_next_periodic_task(struct tcb_s *last_task) {
	kcb_p->tcb_p = last_task;

	uint16_t earliest_deadline = 0xffff;
	uint16_t task_id = -1;

	for(uint16_t i = 0; i < task_count; i++) {
		kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;

		if(!kcb_p->tcb_p->is_periodic) {
			continue;
		}

		if(kcb_p->tcb_p->remaining_capacity_ticks <= 0) {
			continue;
		}

		if(kcb_p->tcb_p->remaining_deadline_ticks < earliest_deadline) {
			earliest_deadline = kcb_p->tcb_p->remaining_deadline_ticks;
			task_id = kcb_p->tcb_p->id;
		}
	}

	return task_id;
}

void print_report() {
	uint16_t tasks_run = 0;
	for(uint16_t i = 0; i < task_count; i++) {
		kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;

		if (kcb_p->tcb_p->has_run_in_lcm) {
			tasks_run++;
		}
	}

	printf("=================================================\n");
	printf("Report:\n");
	printf("Deadline misses: %d\n", kcb_p->deadline_misses);
	printf("Jobs run: %d\n", tasks_run);
	printf("=================================================\n");
}

void run_statistics_stuff() {
	if(kcb_p->ticks_until_next_report <= 0) {
		print_report();

		kcb_p->ticks_until_next_report = kcb_p->periods_least_common_multiple;
		kcb_p->deadline_misses = 0;
		for(uint16_t i = 0; i < task_count; i++) {
			kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;
			kcb_p->tcb_p->has_run_in_lcm = 0;
		}
	}
	kcb_p->ticks_until_next_report--;
}

uint16_t krnl_rt_schedule() {
	struct tcb_s *preempted_task = kcb_p->tcb_p;

#ifndef SCHEDULER_DEBUG
	run_statistics_stuff();
#endif

	if (kcb_p->tcb_p->state == TASK_RUNNING) {
		kcb_p->tcb_p->state = TASK_READY;

		if (kcb_p->tcb_p->is_periodic) {
			kcb_p->tcb_p->remaining_capacity_ticks--;
		}
	}

	tick_period_and_deadline();
	handle_period_resets();
	drop_tasks_with_missed_deadlines();

	uint16_t next_task_id = find_next_periodic_task(preempted_task);
	if(next_task_id == (uint16_t)-1) {
		next_task_id = krnl_schedule();
	}

	for(uint16_t i = 0; i < task_count; i++) {
		kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;

		if (kcb_p->tcb_p->id == next_task_id) {
			break;
		}
	}

	kcb_p->tcb_p->state = TASK_RUNNING;
	kcb_p->tcb_p->has_run_in_lcm = 1;
	kcb_p->ctx_switches++;

#ifdef SCHEDULER_DEBUG
	printf("%d\n", next_task_id);
#endif
	return next_task_id;
}

void krnl_dispatcher(void)
{
//    printf("|%d|", dispatch_count++);
	if (!setjmp(kcb_p->tcb_p->context)) {
		krnl_delay_update();
		krnl_guard_check();
		krnl_rt_schedule();
		_interrupt_tick();
		longjmp(kcb_p->tcb_p->context, 1);
	}
}


/* task management API */

int32_t ucx_task_add(void *task, uint16_t guard_size)
{
	struct tcb_s *tcb_last = kcb_p->tcb_p;
	
	kcb_p->tcb_p = (struct tcb_s *)malloc(sizeof(struct tcb_s));
	if (kcb_p->tcb_first == 0)
		kcb_p->tcb_first = kcb_p->tcb_p;

	if (!kcb_p->tcb_p)
		return -1;

	if (tcb_last)
		tcb_last->tcb_next = kcb_p->tcb_p;
	kcb_p->tcb_p->tcb_next = kcb_p->tcb_first;
	kcb_p->tcb_p->task = task;
	kcb_p->tcb_p->delay = 0;
	kcb_p->tcb_p->guard_sz = guard_size;
	kcb_p->tcb_p->id = kcb_p->id++;
	kcb_p->tcb_p->state = TASK_STOPPED;
	kcb_p->tcb_p->priority = TASK_NORMAL_PRIO;
	kcb_p->tcb_p->is_periodic = 0;
	kcb_p->tcb_p->has_run_in_lcm = 0;


	task_count++;
	
	return 0;
}

int32_t ucx_task_add_periodic(void *task, uint16_t period, uint16_t capacity, uint16_t deadline, uint16_t guard_size) {
	if(ucx_task_add(task, guard_size)) {
		return -1;
	}

	kcb_p->tcb_p->period = period;
	kcb_p->tcb_p->remaining_period_ticks = period;
	kcb_p->tcb_p->capacity = capacity;
	kcb_p->tcb_p->remaining_capacity_ticks = capacity;
	kcb_p->tcb_p->deadline = deadline;
	kcb_p->tcb_p->remaining_deadline_ticks = deadline;
	kcb_p->tcb_p->is_periodic = 1;

	return 0;
}

/*
 * First following lines of code are absurd at best. Stack marks are
 * used by krnl_guard_check() to detect stack overflows on guard space.
 * It is up to the user to define sufficient stack guard space (considering
 * local thread allocation of the stack for recursion and context
 * saving). Stack allocated for data before ucx_task_init() (generally
 * most stack used by a task) is not verified.
 * We also need the safety pig, just in case.
						 _
 _._ _..._ .-',     _.._(`)) 
'-. `     '  /-._.-'    ',/ 
   )         \            '. 
  / _    _    |             \ 
 |  a    a    /              | 
 \   .-.                     ;   
  '-('' ).-'       ,'       ; 
	 '-;           |      .'
		\           \    /
		| 7  .__  _.-\   \
		| |  |  ``/  /`  /
	   /,_|  |   /,_/   /
		  /,_/      '`-'
*/
void ucx_task_init(void)
{
	char guard[kcb_p->tcb_p->guard_sz];
	
	memset(guard, 0x69, kcb_p->tcb_p->guard_sz);
	memset(guard, 0x33, 4);
	memset((guard) + kcb_p->tcb_p->guard_sz - 4, 0x33, 4);
	kcb_p->tcb_p->guard_addr = (uint32_t *)guard;
	//printf("task %d, guard: %08x - %08x\n", kcb_p->tcb_p->id, (size_t)kcb_p->tcb_p->guard_addr,
	//	(size_t)kcb_p->tcb_p->guard_addr + kcb_p->tcb_p->guard_sz);
	
	if (!setjmp(kcb_p->tcb_p->context)) {
		kcb_p->tcb_p->state = TASK_READY;
		if (kcb_p->tcb_p->tcb_next == kcb_p->tcb_first) {
			kcb_p->tcb_p->state = TASK_RUNNING;
		} else {
			kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;
			kcb_p->tcb_p->state = TASK_RUNNING;
			(*kcb_p->tcb_p->task)();
		}
	}
	_ei(1);
}

void ucx_task_yield()
{
	if (!setjmp(kcb_p->tcb_p->context)) {
		krnl_delay_update();		/* TODO: check if we need to run a delay update on yields. maybe only on a non-preemtive execution? */ 
		krnl_guard_check();
		krnl_schedule();
		longjmp(kcb_p->tcb_p->context, 1);
	}
}

void ucx_task_delay(uint16_t ticks)
{
	kcb_p->tcb_p->delay = ticks;
	kcb_p->tcb_p->state = TASK_BLOCKED;
	ucx_task_yield();
}

int32_t ucx_task_suspend(uint16_t id)
{
	struct tcb_s *tcb_ptr = kcb_p->tcb_first;
	
	for (;; tcb_ptr = tcb_ptr->tcb_next) {
		if (tcb_ptr->id == id) {
			ucx_critical_enter();
			if (tcb_ptr->state == TASK_READY || tcb_ptr->state == TASK_RUNNING) {
				tcb_ptr->state = TASK_SUSPENDED;
				ucx_critical_leave();
				break;
			} else {
				ucx_critical_leave();
				return -1;
			}
		}
		if (tcb_ptr->tcb_next == kcb_p->tcb_first)
			return -1;
	}
	if (kcb_p->tcb_p->id == id)
		ucx_task_yield();
	
	return 0;
}

int32_t ucx_task_resume(uint16_t id)
{
	struct tcb_s *tcb_ptr = kcb_p->tcb_first;
	
	for (;; tcb_ptr = tcb_ptr->tcb_next) {
		if (tcb_ptr->id == id) {
			ucx_critical_enter();
			if (tcb_ptr->state == TASK_SUSPENDED) {
				tcb_ptr->state = TASK_READY;
				ucx_critical_leave();
				break;
			} else {
				ucx_critical_leave();
				return -1;
			}
		}	
		if (tcb_ptr->tcb_next == kcb_p->tcb_first)
			return -1;
	}
	if (kcb_p->tcb_p->id == id)
		ucx_task_yield();
	
	return 0;
}

int32_t ucx_task_priority(uint16_t id, uint16_t priority)
{
	struct tcb_s *tcb_ptr = kcb_p->tcb_first;

	switch (priority) {
	case TASK_CRIT_PRIO:
	case TASK_HIGH_PRIO:
	case TASK_NORMAL_PRIO:
	case TASK_LOW_PRIO:
	case TASK_IDLE_PRIO:
		break;
	default:
		return -1;
	}
	
	for (;; tcb_ptr = tcb_ptr->tcb_next) {
		if (tcb_ptr->id == id) {
			tcb_ptr->priority = priority;
			break;
		}
		if (tcb_ptr->tcb_next == kcb_p->tcb_first)
			return -1;
	}
	
	return 0;
}

uint16_t ucx_task_id()
{
	return kcb_p->tcb_p->id;
}

void ucx_task_wfi()
{
	volatile uint32_t s;
	
	s = kcb_p->ctx_switches;
	while (s == kcb_p->ctx_switches);
}

uint16_t ucx_task_count()
{
	return task_count;
}

void ucx_critical_enter()
{
	_timer_disable();
}

void ucx_critical_leave()
{
	_timer_enable();
}


/* main() function, called from the C runtime */

void idle(void) {
	ucx_task_init();
	for(;;){}
}

void calculate_periods_lcm() {

	uint16_t longest_period = 0;
	for(uint16_t i = 0; i < task_count; i++) {
		kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;

		if(!kcb_p->tcb_p->is_periodic) {
			continue;
		}

		if(kcb_p->tcb_p->period > longest_period) {
			longest_period = kcb_p->tcb_p->period;
		}
	}

	uint16_t lcm = longest_period;
	uint8_t found_lcm = 1;
	do {
		found_lcm = 1;
		for(uint16_t i = 0; i < task_count; i++) {
			kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;

			if(!kcb_p->tcb_p->is_periodic) {
				continue;
			}

			if(lcm % kcb_p->tcb_p->period != 0) {
				found_lcm = 0;
				lcm++;
			}
		}
	} while(!found_lcm);

	kcb_p->periods_least_common_multiple = lcm;
}

int32_t main(void)
{
	int32_t pr;
	
	_hardware_init();
	
	kcb_p->tcb_p = 0;
	kcb_p->tcb_first = 0;
	kcb_p->ctx_switches = 0;
	kcb_p->id = 0;
	
	printf("UCX/OS boot on %s\n", __ARCH__);
#ifndef UCX_OS_HEAP_SIZE
	ucx_heap_init((size_t *)&_heap_start, (size_t)&_heap_size);
	printf("heap_init(), %d bytes free\n", (size_t)&_heap_size);
#else
	ucx_heap_init((size_t *)&_heap, UCX_OS_HEAP_SIZE);
	printf("heap_init(), %d bytes free\n", UCX_OS_HEAP_SIZE);
#endif
	printf("x\n");


	pr = app_main();

	uint8_t has_aperiodic = 0;
	for(uint16_t i = 0; i < task_count; i++) {
		if(!kcb_p->tcb_p->is_periodic) {
			has_aperiodic = 1;
		}

		kcb_p->tcb_p = kcb_p->tcb_p->tcb_next;
	}

	if(!has_aperiodic) {
		ucx_task_add(idle, DEFAULT_GUARD_SIZE);
		ucx_task_priority(kcb_p->tcb_p->id, TASK_IDLE_PRIO);
	}

	calculate_periods_lcm();
	kcb_p->ticks_until_next_report = kcb_p->periods_least_common_multiple;

	krnl_sched_init(pr);
	
	return 0;
}
