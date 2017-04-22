// bootpackのメイン
#include <stdio.h>
#include "bootpack.h"

void make_window8(unsigned char *buffer, int width, int height, char *title);
void make_textbox(struct SHEET *sheet, int x0, int y0, int width, int height, int color);
void putfonts8_ascii_sheet(struct SHEET *sheet, int x, int y, int color, int bg_color, char *s, int l);
void task_b_main(struct SHEET *sheet_back);

void BungoMain(void)
{
    struct BOOT_INFO *boot_info = (struct BOOT_INFO *) ADR_BOOTINFO;
    struct FIFO32 fifo;
    int fifo_buffer[FIFO_BUF_SIZE];
    char s[40];
    struct MOUSE_DECODER mouse_decoder;
    int mouse_x, mouse_y;
	int cursor_x, cursor_c;
    int data;
    unsigned int memory_total;
    struct MEMORY_MANAGER *memory_manager = (struct MEMORY_MANAGER *) MEMORY_MANAGER_ADDRESS;
    struct SHEET_CONTROL *sheet_ctl;
    struct SHEET *sheet_back, *sheet_mouse, *sheet_window;
    unsigned char *sheet_buffer_back, *sheet_buffer_window, sheet_buffer_mouse[160];
    struct TIMER *timer01, *timer02, *timer03;
    static char keytable[0x54] = {
        0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0,   0,
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[', 0,   0,   'A', 'S',
        'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0,   0,   ']', 'Z', 'X', 'C', 'V',
        'B', 'N', 'M', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
        0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
        '2', '3', '0', '.'
    };
    struct TSS32 tss_a, tss_b;
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;
    int task_b_esp;

    init_gdtidt();
    init_pic();
    io_sti();       // IDT/PICの初期化が終わったのでCPUの割り込み禁止を解除

    fifo32_init(&fifo, FIFO_BUF_SIZE, fifo_buffer);
    init_pit();
    init_keyboard(&fifo, 0x0100);
    enable_mouse(&fifo, 0x200, &mouse_decoder);

    io_out8(PIC0_IMR, 0xf8);    // PITとPIC1とキーボードを許可(11111000)
    io_out8(PIC1_IMR, 0xef);    // マウスを許可(11101111)

    timer01 = timer_alloc();
    timer_init(timer01, &fifo, 10);
    timer_set_time(timer01, 1000);
    timer02 = timer_alloc();
    timer_init(timer02, &fifo, 3);
    timer_set_time(timer02, 300);
    timer03 = timer_alloc();
    timer_init(timer03, &fifo, 1);
    timer_set_time(timer03, 50);

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
    make_window8(sheet_buffer_window, 160, 52, "window");
    make_textbox(sheet_window, 8, 28, 144, 16, COL8_FFFFFF);
    cursor_x = 8;
    cursor_c = COL8_FFFFFF;
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
    putfonts8_ascii_sheet(sheet_back, 0, 32, COL8_FFFFFF, COL8_008484, s, 40);

    tss_a.ldtr  = 0;
    tss_a.iomap = 0x40000000;
    tss_b.ldtr  = 0;
    tss_b.iomap = 0x40000000;
    set_segment_descriptor(gdt + 3, 103, (int) &tss_a, AR_TSS32);
    set_segment_descriptor(gdt + 4, 103, (int) &tss_b, AR_TSS32);
    load_tr(3 * 8);
    task_b_esp = memman_alloc_4k(memory_manager, 64 * 1024) + 64 * 1024 - 8;
    tss_b.eip    = (int) &task_b_main;
    tss_b.eflags = 0x00000202;    // IF = 1;
    tss_b.eax    = 0;
    tss_b.ecx    = 0;
    tss_b.edx    = 0;
    tss_b.ebx    = 0;
    tss_b.esp    = task_b_esp;
    tss_b.ebp    = 0;
    tss_b.esi    = 0;
    tss_b.edi    = 0;
    tss_b.es     = 1 * 8;
    tss_b.cs     = 2 * 8;
    tss_b.ss     = 1 * 8;
    tss_b.ds     = 1 * 8;
    tss_b.fs     = 1 * 8;
    tss_b.gs     = 1 * 8;
    *((int *) (task_b_esp + 4)) = (int) sheet_back;
    tasks_init();

    for (;;) {
        io_cli();
        if (fifo32_status(&fifo) == 0) {
            io_stihlt();
        }
        else {
            data = fifo32_get(&fifo);
            io_sti();
            // キーボード操作
            if(0x0100 <= data && data < 0x0200) {
                if (data < 0x0100 + 0x54) {
                    if (keytable[data - 0x0100] != 0 && cursor_x <= 144) {
                        s[0] = keytable[data - 0x0100];
                        s[1] = 0;
                        putfonts8_ascii_sheet(sheet_window, cursor_x, 28, COL8_000000, COL8_FFFFFF, s, 1);
                        if (cursor_x < 144) { cursor_x += 8; }
                    }
                }
                if (data == 0x0100 + 0xe && 8 <= cursor_x) {
                    putfonts8_ascii_sheet(sheet_window, cursor_x, 28, COL8_000000, COL8_FFFFFF, " ", 1);
                    if (cursor_x > 8) { cursor_x -= 8; }
                }
                // カーソルの再表示
				boxfill8(sheet_window->buffer, sheet_window->width, cursor_c, cursor_x, 28, cursor_x + 7, 43);
				sheet_refresh(sheet_window, cursor_x, 28, cursor_x + 8, 44);
            }
            // マウス操作
            else if (0x0200 <= data && data < 0x0300) {
                if (mouse_decode(&mouse_decoder, data - 0x0200) != 0) {
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
                    putfonts8_ascii_sheet(sheet_back, 32, 16, COL8_FFFFFF, COL8_008484, s, 15);
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
                    sprintf(s, "(%3d, %3d)", mouse_x, mouse_y);
                    putfonts8_ascii_sheet(sheet_back, 0, 0, COL8_FFFFFF, COL8_008484, s, 10);
                    sheet_slide(sheet_mouse, mouse_x, mouse_y);
                    if ((mouse_decoder.button & 0x01) != 0) {
                        sheet_slide(sheet_window, mouse_x, mouse_y);
                    }
                }
            }
            else if (data == 10) {
                putfonts8_ascii_sheet(sheet_back, 0, 64, COL8_FFFFFF, COL8_008484, "10[sec]", 7);
            }
            else if (data == 3) {
                putfonts8_ascii_sheet(sheet_back, 0, 80, COL8_FFFFFF, COL8_008484, " 3[sec]", 7);
            }
            else if (data <= 1) {
                if (data != 0){
                    timer_init(timer03, &fifo, 0);
                    cursor_c = COL8_000000;
                }
                else {
                    timer_init(timer03, &fifo, 1);
                    cursor_c = COL8_FFFFFF;
                }
                timer_set_time(timer03, 50);
                boxfill8(sheet_buffer_window, sheet_window->width, cursor_c, cursor_x, 28, cursor_x + 7, 43);
                sheet_refresh(sheet_window, cursor_x, 28, cursor_x + 8, 44);
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

void make_textbox(struct SHEET *sheet, int x0, int y0, int width, int height, int color) {
    int x1 = x0 + width;
    int y1 = y0 + height;

    boxfill8(sheet->buffer, sheet->width, COL8_848484, x0 - 2, y0 - 3, x1 + 1, y0 - 3);
    boxfill8(sheet->buffer, sheet->width, COL8_848484, x0 - 3, y0 - 3, x0 - 3, y1 + 1);
    boxfill8(sheet->buffer, sheet->width, COL8_FFFFFF, x0 - 3, y1 + 2, x1 + 1, y1 + 2);
    boxfill8(sheet->buffer, sheet->width, COL8_FFFFFF, x1 + 2, y0 - 3, x1 + 2, y1 + 2);
    boxfill8(sheet->buffer, sheet->width, COL8_000000, x0 - 1, y0 - 2, x1 + 0, y0 - 2);
    boxfill8(sheet->buffer, sheet->width, COL8_000000, x0 - 2, y0 - 2, x0 - 2, y1 + 0);
    boxfill8(sheet->buffer, sheet->width, COL8_C6C6C6, x0 - 2, y1 + 1, x1 + 0, y1 + 1);
    boxfill8(sheet->buffer, sheet->width, COL8_C6C6C6, x1 + 1, y0 - 2, x1 + 1, y1 + 1);
    boxfill8(sheet->buffer, sheet->width, color      , x0 - 1, y0 - 1, x1 + 0, y1 + 0);
    return;
}

void putfonts8_ascii_sheet(struct SHEET *sheet, int x, int y, int color, int bg_color, char *s, int l) {
    boxfill8(sheet->buffer, sheet->width, bg_color, x, y, x + (8*l) - 1, y + 15);
    putfonts8_ascii(sheet->buffer, sheet->width, x, y, color, s);
    sheet_refresh(sheet, x, y, x + (8*l), y + 16);
}

void task_b_main(struct SHEET *sheet_back) {
    struct FIFO32 fifo;
    int fifo_buffer[FIFO_BUF_SIZE];
    int fifo_data;
    struct TIMER  *timer_put;
    int count = 0;
    char s[11];

    fifo32_init(&fifo, FIFO_BUF_SIZE, fifo_buffer);
    timer_put = timer_alloc();
    timer_init(timer_put, &fifo, 1);
    timer_set_time(timer_put, 1);

    for (;;) {
        count++;
        io_cli();
        if (fifo32_status(&fifo) == 0) {
            io_stihlt();
        }
        else {
            fifo_data = fifo32_get(&fifo);
            io_sti();
            if (fifo_data == 1) {
                sprintf(s, "%10d", count);
                putfonts8_ascii_sheet(sheet_back, 0, 144, COL8_FFFFFF, COL8_008484, s, 10);
                timer_set_time(timer_put, 1);
            }
        }
    }
}
