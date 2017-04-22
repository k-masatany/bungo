// FIFOライブラリ
#include "bootpack.h"

#define FLAGS_OVERRUN   0x0001

// FIFOバッファの初期化
void fifo32_init(struct FIFO32 *fifo, int size, int *buf) {
    fifo->size  = size;
    fifo->buf   = buf;
    fifo->free  = size;
    fifo->flags = 0;
    fifo->p     = 0;
    fifo->q     = 0;
    return;
}

// FIFOへデータを送り込んで蓄える
int fifo32_put(struct FIFO32 *fifo, int data) {
    if (fifo->free == 0) {
        // 空きがなくてあふれた
        fifo->flags |= FLAGS_OVERRUN;
        return -1;
    }
    fifo->buf[fifo->p] = data;
    fifo->p = (fifo->p + 1) % fifo->size;
    fifo->free--;
    return 0;
}

// FIFOからデータを一つ取ってくる
int fifo32_get(struct FIFO32 *fifo) {
    int data;
    if (fifo->free == fifo->size) {
        // バッファが空の時は、取り合えず-1を返す
        return -1;
    }
    data = fifo->buf[fifo->q];
    fifo->q = (fifo->q + 1) % fifo->size;
    fifo->free++;
    return data;
}

// どれくらいデータが溜まっているかを報告する
int fifo32_status(struct FIFO32 *fifo) {
    return fifo->size - fifo->free;
}
