// キーボード関係
#include "bootpack.h"

struct FIFO32 *keyboard_fifo;
int keyboard_data_base;

void inthandler21(int *esp)
// PS/2キーボードからの割り込み */
{
    int data;
    io_out8(PIC0_OCW2, 0x61);   // IRQ-01受付完了をPICに追加
    data = io_in8(PORT_KEY_DATA);
    fifo32_put(keyboard_fifo, data + keyboard_data_base);
    return;
}

#define PORT_KEY_STATUS			  0x0064
#define KEY_STATUS_SEND_NOTREAD   0x02
#define KEY_COMMAND_WRITE_MODE    0x60
#define KBC_MODE                  0x47

// キーボードコントローラ（KBC）がデータ送信可能になるのを待つ
void wait_KBC_sendready(void) {
    for(;;) {
        if((io_in8(PORT_KEY_STATUS) & KEY_STATUS_SEND_NOTREAD) == 0)
            return;
    }
}

// キーボードコントローラの初期化
void init_keyboard(struct FIFO32 *fifo, int data) {
    // 書き込み先のFIFOバッファを初期化
    keyboard_fifo = fifo;
    keyboard_data_base = data;
    // キーボードコントローラの初期化
    wait_KBC_sendready();
    io_out8(PORT_KEY_COMMAND, KEY_COMMAND_WRITE_MODE);
    wait_KBC_sendready();
    io_out8(PORT_KEY_DATA, KBC_MODE);
    return;
}
