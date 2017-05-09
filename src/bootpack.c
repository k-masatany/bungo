// bootpackのメイン
#include <stdio.h>
#include <string.h>
#include "bootpack.h"

#define KEY_COMMAND_LED		0xed

void switch_sheet(struct SHEET *active_window);
void leave_sheet(struct SHEET *active_window);

void BungoMain(void)
{
    int i;
    int data;
    char s[40];
    struct BOOT_INFO *boot_info = (struct BOOT_INFO *) ADR_BOOTINFO;
    struct FIFO32 fifo;
    int fifo_buffer[FIFO_BUF_SIZE];
    struct TASK *task_a;
    // マウス関係
    struct MOUSE_DECODER mouse_decoder;
    int x, y;
    int mouse_x, mouse_y;
    int move_x = -1, move_y = -1;
    // メモリ関係
    struct MEMORY_MANAGER *memory_manager = (struct MEMORY_MANAGER *) MEMORY_MANAGER_ADDRESS;
    unsigned int total_memory;
    // ウィンドウ関係
    struct SHEET_CONTROL *sheet_ctl;
    struct SHEET *sheet_back, *sheet_mouse;
    unsigned char *sheet_buffer_back, sheet_buffer_mouse[160];
    struct SHEET *active_window;
    struct SHEET *sheet = 0;
    struct TASK *task;
    // コンソール関係
    struct SHEET *sheet_console[2];
    unsigned char *sheet_buffer_console[2];
    struct TASK *task_console[2];
    int *console_fifo[2];
    // キー入力関係
    struct FIFO32 key_command;
    int key_command_buffer[KEY_CMD_BUF];
    static char key_table[0x80] = {
        0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0x08,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '@', '[', 0x0a,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', ';', ':', 0,   0,   ']', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0x5c, 0,  0,   0,   0,   0,   0,   0,   0,   0,   0x5c, 0,  0
	};
    static char shift_key_table[0x80] = {
        0,   0,   '!', 0x22, '#', '$', '%', '&', 0x27, '(', ')', '~', '=', '~', 0x08,   0,
		'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '`', '{', 0x0a,   0,   'A', 'S',
		'D', 'F', 'G', 'H', 'J', 'K', 'L', '+', '*', 0,   0,   '}', 'Z', 'X', 'C', 'V',
		'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   '7', '8', '9', '-', '4', '5', '6', '+', '1',
		'2', '3', '0', '.', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0,   '_', 0,   0,   0,   0,   0,   0,   0,   0,   0,   '|', 0,   0
	};
    static char control_key_table[0x80] = {
           0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 0x1e,    0,    0,
		0x11, 0x17, 0x05, 0x12, 0x14, 0x19, 0x15, 0x09, 0x0f, 0x10, 0x00, 0x1b, 0x0a,    0, 0x01, 0x13,
		0x04, 0x06, 0x07, 0x08, 0x0a, 0x0b, 0x0c,    0,    0,    0,    0, 0x1d, 0x1a, 0x18, 0x03, 0x16,
		0x02, 0x0e, 0x0d,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		   0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		   0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		   0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
		   0,    0, 0x1f,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0, 0x1c,    0,    0
	};
    int key_shift = 0;
    int key_control = 0;
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
    // memory test
    total_memory = memtest(0x00400000, 0xbfffffff);
    memman_init(memory_manager);
    memman_free(memory_manager, 0x00001000, 0x0009e000); // 0x00001000 - 0x0009efff
    memman_free(memory_manager, 0x00400000, total_memory - 0x00400000);

    init_palette();
    task_a = task_init(memory_manager);
    fifo.task = task_a;
    task_run(task_a, 1, 2);

    sheet_ctl = sheet_control_init(memory_manager, boot_info->vram, boot_info->screen_x, boot_info->screen_y);
    *((int *) 0x0fe4) = (int) sheet_ctl;
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
    // sheet console
    for (i = 0; i < 2; i++) {
        sheet_console[i] = sheet_alloc(sheet_ctl);
        sheet_buffer_console[i] = (unsigned char *)memman_alloc_4k(memory_manager, 256 * 165);
        sheet_set_buffer(sheet_console[i], sheet_buffer_console[i], 256, 165, -1);
        make_window8(sheet_buffer_console[i], 256, 165, "console", 0);
        make_textbox8(sheet_console[i], 8, 28, 240, 128, COL8_000000);
        task_console[i] = task_alloc();
        task_console[i]->tss.esp = memman_alloc_4k(memory_manager, 64 * 1024) + 64 * 1024 - 12;
        task_console[i]->tss.eip = (int) &console_task;
        task_console[i]->tss.es  = 1 * 8;
        task_console[i]->tss.cs  = 2 * 8;
        task_console[i]->tss.ss  = 1 * 8;
        task_console[i]->tss.ds  = 1 * 8;
        task_console[i]->tss.fs  = 1 * 8;
        task_console[i]->tss.gs  = 1 * 8;
        *((int *) (task_console[i]->tss.esp + 4)) = (int) sheet_console[i];
        *((int *) (task_console[i]->tss.esp + 8)) = total_memory;
        task_run(task_console[i], 2, 2);
        sheet_console[i]->task = task_console[i];
        sheet_console[i]->flags |= SHEET_ACTIVE;
        console_fifo[i] = (int *) memman_alloc_4k(memory_manager, 128 * 4);
        fifo32_init(&(task_console[i]->fifo), 128, console_fifo[i], task_console[i]);
    }

    sheet_slide(sheet_back, 0, 0);
    sheet_slide(sheet_console[0], 32, 4);
    sheet_slide(sheet_console[1], 64, 36);
    sheet_slide(sheet_mouse,  mouse_x, mouse_y);
    sheet_updown(sheet_back,    0);
    sheet_updown(sheet_console[0], 1);
    sheet_updown(sheet_console[1], 2);
    sheet_updown(sheet_mouse,   3);
    active_window = sheet_console[0];
    switch_sheet(active_window);

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
            if (active_window->flags == 0) {    // アクティブウィンドウが閉じられた
                active_window = sheet_ctl->sheets_head[sheet_ctl->top - 1];
                switch_sheet(active_window);
            }
            // キーボード操作
            if(0x0100 <= data && data < 0x0200) {
                if (data < 0x80 + 0x0100) {
                    if (key_shift == 0) {
                        if (key_control == 0) {
                            s[0] = key_table[data - 0x0100];
                        }
                        else {
                            s[0] = control_key_table[data - 0x0100];
                        }
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
                // 通常文字, BS, Enter
                if (s[0] != 0) {
                    fifo32_put(&(active_window->task->fifo), s[0] + 0x0100);
                }
                // TAB
                if (data == 0x0100 + 0x0f) {
                    leave_sheet(active_window);
                    i = active_window->layer - 1;
                    if (i == 0) {
                        i = sheet_ctl->top - 1;
                    }
                    active_window = sheet_ctl->sheets_head[i];
                    switch_sheet(active_window);
                }
                // F6
                if (data == 0x0100 + 0x40) {
                    sheet_updown(sheet_ctl->sheets_head[1], sheet_ctl->top - 1);
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
                // 左コントロールON
                if (data == 0x0100 + 0x1d) {
                    key_control |= 1;
                }
                // 左コントロールOFF
                if (data == 0x0100 + 0x9d) {
                    key_control &= ~1;
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
                // Ctrl+C
                if (s[0] == 0x03) {
                    task = active_window->task;
                    console_putstr(task->console, "^C");
                    if (task->tss.ss0 != 0) {
                        io_cli();
                        task->tss.eax = (int) &(task->tss.esp0);
                        task->tss.eip = (int) asm_end_app;
                        io_sti();
                    }
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
            }
            // マウス操作
            else if (0x0200 <= data && data < 0x0300) {
                if (mouse_decode(&mouse_decoder, data - 0x0200) != 0) {
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
                    sheet_slide(sheet_mouse, mouse_x, mouse_y);
                    if ((mouse_decoder.button & 0x01) != 0) {   // 左クリック
                        if (move_x < 0) {   // 通常モード
                            // 上のシートから順番にマウスがクリックしたシートを探す
                            // sheet_ctl->topはマウスカーソル、0は背景なので検索対象から外す
                            for (i = sheet_ctl->top - 1; i > 0; i-- ) {
                                sheet = sheet_ctl->sheets_head[i];
                                x = mouse_x - sheet->x;
                                y = mouse_y - sheet->y;
                                if (0 <= x && x < sheet->width && 0 <= y && y < sheet->height) {
                                    if (sheet->buffer[y * sheet->width + x] != sheet->color_invisible) {
                                        sheet_updown(sheet, sheet_ctl->top - 1);
                                        if (sheet != active_window) {
                                            leave_sheet(active_window);
                                            active_window = sheet;
                                            switch_sheet(active_window);
                                        }
                                        if (3 <= x && x < sheet->width - 3 && 3 <= y && y < 21) {  // ウィンドウ移動
                                            move_x = mouse_x;
                                            move_y = mouse_y;
                                        }
                                        if (sheet->width - 21 <= x && x < sheet->width - 5 && 5 <= y && y < 19) { // 閉じるボタン
                                            if ((sheet->flags & SHEET_AUTO_CLOSE) != 0) {
                                                task = active_window->task;
                                                console_putstr(task->console, "^C(mouse)\n");
                                                io_cli();
                                                task->tss.eax = (int) &(task->tss.esp0);
                                                task->tss.eip = (int) asm_end_app;
                                                io_sti();
                                            }
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                        else {  // ウィンドウ移動モード
                            x = mouse_x - move_x;
                            y = mouse_y - move_y;
                            sheet_slide(sheet, sheet->x + x, sheet->y + y);
                            // 移動後の座標に更新
                            move_x = mouse_x;
                            move_y = mouse_y;
                        }
                    }
                    else {
                        move_x = -1;
                    }
                }
            }
        }
    }
}

void switch_sheet(struct SHEET *active_window) {
    change_window_title_color(active_window, 1);
    if ((active_window->flags & SHEET_ACTIVE) != 0 ) {
        fifo32_put(&(active_window->task->fifo), 2);     // コンソールのカーソルOFF
    }
    return;
}

void leave_sheet(struct SHEET *active_window) {
    change_window_title_color(active_window, 0);
    if ((active_window->flags & SHEET_ACTIVE) != 0) {
        fifo32_put(&(active_window->task->fifo), 3);     // コンソールのカーソルOFF
    }
    return;
}
