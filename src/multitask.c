// マルチタスク関係
#include "bootpack.h"

struct TASK_CONTROL *task_ctl;
struct TIMER *task_timer;

struct TASK *task_now(void) {
    struct TASK_LEVEL *task_lv = &task_ctl->levels[task_ctl->now_level];
    return task_lv->tasks_head[task_lv->now];
}

void task_add(struct TASK *task) {
    struct TASK_LEVEL *task_lv = &task_ctl->levels[task_ctl->now_level];
    task_lv->tasks_head[task_lv->running] = task;
    task_lv->running++;
    task->flags = 2;
    return;
}

void task_remove(struct TASK *task) {
    int i;
    struct TASK_LEVEL *task_lv = &task_ctl->levels[task_ctl->now_level];

    // taskがどこにいるかを探す
    for (i = 0; i < task_lv->running; i++) {
        if (task_lv->tasks_head[i] == task) {
            break;
        }
    }

    task_lv->running--;
    if (i < task_lv->now) {
        task_lv->now--; // ずれるので修正
    }
    if (task_lv->now >= task_lv->running) {
        task_lv->now = 0; // nowがおかしな値になっていたら修正
    }
    task->flags = 1; // スリープ

    // ずらし
    for (; i < task_lv->running; i++) {
        task_lv->tasks_head[i] = task_lv->tasks_head[i+1];
    }

    return;
}

void task_switch_sub(void) {
    int i;
    // 一番上のLvを探す
    for (i = 0; i < MAX_TASKLEVELS; i++) {
        if (task_ctl->levels[i].running > 0) {
            break;
        }
    }
    task_ctl->now_level = i;
    task_ctl->need_lv_change = 0;
    return;
}

struct TASK  *task_init(struct MEMORY_MANAGER *memory_manager) {
    int i;
    struct TASK *task;
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;
    task_ctl = (struct TASK_CONTROL *) memman_alloc_4k(memory_manager, sizeof(struct TASK_CONTROL));
    for (i = 0; i < MAX_TASKS; i++) {
        task_ctl->tasks[i].flags = 0;
        task_ctl->tasks[i].selector = (TASK_GDT0 + i) * 8;
        set_segment_descriptor(gdt + TASK_GDT0 + i, 103, (int) &task_ctl->tasks[i].tss, AR_TSS32);
    }
    for (i = 0; i < MAX_TASKLEVELS; i++) {
        task_ctl->levels[i].running = 0;
        task_ctl->levels[i].now = 0;
    }
    task = task_alloc();
    task->flags = 2;    // 動作中マーク
    task->priority = 2; // 0.02[s]
    task->level = 0; // 優先度最高
    task_add(task);
    task_switch_sub();   // Lv設定
    load_tr(task->selector);
    task_timer = timer_alloc();
    timer_set_time(task_timer, task->priority);
    return task;
}

struct TASK *task_alloc(void) {
    int i;
    struct TASK *task;

    for (i = 0; i < MAX_TASKS; i++) {
        if (task_ctl->tasks[i].flags == 0) {
            task = &task_ctl->tasks[i];
            task->flags = 1;                // 使用中マーク
            task->tss.eflags = 0x00000202;  // IF = 1;
            task->tss.eax    = 0;           // とりあえず0にしておく
            task->tss.ecx    = 0;
            task->tss.edx    = 0;
            task->tss.ebx    = 0;
            task->tss.ebp    = 0;
            task->tss.esi    = 0;
            task->tss.edi    = 0;
            task->tss.es     = 0;
            task->tss.ds     = 0;
            task->tss.fs     = 0;
            task->tss.gs     = 0;
            task->tss.ldtr   = 0;
            task->tss.iomap  = 0x40000000;
            return task;
        }
    }
    return 0;   // 全部使用中
}

void task_run(struct TASK *task, int level, int priority) {
    if (level < 0) {
        level = task->level;    // Lvを変更しない
    }
    if (priority > 0) {
        task->priority = priority;
    }
    if (task->flags == 2 && task->level != level) { // 動作中のLvの変更
        task_remove(task);
    }
    if (task->flags != 2) { // スリープから起こされる場合
        task->level = level;
        task_add(task);
    }

    task_ctl->need_lv_change = 1;   // 次回タスク切り替え時にLvを変更する
    return;
}

void task_switch(void) {
    struct TASK_LEVEL *task_lv = &task_ctl->levels[task_ctl->now_level];
    struct TASK *now_task = task_lv->tasks_head[task_lv->now];
    struct TASK *new_task;

    task_lv->now++;
    if (task_lv->now == task_lv->running) {
        task_lv->now = 0;
    }
    if (task_ctl->need_lv_change != 0) {
        task_switch_sub();
        task_lv = &task_ctl->levels[task_ctl->now_level];
    }
    new_task = task_lv->tasks_head[task_lv->now];
    timer_set_time(task_timer, new_task->priority);
    if (new_task != now_task) {
        farjmp(0, new_task->selector);
    }
    return;
}

void task_sleep(struct TASK *task) {
    struct TASK *now_task;
    if (task->flags == 2) { // 指定タスクが稼働中であれば
        now_task = task_now();
        task_remove(task);  // これを実行するとflagsが1になる
        if (task == now_task) {
            // 自分自身をSleepするので、後でタスクスイッチ
            task_switch_sub();
            now_task = task_now();  // 設定値での「現在のタスク」を教えてもらう
            farjmp(0, now_task->selector);
        }
    }
    return;
}
