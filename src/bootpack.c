// bootpackのメイン
#include <stdio.h>
#include "bootpack.h"

void make_window8(unsigned char *buffer, int width, int height, char *title, char active);
void make_window_title(unsigned char *buffer, int width, char *title, char active);
void make_textbox8(struct SHEET *sheet, int x0, int y0, int width, int height, int color);
void putfonts8_ascii_sheet(struct SHEET *sheet, int x, int y, int color, int bg_color, char *s, int l);
void console_task(struct SHEET *sheet);

#define KEY_COMMAND_LED		0xed

void BungoMain(void)
{
    struct BOOT_INFO *boot_info = (struct BOOT_INFO *) ADR_BOOTINFO;
    struct FIFO32 fifo;
    struct FIFO32 key_command;
    int fifo_buffer[FIFO_BUF_SIZE];
    int key_command_buffer[KEY_CMD_BUF];
    char s[40];
    struct MOUSE_DECODER mouse_decoder;
    int mouse_x, mouse_y;
	int cursor_x, cursor_c;
    int data;
    unsigned int memory_total;
    struct MEMORY_MANAGER *memory_manager = (struct MEMORY_MANAGER *) MEMORY_MANAGER_ADDRESS;
    struct SHEET_CONTROL *sheet_ctl;
    struct SHEET *sheet_back, *sheet_mouse, *sheet_window, *sheet_console;
    unsigned char *sheet_buffer_back, sheet_buffer_mouse[160];
    unsigned char *sheet_buffer_window, *sheet_buffer_console;
    struct TIMER *cursor_timer;
    struct TASK *task_a, *task_console;
    static char key_table[0x80] = {
        0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[', 0,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0,   0,   ']', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0x5c, 0,  0,   0,   0,   0,   0,   0,   0,   0,   0x5c, 0,  0
	};
    static char shift_key_table[0x80] = {
        0,   0,   '!', 0x22, '#', '$', '%', '&', 0x27, '(', ')', '~', '=', '~', 0,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '`', '{', 0,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', '+', '*', 0,   0,   '}', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   '_', 0,   0,   0,   0,   0,   0,   0,   0,   0,   '|', 0,   0
	};
    int key_to = 0;
    int key_shift = 0;
    int key_leds = (boot_info->leds >> 4) & 7;
    int key_command_wait = -1;

    init_gdtidt();
    init_pic();
    io_sti();       // IDT/PICの初期化が終わったのでCPUの割り込み禁止を解除

    fifo32_init(&fifo, FIFO_BUF_SIZE, fifo_buffer, 0);
    fifo32_init(&key_command, KEY_CMD_BUF, key_command_buffer, 0);
    init_pit();
    init_keyboard(&fifo, 0x0100);
    enable_mouse(&fifo, 0x200, &mouse_decoder);

    io_out8(PIC0_IMR, 0xf8);    // PITとPIC1とキーボードを許可(11111000)
    io_out8(PIC1_IMR, 0xef);    // マウスを許可(11101111)
    // timer
    cursor_timer = timer_alloc();
    timer_init(cursor_timer, &fifo, 1);
    timer_set_time(cursor_timer, 50);
    // memory test
    memory_total = memtest(0x00400000, 0xbfffffff);
    memman_init(memory_manager);
    memman_free(memory_manager, 0x00001000, 0x0009e000); // 0x00001000 - 0x0009efff
    memman_free(memory_manager, 0x00400000, memory_total - 0x00400000);

    init_palette();
    task_a = task_init(memory_manager);
    fifo.task = task_a;
    task_run(task_a, 1, 0);

    sheet_ctl = sheet_control_init(memory_manager, boot_info->vram, boot_info->screen_x, boot_info->screen_y);
    // sheet back
    sheet_back   = sheet_alloc(sheet_ctl);
    sheet_buffer_back   = (unsigned char *)memman_alloc_4k(memory_manager, boot_info->screen_x * boot_info->screen_y);
    sheet_set_buffer(sheet_back, sheet_buffer_back, boot_info->screen_x, boot_info->screen_y, -1);
    init_screen8(sheet_buffer_back, boot_info->screen_x, boot_info->screen_y);
    // sheet mouse
    sheet_mouse  = sheet_alloc(sheet_ctl);
    sheet_set_buffer(sheet_mouse, sheet_buffer_mouse, 10, 16, 99);
    init_mouse_cursor8(sheet_buffer_mouse, 99);
    mouse_x = (boot_info->screen_x - 16) / 2;    // 画面中央になるように座標計算
    mouse_y = (boot_info->screen_y - 28 - 16) / 2;
    // sheet window
    sheet_window = sheet_alloc(sheet_ctl);
    sheet_buffer_window = (unsigned char *)memman_alloc_4k(memory_manager, 144 * 52);
    sheet_set_buffer(sheet_window, sheet_buffer_window, 144, 52, -1);
    make_window8(sheet_buffer_window, 144, 52, "task_a", 1);
    make_textbox8(sheet_window, 8, 28, 128, 16, COL8_FFFFFF);
    cursor_x = 8;
    cursor_c = COL8_FFFFFF;
    // sheet console
    sheet_console = sheet_alloc(sheet_ctl);
    sheet_buffer_console = (unsigned char *)memman_alloc_4k(memory_manager, 256 * 165);
    sheet_set_buffer(sheet_console, sheet_buffer_console, 256, 165, -1);
    make_window8(sheet_buffer_console, 256, 165, "console", 0);
    make_textbox8(sheet_console, 8, 28, 240, 128, COL8_000000);
    task_console = task_alloc();
    task_console->tss.esp = memman_alloc_4k(memory_manager, 64 * 1024) + 64 * 1024 - 8;
    task_console->tss.eip = (int) &console_task;
    task_console->tss.es  = 1 * 8;
    task_console->tss.cs  = 2 * 8;
    task_console->tss.ss  = 1 * 8;
    task_console->tss.ds  = 1 * 8;
    task_console->tss.fs  = 1 * 8;
    task_console->tss.gs  = 1 * 8;
    *((int *) (task_console->tss.esp + 4)) = (int) sheet_console;
    task_run(task_console, 2, 2);

    sheet_slide(sheet_back, 0, 0);
    sheet_slide(sheet_console, 32, 4);
    sheet_slide(sheet_window, 64, 56);
    sheet_slide(sheet_mouse,  mouse_x, mouse_y);
    sheet_updown(sheet_back,    0);
    sheet_updown(sheet_console, 1);
    sheet_updown(sheet_window,  2);
    sheet_updown(sheet_mouse,   3);

    sprintf(s, "(%3d, %3d)", mouse_x, mouse_y);
	putfonts8_ascii(sheet_buffer_back, boot_info->screen_x, 0, 0, COL8_FFFFFF, s);
    sprintf(s, "memory %dMB   free : %dKB",
            memory_total / (1024 * 1024), memman_total(memory_manager) / 1024);
    putfonts8_ascii_sheet(sheet_back, 0, 32, COL8_FFFFFF, COL8_008484, s, 40);

    // 最初にキーボードの状態と食い違いがないように設定をしておくことにする
    fifo32_put(&key_command, KEY_COMMAND_LED);
    fifo32_put(&key_command, key_leds);

    for (;;) {
        if (fifo32_status(&key_command) > 0 && key_command_wait < 0)
        {
            // キーボードコントローラに送るデータがあれば送る
            key_command_wait = fifo32_get(&key_command);
            wait_KBC_sendready();
            io_out8(PORT_KEY_DATA, key_command_wait);
        }
        io_cli();
        if (fifo32_status(&fifo) == 0) {
            task_sleep(task_a);
            io_sti();
        }
        else {
            data = fifo32_get(&fifo);
            io_sti();
            // キーボード操作
            if(0x0100 <= data && data < 0x0200) {
                sprintf(s, "%02X", data - 0x0100);
                putfonts8_ascii_sheet(sheet_back, 0, 16, COL8_FFFFFF, COL8_008484, s, 2);
                if (data < 0x80 + 0x0100) {
                    if (key_shift == 0) {
                        s[0] = key_table[data - 0x0100];
                    }
                    else {
                        s[0] = shift_key_table[data - 0x0100];
                    }
                }
                else {
                    s[0] = 0;
                }
                if ('A' <= s[0] && s[0] <= 'Z') {   // 入力がアルファベット
                    if (((key_leds & 4) == 0 && key_shift == 0) ||
                            ((key_leds & 4) != 0 && key_shift != 0)) {
                        s[0] += 0x20;   // to_lower
                    }
                }
                // 通常文字
                if (s[0] != 0) {
                    if (key_to == 0) {
                        if (cursor_x < 128) {
                            s[1] = 0;
                            putfonts8_ascii_sheet(sheet_window, cursor_x, 28, COL8_000000, COL8_FFFFFF, s, 1);
                            cursor_x += 8;
                        }
                    }
                    else {  // コンソールへ
                        fifo32_put(&task_console->fifo, s[0] + 0x0100);
                    }
                }
                // BS
                if (data == 0x0100 + 0x0e) {
                    if (key_to == 0) {
                        if (cursor_x > 8) {
                            putfonts8_ascii_sheet(sheet_window, cursor_x, 28, COL8_000000, COL8_FFFFFF, " ", 1);
                            cursor_x -= 8;
                        }
                    }
                    else {  // コンソールへ
                        fifo32_put(&task_console->fifo, 8 + 0x0100);
                    }
                }
                // TAB
                if (data == 0x0100 + 0x0f) {
                    if (key_to == 0) {
                        key_to = 1;
                        make_window_title(sheet_buffer_window,  sheet_window->width,  "task_a",  0);
                        make_window_title(sheet_buffer_console, sheet_console->width, "console", 1);
                    }
                    else {
                        key_to = 0;
                        make_window_title(sheet_buffer_window,  sheet_window->width,  "task_a",  1);
                        make_window_title(sheet_buffer_console, sheet_console->width, "console", 0);
                    }
                    sheet_refresh(sheet_window,  0, 0, sheet_window->width,  21);
                    sheet_refresh(sheet_console, 0, 0, sheet_console->width, 21);
                }
                // 左シフトON
                if (data == 0x0100 + 0x2a) {
                    key_shift |= 1;
                }
                // 左シフトOFF
                if (data == 0x0100 + 0xaa) {
                    key_shift &= ~1;
                }
                // 右シフトON
                if (data == 0x0100 + 0x36) {
                    key_shift |= 2;
                }
                // 右シフトOFF
                if (data == 0x0100 + 0xb6) {
                    key_shift &= ~2;
                }
                // caps lock
                if (data == 0x0100 + 0x3a) {
                    key_leds ^= 4;
                    fifo32_put(&key_command, KEY_COMMAND_LED);
                    fifo32_put(&key_command, key_leds);
                }
                // num lock
                if (data == 0x0100 + 0x45) {
                    key_leds ^= 2;
                    fifo32_put(&key_command, KEY_COMMAND_LED);
                    fifo32_put(&key_command, key_leds);
                }
                // scroll lock
                if (data == 0x0100 + 0x46) {
                    key_leds ^= 1;
                    fifo32_put(&key_command, KEY_COMMAND_LED);
                    fifo32_put(&key_command, key_leds);
                }
                // キーボードがデータを無事に受け取った
                if (data == 0x0100 + 0xfa) {
                    key_command_wait = -1;
                }
                // キーボードがデータを無事に受け取れなかった
                if (data == 0x0100 + 0xfe) {
                    wait_KBC_sendready();
                    io_out8(PORT_KEY_DATA, key_command_wait);
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
            else if (data <= 1) {
                if (data != 0){
                    timer_init(cursor_timer, &fifo, 0);
                    cursor_c = COL8_000000;
                }
                else {
                    timer_init(cursor_timer, &fifo, 1);
                    cursor_c = COL8_FFFFFF;
                }
                timer_set_time(cursor_timer, 50);
                boxfill8(sheet_buffer_window, sheet_window->width, cursor_c, cursor_x, 28, cursor_x + 7, 43);
                sheet_refresh(sheet_window, cursor_x, 28, cursor_x + 8, 44);
            }
        }
    }
}

void make_window8(unsigned char *buffer, int width, int height, char *title, char active) {
    boxfill8(buffer, width, COL8_C6C6C6, 0,         0,          width - 1, 0         );
	boxfill8(buffer, width, COL8_FFFFFF, 1,         1,          width - 2, 1         );
	boxfill8(buffer, width, COL8_C6C6C6, 0,         0,          0,         height - 1);
	boxfill8(buffer, width, COL8_FFFFFF, 1,         1,          1,         height - 2);
	boxfill8(buffer, width, COL8_848484, width - 2, 1,          width - 2, height - 2);
	boxfill8(buffer, width, COL8_000000, width - 1, 0,          width - 1, height - 1);
	boxfill8(buffer, width, COL8_C6C6C6, 2,         2,          width - 3, height - 3);
	boxfill8(buffer, width, COL8_848484, 1,         height - 2, width - 2, height - 2);
	boxfill8(buffer, width, COL8_000000, 0,         height - 1, width - 1, height - 1);
    make_window_title(buffer, width, title, active);
    return;
}

void make_window_title(unsigned char *buffer, int width, char *title, char active) {
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
    char c, tc, tbc;

    if (active != 0) {
        tc  = COL8_FFFFFF;
        tbc = COL8_000084;
    }
    else {
        tc  = COL8_C6C6C6;
        tbc = COL8_848484;
    }

    boxfill8(buffer, width, tbc, 3, 3, width - 4, 20);
	putfonts8_ascii(buffer, width, 24, 4, tc, title);

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

void make_textbox8(struct SHEET *sheet, int x0, int y0, int width, int height, int color) {
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

void console_task(struct SHEET *sheet) {
    struct TIMER *timer;
    struct TASK *task = task_now();

    int data;
    int fifo_buffer[FIFO_BUF_SIZE];
    int cursor_x = 16;
    int cursor_c = COL8_000000;
    char s[2];

    fifo32_init(&task->fifo, FIFO_BUF_SIZE, fifo_buffer, task);
    timer = timer_alloc();
    timer_init(timer, &task->fifo, 1);
    timer_set_time(timer, 50);

    // プロンプト表示
    putfonts8_ascii_sheet(sheet, 8, 28, COL8_FFFFFF, COL8_000000, "$", 1);

    for (;;) {
        io_cli();
        if (fifo32_status(&task->fifo) == 0) {
            task_sleep(task);
            io_sti();
        }
        else {
            data = fifo32_get(&task->fifo);
            io_sti();
            if (data <= 1) {
                if (data != 0) {
                    timer_init(timer, &task->fifo, 0);
                    cursor_c = COL8_FFFFFF;
                }
                else {
                    timer_init(timer, &task->fifo, 1);
                    cursor_c = COL8_000000;
                }
                timer_set_time(timer, 50);
            }
            if (0x0100 <= data && data < 0x0200) {  // キーボードデータ（タスクA経由）
                if (data == 8 + 0x0100) {   // バックスペース
                    if (cursor_x > 16) {
                        // カーソルをスペースで消し、カーソルを1つ戻す
                        s[0] = data - 0x0100;
                        s[1] = 0;
                        putfonts8_ascii_sheet(sheet, cursor_x, 28, COL8_FFFFFF, COL8_000000, " ", 1);
                        cursor_x -= 8;
                    }
                }
                else {
                    if (cursor_x < 240) {
                        // 1文字表示してから、カーソルを1つ進める
                        s[0] = data - 0x0100;
                        s[1] = 0;
                        putfonts8_ascii_sheet(sheet, cursor_x, 28, COL8_FFFFFF, COL8_000000, s, 1);
                        cursor_x += 8;
                    }
                }
            }
            // カーソル再表示
            boxfill8(sheet->buffer, sheet->width, cursor_c, cursor_x, 28, cursor_x + 7, 43);
            sheet_refresh(sheet, cursor_x, 28, cursor_x + 8, 44);
        }
    }
}
