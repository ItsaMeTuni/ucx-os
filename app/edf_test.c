#include <ucx.h>

void task(void)
{
    int32_t cnt = 0;

    ucx_task_init();

    while (1) {
        _delay_ms(10);
    }
}

int32_t app_main(void)
{
    // Task group 0
//    ucx_task_add_periodic(task, 100, 30, 90, DEFAULT_GUARD_SIZE);
//    ucx_task_add_periodic(task, 200, 30, 160, DEFAULT_GUARD_SIZE);
//    ucx_task_add_periodic(task, 100, 40, 70, DEFAULT_GUARD_SIZE);

    // Task group 1
//    ucx_task_add_periodic(task, 100, 50, 90, DEFAULT_GUARD_SIZE);
//    ucx_task_add_periodic(task, 200, 60, 160, DEFAULT_GUARD_SIZE);
//    ucx_task_add(task, DEFAULT_GUARD_SIZE);
//    ucx_task_add(task, DEFAULT_GUARD_SIZE);
//    ucx_task_add(task, DEFAULT_GUARD_SIZE);


    // Task group 2
    ucx_task_add_periodic(task, 120, 20, 90, DEFAULT_GUARD_SIZE);
    ucx_task_add_periodic(task, 200, 40, 60, DEFAULT_GUARD_SIZE);
    ucx_task_add_periodic(task, 100, 20, 80, DEFAULT_GUARD_SIZE);
    ucx_task_add_periodic(task, 200, 30, 140, DEFAULT_GUARD_SIZE);
    ucx_task_add_periodic(task, 100, 10, 100, DEFAULT_GUARD_SIZE);

    // start UCX/OS, preemptive mode
    return 1;
}
