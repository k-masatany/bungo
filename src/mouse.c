// マウス関係
#include "bootpack.h"

struct FIFO8 mouse_fifo;
void inthandler2c(int *esp)
// PS/2マウスからの割り込み */
{
    unsigned char data;
    io_out8(PIC1_OCW2, 0x64);	// IRQ-12受付完了をPIC1に通知
	io_out8(PIC0_OCW2, 0x62);	// IRQ-02受付完了をPIC0に通知
	data = io_in8(PORT_KEY_DATA);
    fifo8_put(&mouse_fifo, data);
    return;
}

#define KEY_COMMAND_SENDTO_MOUSE    0xd4
#define MOUSE_COMMAND_ENABLE        0xf4

// マウス有効
void enable_mouse(struct MOUSE_DECODER *mouse_decoder) {
    wait_KBC_sendready();
    io_out8(PORT_KEY_COMMAND, KEY_COMMAND_SENDTO_MOUSE);
    wait_KBC_sendready();
    io_out8(PORT_KEY_DATA, MOUSE_COMMAND_ENABLE);
    // うまくいくと、ACK（0xfa）が送信されてくる
    mouse_decoder->phase = 0;
    return;
}

int mouse_decode(struct MOUSE_DECODER *mouse_decoder, unsigned char data) {
    if (mouse_decoder->phase == 0) {
        if (data == 0xfa) {
            mouse_decoder->phase = 1;
        }
        return 0;
    }
    else if (mouse_decoder->phase == 1) {
        if ((data & 0xc8) == 0x08) {
            // 正しい1バイト目ならphase2に移行
            mouse_decoder->buffer[0] = data;
            mouse_decoder->phase = 2;
        }
        return 0;
    }
    else if (mouse_decoder->phase == 2) {
        mouse_decoder->buffer[1] = data;
        mouse_decoder->phase = 3;
        return 0;
    }
    else if (mouse_decoder->phase == 3) {
        mouse_decoder->buffer[2] = data;
        mouse_decoder->phase = 1;
        mouse_decoder->button = mouse_decoder->buffer[0] & 0x07;
        mouse_decoder->x      = mouse_decoder->buffer[1];
        mouse_decoder->y      = mouse_decoder->buffer[2];
        if ((mouse_decoder->buffer[0] & 0x10) != 0) {
            mouse_decoder->x |= 0xffffff00;
        }
        if ((mouse_decoder->buffer[0] & 0x20) != 0) {
            mouse_decoder->y |= 0xffffff00;
        }
        mouse_decoder->y = - mouse_decoder->y;
        return 1;
    }
    return -1; // ここに来ることはないはず
}
