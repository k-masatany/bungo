// タイマ関係
#include "bootpack.h"

#define PIT_CTRL    0x0043
#define PIT_CNT0    0x0040

struct TIMER_CONTROL timer_ctl;

#define TIMER_FLAGS_ALLOC   1   // 確保した状態
#define TIMER_FLAGS_USING   2   // タイマ作動中


void init_pit(void) {
    int i;
    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);
    timer_ctl.count = 0;
    timer_ctl.next  = 0xffffffff;
    timer_ctl.using = 0;
    for (i = 0; i < MAX_TIMER; i++) {
        timer_ctl.timers[i].flags = 0;   // 未使用状態に初期化
    }
    return;
}

struct TIMER *timer_alloc(void) {
    int i;
    for (i = 0; i < MAX_TIMER; i++) {
        if (timer_ctl.timers[i].flags == 0) {
            timer_ctl.timers[i].flags = TIMER_FLAGS_ALLOC;
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

void timer_init(struct TIMER *timer, struct FIFO8 *fifo, unsigned char data) {
    timer->fifo = fifo;
    timer->data = data;
    return;
}

void timer_set_time(struct TIMER *timer, unsigned int timeout) {
    int eflags, i, j;
    timer->timeout = timeout + timer_ctl.count;
    timer->flags   = TIMER_FLAGS_USING;
    eflags = io_load_eflags();
    io_cli();
    // どこに入れればいいかを探す
    for (i = 0; i < timer_ctl.using; i++) {
        if (timer_ctl.timers_head[i]->timeout >= timer->timeout) {
            break;
        }
    }
    // 後ろをずらす
    for (j = timer_ctl.using; j > i; j--) {
        timer_ctl.timers_head[j] = timer_ctl.timers_head[j-1];
    }
    timer_ctl.using++;
    // 空いた隙間に入れる
    timer_ctl.timers_head[i] = timer;
    timer_ctl.next = timer_ctl.timers_head[0]->timeout;
    io_store_eflags(eflags);
    return;
}

void inthandler20(int *esp) {
    int i, j;
    io_out8(PIC0_OCW2, 0x60);   // IRQ-00受付完了をPICに通知
    timer_ctl.count++;
    if (timer_ctl.next > timer_ctl.count) {
        return;
    }
    for (i = 0; i < timer_ctl.using; i++) {
        // timersのタイマはすべて動作中なので、flagsを確認しない
        if (timer_ctl.timers_head[i]->timeout > timer_ctl.count) {
            break;  // まだタイムアウトではない
        }
        else {  // タイムアウト
            timer_ctl.timers_head[i]->flags = TIMER_FLAGS_ALLOC;
            fifo8_put(timer_ctl.timers_head[i]->fifo, timer_ctl.timers_head[i]->data);
        }
    }
    // 1個タイマーがタイムアウトしたので、残りをずらす
    timer_ctl.using -= i;
    for (j = 0; j < timer_ctl.using; j++) {
        timer_ctl.timers_head[j] = timer_ctl.timers_head[i+j];
    }
    if (timer_ctl.using > 0) {
        timer_ctl.next = timer_ctl.timers_head[0]->timeout;
    }
    else {
        timer_ctl.next = 0xffffffff;
    }
    return;
}
