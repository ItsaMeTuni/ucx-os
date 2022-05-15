#include <ucx.h>

void task1(void)
{
    int32_t cnt = 0;

    ucx_task_init();

    while (1) {
//        printf("[task %d %ld]\n", ucx_task_id(), cnt++);
        _delay_ms(10);
    }
}

void task0(void)
{
    int32_t cnt = 0;

    ucx_task_init();

    while (1) {
//        printf("[task %d %ld]\n", ucx_task_id(), cnt++);
        _delay_ms(10);
    }
}

void task_aperiodic() {
    ucx_task_init();

    while (1) {
//        printf("[task %d %ld]\n", ucx_task_id(), cnt++);
        _delay_ms(10);
    }
}

int32_t app_main(void)
{
    ucx_task_add_periodic(task0, 100, 30, 100, DEFAULT_GUARD_SIZE);
    ucx_task_add_periodic(task1, 100, 30, 100, DEFAULT_GUARD_SIZE);
    ucx_task_add(task_aperiodic, DEFAULT_GUARD_SIZE);
    ucx_task_add(task_aperiodic, DEFAULT_GUARD_SIZE);

    // start UCX/OS, preemptive mode
    return 1;
}
