// bootpackのメイン
#include <stdio.h>
#include "bootpack.h"



void BungoMain(void)
{
    struct BOOT_INFO *boot_info = (struct BOOT_INFO *) ADR_BOOTINFO;
    struct MOUSE_DECODER mouse_decoder;
    char s[40], mouse_cursor[256];
    char keyboard_buffer[KEY_BUF_SIZE], mouse_buffer[MOUSE_BUF_SIZE];
	int mouse_x, mouse_y;
    int data;
    unsigned int memory_total;
    struct MEMORY_MANAGER *memory_manager = (struct MEMORY_MANAGER *) MEMORY_MANAGER_ADDRESS;
    struct SHEET_CONTROL *sheet_ctl;
    struct SHEET *sheet_back, *sheet_mouse;
    unsigned char *sheet_buffer_back, sheet_buffer_mouse[160];

    init_gdtidt();
    init_pic();
    io_sti();       // IDT/PICの初期化が終わったのでCPUの割り込み禁止を解除

    fifo8_init(&keyboard_fifo, KEY_BUF_SIZE,   keyboard_buffer);
    fifo8_init(&mouse_fifo,    MOUSE_BUF_SIZE, mouse_buffer);
    io_out8(PIC0_IMR, 0xf9);    // PIC1とキーボードを許可(11111001)
    io_out8(PIC1_IMR, 0xef);    // マウスを許可(11101111)

    init_keyboard();
    enable_mouse(&mouse_decoder);

    memory_total = memtest(0x00400000, 0xbfffffff);
    memman_init(memory_manager);
    memman_free(memory_manager, 0x00001000, 0x0009e000); // 0x00001000 - 0x0009efff
    memman_free(memory_manager, 0x00400000, memory_total - 0x00400000);

    init_palette();

    sheet_ctl = sheet_control_init(memory_manager, boot_info->vram, boot_info->screen_x, boot_info->screen_y);
    sheet_back  = sheet_alloc(sheet_ctl);
    sheet_mouse = sheet_alloc(sheet_ctl);
    sheet_buffer_back = (unsigned char *)memman_alloc_4k(memory_manager, boot_info->screen_x * boot_info->screen_y);
    sheet_set_buffer(sheet_back, sheet_buffer_back, boot_info->screen_x, boot_info->screen_y, -1);
    sheet_set_buffer(sheet_mouse, sheet_buffer_mouse, 10, 16, 99);

    init_screen8(sheet_buffer_back, boot_info->screen_x, boot_info->screen_y);
    init_mouse_cursor8(sheet_buffer_mouse, 99);
    sheet_slide(sheet_ctl, sheet_back, 0, 0);
    mouse_x = (boot_info->screen_x - 16) / 2;    // 画面中央になるように座標計算
    mouse_y = (boot_info->screen_y - 28 - 16) / 2;
    sheet_slide(sheet_ctl, sheet_mouse, mouse_x, mouse_y);
    sheet_updown(sheet_ctl, sheet_back, 0);
    sheet_updown(sheet_ctl, sheet_mouse, 1);
    sprintf(s, "(%3d, %3d)", mouse_x, mouse_y);
	putfonts8_ascii(sheet_buffer_back, boot_info->screen_x, 0, 0, COL8_FFFFFF, s);
    sprintf(s, "memory %dMB   free : %dKB",
            memory_total / (1024 * 1024), memman_total(memory_manager) / 1024);
    putfonts8_ascii(sheet_buffer_back, boot_info->screen_x, 0, 32, COL8_FFFFFF, s);
    sheet_refresh(sheet_ctl, sheet_back, 0, 0, boot_info->screen_x, 48);

    for (;;) {
        io_cli();
        if (fifo8_status(&keyboard_fifo) + fifo8_status(&mouse_fifo) == 0) {
            io_stihlt();
        }
        else {
            if(fifo8_status(&keyboard_fifo) != 0) {
                data = fifo8_get(&keyboard_fifo);
                io_sti();
                sprintf(s, "%02X", data);
    			boxfill8(sheet_buffer_back, boot_info->screen_x, COL8_008484, 0, 16, 15, 31);
    			putfonts8_ascii(sheet_buffer_back, boot_info->screen_x, 0, 16, COL8_FFFFFF, s);
                sheet_refresh(sheet_ctl, sheet_back, 0, 16, 16, 32);
            }
            else if (fifo8_status(&mouse_fifo) != 0) {
                data = fifo8_get(&mouse_fifo);
                io_sti();
                if (mouse_decode(&mouse_decoder, data) != 0) {
                    sprintf(s, "[lcr %4d %4d]", mouse_decoder.x, mouse_decoder.y);
                    if ((mouse_decoder.button & 0x01) != 0) {
                        s[1] = 'L';
                    }
                    if ((mouse_decoder.button & 0x02) != 0) {
                        s[3] = 'R';
                    }
                    if ((mouse_decoder.button & 0x04) != 0) {
                        s[2] = 'C';
                    }
        			boxfill8(sheet_buffer_back, boot_info->screen_x, COL8_008484, 32, 16, 32 + 15 * 8 - 1, 31);
        			putfonts8_ascii(sheet_buffer_back, boot_info->screen_x, 32, 16, COL8_FFFFFF, s);
                    sheet_refresh(sheet_ctl, sheet_back, 32, 16, 32 + 15 * 8, 32);
                    // マウスカーソルの移動
                    mouse_x += mouse_decoder.x;
                    mouse_y += mouse_decoder.y;
                    if (mouse_x < 0) {
                        mouse_x = 0;
                    }
                    if (mouse_x > boot_info->screen_x - 10) {
                        mouse_x = boot_info->screen_x - 10;
                    }
                    if (mouse_y < 0) {
                        mouse_y = 0;
                    }
                    if (mouse_y > boot_info->screen_y - 16) {
                        mouse_y = boot_info->screen_y - 16;
                    }
                    sprintf(s, "(%d, %d)", mouse_x, mouse_y);
                    boxfill8(sheet_buffer_back, boot_info->screen_x, COL8_008484, 0, 0, 79, 15);  // 座標消す
                    putfonts8_ascii(sheet_buffer_back, boot_info->screen_x, 0, 0, COL8_FFFFFF, s); // 座標書く
                    sheet_refresh(sheet_ctl, sheet_back, 0, 0, 80, 16);
                    sheet_slide(sheet_ctl, sheet_mouse, mouse_x, mouse_y);
                }
            }
        }
    }
}
