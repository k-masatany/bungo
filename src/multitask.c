// マルチタスク処理関係
#include "bootpack.h"

struct TIMER *tasks_timer;
int tasks_tr;

void tasks_init(void) {
    tasks_timer = timer_alloc();
    // timer_initは必要ないので省略
    timer_set_time(tasks_timer, 2);
    tasks_tr = 3 * 8;
    return;
}

void tasks_taskswitch(void) {
    if (tasks_tr == 3 * 8) {
        tasks_tr = 4 * 8;
    }
    else {
        tasks_tr = 3 * 8;
    }
    timer_set_time(tasks_timer, 2);
    farjmp(0, tasks_tr);
    return;
}
