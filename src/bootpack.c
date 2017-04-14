// bootpackのメイン
#include <stdio.h>
#include "bootpack.h"

extern struct FIFO8 keyboard_fifo;
extern struct FIFO8 mouse_fifo;
void enable_mouse(void);
void init_keyboard(void);

void BungoMain(void)
{
    struct BOOT_INFO *boot_info = (struct BOOT_INFO *) ADR_BOOTINFO;
    char s[40];
    char mcursor[256];
    char keyboard_buffer[KEY_BUF_SIZE];
    char mouse_buffer[MOUSE_BUF_SIZE];
	int mx, my;
    int i, j;

    init_gdtidt();
    init_pic();
    io_sti();       // IDT/PICの初期化が終わったのでCPUの割り込み禁止を解除

    fifo8_init(&keyboard_fifo, KEY_BUF_SIZE,   keyboard_buffer);
    fifo8_init(&mouse_fifo,    MOUSE_BUF_SIZE, mouse_buffer);
    io_out8(PIC0_IMR, 0xf9);    // PIC1とキーボードを許可(11111001)
    io_out8(PIC1_IMR, 0xef);    // マウスを許可(11101111)

    init_keyboard();

    init_palette();
    init_screen(boot_info->vram, boot_info->screen_x, boot_info->screen_y);
    mx = (boot_info->screen_x - 16) / 2;    // 画面中央になるように座標計算
	my = (boot_info->screen_y - 28 - 16) / 2;
	init_mouse_cursor8(mcursor, COL8_008484);
	putblock8_8(boot_info->vram, boot_info->screen_x, 16, 16, mx, my, mcursor, 16);
    sprintf(s, "(%d, %d)", mx, my);
    putfonts8_ascii(boot_info->vram, boot_info->screen_x, 0, 0, COL8_FFFFFF, s);

    enable_mouse();

    for (;;) {
        io_cli();
        if (fifo8_status(&keyboard_fifo) + fifo8_status(&mouse_fifo) == 0) {
            io_stihlt();
        }
        else {
            if(fifo8_status(&keyboard_fifo) != 0) {
                i = fifo8_get(&keyboard_fifo);
                io_sti();
                sprintf(s, "%02X", i);
    			boxfill8(boot_info->vram, boot_info->screen_x, COL8_008484, 0, 16, 15, 31);
    			putfonts8_ascii(boot_info->vram, boot_info->screen_x, 0, 16, COL8_FFFFFF, s);
            }
            else if (fifo8_status(&mouse_fifo) != 0) {
                i = fifo8_get(&mouse_fifo);
                io_sti();
                sprintf(s, "%02X", i);
    			boxfill8(boot_info->vram, boot_info->screen_x, COL8_008484, 32, 16, 47, 31);
    			putfonts8_ascii(boot_info->vram, boot_info->screen_x, 32, 16, COL8_FFFFFF, s);
            }
        }
    }
}

#define PORT_KEY_DATA			  0x0060
#define PORT_KEY_STATUS			  0x0064
#define PORT_KEY_COMMAND		  0x0064
#define KEY_STATUS_SEND_NOTREAD   0x02
#define KEY_COMMAND_WRITE_MODE    0x60
#define KBC_MODE                  0x47

// キーボードコントローラ（KBC）がデータ送信可能になるのを待つ
void wait_KBC_sendready(void) {
    for(;;) {
        if((io_in8(PORT_KEY_STATUS) & KEY_STATUS_SEND_NOTREAD) == 0) {
            break;
        }
    }
    return;
}

// キーボードコントローラの初期化
void init_keyboard(void) {
    wait_KBC_sendready();
    io_out8(PORT_KEY_COMMAND, KEY_COMMAND_WRITE_MODE);
    wait_KBC_sendready();
    io_out8(PORT_KEY_DATA, KBC_MODE);
    return;
}

#define KEY_COMMAND_SENDTO_MOUSE    0xd4
#define MOUSE_COMMAND_ENABLE        0xf4

// マウス有効
void enable_mouse(void) {
    wait_KBC_sendready();
    io_out8(PORT_KEY_COMMAND, KEY_COMMAND_SENDTO_MOUSE);
    wait_KBC_sendready();
    io_out8(PORT_KEY_DATA, MOUSE_COMMAND_ENABLE);
    return; // うまくいくと、ACK（0xfa）が送信されてくる
}
