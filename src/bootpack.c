// bootpackのメイン
#include <stdio.h>
#include "bootpack.h"

void make_window8(unsigned char *buffer, int width, int height, char *title);

void BungoMain(void)
{
    struct BOOT_INFO *boot_info = (struct BOOT_INFO *) ADR_BOOTINFO;
    char s[40];
    char keyboard_buffer[KEY_BUF_SIZE], mouse_buffer[MOUSE_BUF_SIZE];
    struct MOUSE_DECODER mouse_decoder;
	int mouse_x, mouse_y;
    int data;
    unsigned int memory_total;
    struct MEMORY_MANAGER *memory_manager = (struct MEMORY_MANAGER *) MEMORY_MANAGER_ADDRESS;
    struct SHEET_CONTROL *sheet_ctl;
    struct SHEET *sheet_back, *sheet_mouse, *sheet_window;
    unsigned char *sheet_buffer_back, *sheet_buffer_window, sheet_buffer_mouse[160];
    struct TIMER *timer01, *timer02, *timer03;
    struct FIFO8 timer_fifo01, timer_fifo02, timer_fifo03;
    char timer_buffer01[TIMER_BUF_SIZE], timer_buffer02[TIMER_BUF_SIZE], timer_buffer03[TIMER_BUF_SIZE];


    init_gdtidt();
    init_pic();
    io_sti();       // IDT/PICの初期化が終わったのでCPUの割り込み禁止を解除

    fifo8_init(&keyboard_fifo, KEY_BUF_SIZE,   keyboard_buffer);
    fifo8_init(&mouse_fifo,    MOUSE_BUF_SIZE, mouse_buffer);
    init_pit();
    io_out8(PIC0_IMR, 0xf8);    // PITとPIC1とキーボードを許可(11111000)
    io_out8(PIC1_IMR, 0xef);    // マウスを許可(11101111)

    timer01 = timer_alloc();
    timer02 = timer_alloc();
    timer03 = timer_alloc();
    fifo8_init(&timer_fifo01, TIMER_BUF_SIZE, timer_buffer01);
    fifo8_init(&timer_fifo02, TIMER_BUF_SIZE, timer_buffer02);
    fifo8_init(&timer_fifo03, TIMER_BUF_SIZE, timer_buffer03);
    timer_init(timer01, &timer_fifo01, 1);
    timer_init(timer02, &timer_fifo02, 1);
    timer_init(timer03, &timer_fifo03, 1);
    timer_set_time(timer01, 1000);
    timer_set_time(timer02, 300);
    timer_set_time(timer03, 50);

    init_keyboard();
    enable_mouse(&mouse_decoder);

    memory_total = memtest(0x00400000, 0xbfffffff);
    memman_init(memory_manager);
    memman_free(memory_manager, 0x00001000, 0x0009e000); // 0x00001000 - 0x0009efff
    memman_free(memory_manager, 0x00400000, memory_total - 0x00400000);

    init_palette();

    sheet_ctl = sheet_control_init(memory_manager, boot_info->vram, boot_info->screen_x, boot_info->screen_y);
    sheet_back   = sheet_alloc(sheet_ctl);
    sheet_window = sheet_alloc(sheet_ctl);
    sheet_mouse  = sheet_alloc(sheet_ctl);
    sheet_buffer_back   = (unsigned char *)memman_alloc_4k(memory_manager, boot_info->screen_x * boot_info->screen_y);
    sheet_buffer_window = (unsigned char *)memman_alloc_4k(memory_manager, 160 * 52);
    sheet_set_buffer(sheet_back, sheet_buffer_back, boot_info->screen_x, boot_info->screen_y, -1);
    sheet_set_buffer(sheet_window, sheet_buffer_window, 160, 52, -1);
    sheet_set_buffer(sheet_mouse, sheet_buffer_mouse, 10, 16, 99);

    init_screen8(sheet_buffer_back, boot_info->screen_x, boot_info->screen_y);
    init_mouse_cursor8(sheet_buffer_mouse, 99);
    make_window8(sheet_buffer_window, 160, 52, "counter");
    sheet_slide(sheet_back, 0, 0);
    mouse_x = (boot_info->screen_x - 16) / 2;    // 画面中央になるように座標計算
    mouse_y = (boot_info->screen_y - 28 - 16) / 2;
    sheet_slide(sheet_window, 80, 72);
    sheet_slide(sheet_mouse,  mouse_x, mouse_y);
    sheet_updown(sheet_back,   0);
    sheet_updown(sheet_window, 1);
    sheet_updown(sheet_mouse,  2);
    sprintf(s, "(%3d, %3d)", mouse_x, mouse_y);
	putfonts8_ascii(sheet_buffer_back, boot_info->screen_x, 0, 0, COL8_FFFFFF, s);
    sprintf(s, "memory %dMB   free : %dKB",
            memory_total / (1024 * 1024), memman_total(memory_manager) / 1024);
    putfonts8_ascii(sheet_buffer_back, boot_info->screen_x, 0, 32, COL8_FFFFFF, s);
    sheet_refresh(sheet_back, 0, 0, boot_info->screen_x, 48);

    for (;;) {
        sprintf(s, "%010d", timer_ctl.count);
        boxfill8(sheet_buffer_window, 160, COL8_C6C6C6, 40, 28, 119, 43);
        putfonts8_ascii(sheet_buffer_window, 160, 40, 28, COL8_000000, s);
        sheet_refresh(sheet_window, 40, 28, 120, 44);

        io_cli();
        if (fifo8_status(&keyboard_fifo) + fifo8_status(&mouse_fifo) + fifo8_status(&timer_fifo01)
                + fifo8_status(&timer_fifo02) + fifo8_status(&timer_fifo03) == 0) {
            io_sti();
        }
        else {
            if(fifo8_status(&keyboard_fifo) != 0) {
                data = fifo8_get(&keyboard_fifo);
                io_sti();
                sprintf(s, "%02X", data);
    			boxfill8(sheet_buffer_back, boot_info->screen_x, COL8_008484, 0, 16, 15, 31);
    			putfonts8_ascii(sheet_buffer_back, boot_info->screen_x, 0, 16, COL8_FFFFFF, s);
                sheet_refresh(sheet_back, 0, 16, 16, 32);
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
                    sheet_refresh(sheet_back, 32, 16, 32 + 15 * 8, 32);
                    // マウスカーソルの移動
                    mouse_x += mouse_decoder.x;
                    mouse_y += mouse_decoder.y;
                    if (mouse_x < 0) {
                        mouse_x = 0;
                    }
                    if (mouse_x > boot_info->screen_x - 1) {
                        mouse_x = boot_info->screen_x - 1;
                    }
                    if (mouse_y < 0) {
                        mouse_y = 0;
                    }
                    if (mouse_y > boot_info->screen_y - 1) {
                        mouse_y = boot_info->screen_y - 1;
                    }
                    sprintf(s, "(%d, %d)", mouse_x, mouse_y);
                    boxfill8(sheet_buffer_back, boot_info->screen_x, COL8_008484, 0, 0, 79, 15);  // 座標消す
                    putfonts8_ascii(sheet_buffer_back, boot_info->screen_x, 0, 0, COL8_FFFFFF, s); // 座標書く
                    sheet_refresh(sheet_back, 0, 0, 80, 16);
                    sheet_slide(sheet_mouse, mouse_x, mouse_y);
                }
            }
            else if (fifo8_status(&timer_fifo01) != 0) {
                data = fifo8_get(&timer_fifo01); // とりあえず読み込む（空にするため）
                io_sti();
                putfonts8_ascii(sheet_buffer_back, boot_info->screen_x, 0, 64, COL8_FFFFFF, "10[sec]");
                sheet_refresh(sheet_back, 0, 64, 56, 80);
            }
            else if (fifo8_status(&timer_fifo02) != 0) {
                data = fifo8_get(&timer_fifo02); // とりあえず読み込む（空にするため）
                io_sti();
                putfonts8_ascii(sheet_buffer_back, boot_info->screen_x, 0, 80, COL8_FFFFFF, "3[sec]");
                sheet_refresh(sheet_back, 0, 80, 48, 96);
            }
            else if (fifo8_status(&timer_fifo03) != 0) {
                data = fifo8_get(&timer_fifo03); // とりあえず読み込む（空にするため）
                io_sti();
                if (data != 0) {
                    timer_init(timer03, &timer_fifo03, 0);
                    boxfill8(sheet_buffer_back, boot_info->screen_x, COL8_FFFFFF, 8, 96, 15, 111);
                }
                else {
                    timer_init(timer03, &timer_fifo03, 1);
                    boxfill8(sheet_buffer_back, boot_info->screen_x, COL8_008484, 8, 96, 15, 111);
                }
                timer_set_time(timer03, 50);
                sheet_refresh(sheet_back, 8, 96, 16, 112);
            }
        }
    }
}

void make_window8(unsigned char *buffer, int width, int height, char *title) {
    static char close_button[14][16] = {
        "OOOOOOOOOOOOOOO@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQQQ@@QQQQQ$@",
		"OQQQQQ@@@@QQQQ$@",
		"OQQQQ@@QQ@@QQQ$@",
		"OQQQ@@QQQQ@@QQ$@",
		"OQQQQQQQQQQQQQ$@",
		"OQQQQQQQQQQQQQ$@",
		"O$$$$$$$$$$$$$$@",
		"@@@@@@@@@@@@@@@@"
    };
    int x, y;
    char c;

    boxfill8(buffer, width, COL8_C6C6C6, 0,         0,          width - 1, 0         );
	boxfill8(buffer, width, COL8_FFFFFF, 1,         1,          width - 2, 1         );
	boxfill8(buffer, width, COL8_C6C6C6, 0,         0,          0,         height - 1);
	boxfill8(buffer, width, COL8_FFFFFF, 1,         1,          1,         height - 2);
	boxfill8(buffer, width, COL8_848484, width - 2, 1,          width - 2, height - 2);
	boxfill8(buffer, width, COL8_000000, width - 1, 0,          width - 1, height - 1);
	boxfill8(buffer, width, COL8_C6C6C6, 2,         2,          width - 3, height - 3);
	boxfill8(buffer, width, COL8_000084, 3,         3,          width - 4, 20        );
	boxfill8(buffer, width, COL8_848484, 1,         height - 2, width - 2, height - 2);
	boxfill8(buffer, width, COL8_000000, 0,         height - 1, width - 1, height - 1);
	putfonts8_ascii(buffer, width, 24, 4, COL8_FFFFFF, title);

    for (y = 0; y < 14; y++) {
        for (x = 0; x < 16; x++) {
            c = close_button[y][x];
            if (c == '@') {
                c = COL8_000000;
            }
            else if (c == '$') {
                c = COL8_848484;
            }
            else if (c == 'Q') {
                c = COL8_C6C6C6;
            }
            else {
                c = COL8_FFFFFF;
            }
            buffer[(5+y) * width + (width - 21 + x)] = c;
        }
    }
    return;
}
