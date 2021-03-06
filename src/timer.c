// タイマ関係
#include "bootpack.h"

#define PIT_CTRL    0x0043
#define PIT_CNT0    0x0040

struct TIMER_CONTROL timer_ctl;

void init_pit(void) {
    int i;
    struct TIMER *t;
    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);
    timer_ctl.count = 0;
    for (i = 0; i < MAX_TIMER; i++) {
        timer_ctl.timers[i].flags = 0;   // 未使用状態に初期化
    }
    // 番兵用に1つ利用
    t = timer_alloc();
    t->timeout = 0xffffffff;
    t->flags   = TIMER_FLAGS_USING;
    t->next    = 0; // 一番後ろ
    timer_ctl.timers_head = t;
    timer_ctl.next = 0xffffffff;
    return;
}

struct TIMER *timer_alloc(void) {
    int i;
    for (i = 0; i < MAX_TIMER; i++) {
        if (timer_ctl.timers[i].flags == 0) {
            timer_ctl.timers[i].flags = TIMER_FLAGS_ALLOC;
            timer_ctl.timers[i].flags2 = 0;
            return &timer_ctl.timers[i];
        }
    }
    return 0;   // 空きタイマーが見つからなかった
}

void timer_free(struct TIMER *timer)
{
	timer->flags = 0;  // 未使用状態に戻す
	return;
}

void timer_init(struct TIMER *timer, struct FIFO32 *fifo, int data) {
    timer->fifo = fifo;
    timer->data = data;
    return;
}

void timer_set_time(struct TIMER *timer, unsigned int timeout) {
    int eflags;
    struct TIMER *t, *s;
    timer->timeout = (timeout + timer_ctl.count) & 0x7fffffff;
    timer->flags   = TIMER_FLAGS_USING;
    eflags = io_load_eflags();
    io_cli();

    t = timer_ctl.timers_head;
    // 先頭に入れる場合
    if (timer->timeout <= t->timeout) {
        timer_ctl.timers_head = timer;
        timer->next = t;    // 次はない
        timer_ctl.next = timer->timeout;
        io_store_eflags(eflags);
        return;
    }
    // どこに入れればいいかを探す
    for (;;) {
        s = t;
        t = t->next;
        if (timer->timeout <= t->timeout) {
            s->next = timer;
            timer->next = t;
            io_store_eflags(eflags);
            return;
        }
    }
}

void inthandler20(int *esp) {
    struct TIMER *timer;
    char need_task_switch = 0;
    io_out8(PIC0_OCW2, 0x60);   // IRQ-00受付完了をPICに通知
    timer_ctl.count = (timer_ctl.count + 1) & 0x7fffffff;
    if (timer_ctl.next > timer_ctl.count) {
        return;
    }
    timer = timer_ctl.timers_head;   // とりあえず先頭番地を代入
    for (;;) {
        // timersのタイマはすべて動作中なので、flagsを確認しない
        if (timer_ctl.count != timer->timeout) {
            break;  // まだタイムアウトではない
        }
        timer->flags = TIMER_FLAGS_ALLOC;
        if (timer != task_timer) {
            fifo32_put(timer->fifo, timer->data);
        }
        else {
            need_task_switch = 1;
        }
        timer = timer->next;    // 次のタイマを代入
    }
    // 新しいずらし
    timer_ctl.timers_head = timer;
    timer_ctl.next = timer->timeout;
    if (need_task_switch != 0) {
        task_switch();
    }
    return;
}

int timer_cancel(struct TIMER *timer) {
    int eflags;
    struct TIMER *t;

    eflags = io_load_eflags();
    io_cli();
    if (timer->flags == TIMER_FLAGS_USING) {    // 取り消し処理が必要か？
        if (timer == timer_ctl.timers_head) {
            // 先頭だった場合の取り消し処理
            t = timer->next;
            timer_ctl.timers_head = t;
            timer_ctl.next = t->timeout;
        }
        else {
            // 先頭以外の場合の取り消し処理
            // timerの1つ前を探す
            t = timer_ctl.timers_head;
            for (;;) {
                if (t->next == timer) {
                    break;
                }
                t = t->next;
            }
            // timerの直前のnextがtimerのnextを指すようにする
            t->next = timer->next;
        }
        timer->flags = TIMER_FLAGS_ALLOC;
        io_store_eflags(eflags);
        return 1;   // キャンセル処理成功
    }
    io_store_eflags(eflags);
    return 0;   // キャンセル処理失敗
}

void timer_cancel_all(struct FIFO32 *fifo) {
    int i;
    int eflags;
    struct TIMER *t;

    eflags = io_load_eflags();
    io_cli();
    for (i = 0; i < MAX_TIMER; i++) {
        t = &(timer_ctl.timers[i]);
        if (t->flags != 0 && t->flags2 != 0 && t->fifo == fifo) {
            timer_cancel(t);
            timer_free(t);
        }
    }
    io_store_eflags(eflags);
    return;
}
