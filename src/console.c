// コンソール関係
#include <stdio.h>
#include <string.h>
#include "bootpack.h"

void console_task(struct SHEET *sheet, unsigned int total_memory) {
    int data;
    char command_line[32];

    struct TASK *task = task_now();
    struct MEMORY_MANAGER *memory_manager = (struct MEMORY_MANAGER *) MEMORY_MANAGER_ADDRESS;
    int *fat = (int *) memman_alloc_4k(memory_manager, 4 * 2880);

    struct CONSOLE console;
    console.sheet = sheet;
    console.cursor_x =  8;
    console.cursor_y = 28;
    console.cursor_c = -1;
    task->console =  &console;

    console.timer = timer_alloc();
    timer_init(console.timer, &task->fifo, 1);
    timer_set_time(console.timer, 50);
    file_read_fat(fat, (unsigned char *) (ADR_DISKIMG + 0x000200));

    // プロンプト表示
    console_putchar(&console, '>', 1);

    for (;;) {
        io_cli();
        if (fifo32_status(&task->fifo) == 0) {
            task_sleep(task);
            io_sti();
        }
        else {
            data = fifo32_get(&task->fifo);
            io_sti();
            if (data <= 1) {    // カーソル用タイマ処理
                if (data != 0) {
                    timer_init(console.timer, &task->fifo, 0);
                    if (console.cursor_c >= 0) {
                        console.cursor_c = COL8_FFFFFF;
                    }
                }
                else {
                    timer_init(console.timer, &task->fifo, 1);
                    if (console.cursor_c >= 0) {
                        console.cursor_c = COL8_000000;
                    }
                }
                timer_set_time(console.timer, 50);
            }
            if (data == 2) {    // カーソルON
                console.cursor_c = COL8_FFFFFF;
            }
            if (data == 3) {    // カーソルOFF
                boxfill8(console.sheet->buffer, console.sheet->width, COL8_000000,
                            console.cursor_x, console.cursor_y, console.cursor_x + 7, console.cursor_y + 15);
                console.cursor_c = -1;
            }
            // キーボードデータ（タスクA経由）処理
            if (0x0100 <= data && data < 0x0200) {
                if (data == 0x08 + 0x0100) {   // BS
                    if (console.cursor_x > 16) {
                        // カーソルをスペースで消し、カーソルを1つ戻す
                        console_putchar(&console, ' ', 0);
                        console.cursor_x -= 8;
                    }
                }
                else if (data == 0x0a + 0x0100) {    // LF
                    // カーソルをスペースで消す
                    console_putchar(&console, ' ', 0);
                    command_line[console.cursor_x / 8 - 2] = 0;
                    console_newline(&console);
                    console_run_command(command_line, &console, fat, total_memory);
                    // プロンプト表示
                    console_putchar(&console, '>', 1);
                }
                else {
                    if (0x1f < (data - 0x0100) && console.cursor_x < 240) { // 制御文字は表示しない
                        // 1文字表示してから、カーソルを1つ進める
                        command_line[console.cursor_x / 8 - 2] = data - 0x0100;
                        console_putchar(&console, data - 0x0100, 1);
                    }
                }
            }
            // カーソル再表示
            if (console.cursor_c >= 0) {
                boxfill8(sheet->buffer, sheet->width, console.cursor_c, console.cursor_x, console.cursor_y, console.cursor_x + 7, console.cursor_y + 15);
            }
            sheet_refresh(sheet, console.cursor_x, console.cursor_y, console.cursor_x + 8, console.cursor_y + 16);
        }
    }
}

void console_putchar(struct CONSOLE *console, int c, char move) {
    char s[2];
    s[0] = c;
    s[1] = 0;
    if (s[0] == 0x09) { // TAB
        for (;;) {
            putfonts8_ascii_sheet(console->sheet, console->cursor_x, console->cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
            console->cursor_x += 8;
            if (console->cursor_x == 8 + 240) {
                console_newline(console);
            }
            if (((console->cursor_x - 8) & 0x1f) == 0) {
                break;
            }
        }
    }
    else if (s[0] == 0x0a) {    // LF
        console_newline(console);
    }
    else if (s[0] == 0x0d) {    // CR
        // (無視)
    }
    else {  // 普通の文字
        putfonts8_ascii_sheet(console->sheet, console->cursor_x, console->cursor_y, COL8_FFFFFF, COL8_000000, s, 1);
        // moveが0の時はカーソルを進めない
        if (move != 0) {
            console->cursor_x += 8;
            if (console->cursor_x == 8 + 240) {
            console_newline(console);
            }
        }
    }
}

void console_putstr(struct CONSOLE *console, char *str) {
    for (; *str != 0; str++) {
        console_putchar(console, *str, 1);
    }
    return;
}

void console_putnstr(struct CONSOLE *console, char *str, int n) {
    int i;
    for (i = 0; i < n; i++) {
        console_putchar(console, str[i], 1);
    }
    return;
}

void  console_newline(struct CONSOLE *console) {
    int x, y;
    struct SHEET *sheet = console->sheet;
    if (console->cursor_y < 28 + 112) {  // 次の行へ
        console->cursor_y += 16;
    }
    else {  // スクロール
        for (y = 28; y < 28 + 112; y++) {
            for (x = 8; x < 8 + 240; x++) {
                sheet->buffer[x + y * sheet->width] = sheet->buffer[x + (y+16) * sheet->width];
            }
        }
        for (y = 28 + 112; y < 28 + 128; y++) {
            for (x = 8; x < 8 + 240; x++) {
                sheet->buffer[x + y * sheet->width] = COL8_000000;
            }
        }
        sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
    }
    console->cursor_x = 8;
    return;
}

void console_run_command(char *command_line, struct CONSOLE *console, int *fat, unsigned int total_memory) {
    char s[32];

    if (strcmp(command_line, "free") == 0) {
        command_free(console, total_memory);
    }
    else if (strcmp(command_line, "clear") == 0) {
        command_clear(console);
    }
    else if (strcmp(command_line, "ls") == 0) {
        command_ls(console);
    }
    else if (strncmp(command_line, "cat ", 4) == 0) {
        command_cat(console, fat, command_line);
    }
    else if (command_line[0] != 0) {
        if (command_app(console, fat, command_line) == 0) {
            // コマンドではなく、空行でもない
            sprintf(s, "%s : command not found\n", command_line);
            console_putstr(console, s);
        }
        if (console->cursor_x > 8) {
            console_newline(console);
        }
    }
    return;
}

void command_free(struct CONSOLE *console, unsigned int total_memory) {
    struct MEMORY_MANAGER *memory_manager = (struct MEMORY_MANAGER *) MEMORY_MANAGER_ADDRESS;
    char s[32];
    // 似非freeコマンド
    sprintf(s, "total: %11dMB\n", total_memory /1024/1024);
    console_putstr(console, s);
    sprintf(s, "free : %11dMB\n", memman_total(memory_manager) /1024/1024);
    console_putstr(console, s);
    return;
}

void command_clear(struct CONSOLE *console) {
    int x, y;
    struct SHEET *sheet = console->sheet;

    for (y = 28; y < 28 + 128; y++) {
        for (x = 8; x< 8 + 240; x++) {
            sheet->buffer[x + y * sheet->width] = COL8_000000;
        }
    }
    sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
    console->cursor_y = 28;

    return;
}

void command_ls(struct CONSOLE *console) {
    struct FILE_INFO *f_info = (struct FILE_INFO *) (ADR_DISKIMG + 0x002600);
    int i, j;
    char s[32];

    for (i = 0; i < 224; i++) {
        if (f_info[i].name[0] == 0x00) {
            break;
        }
        if (f_info[i].name[0] != 0xe5) {
            if ((f_info[i].type & 0x18) == 0) {
                sprintf(s, "filename.ext %7d\n", f_info[i].size);
                for (j = 0; j < 8; j++) {
                    s[j] = f_info[i].name[j];
                }
                s[9]  = f_info[i].ext[0];
                s[10] = f_info[i].ext[1];
                s[11] = f_info[i].ext[2];
                console_putstr(console, s);
            }
        }
    }
    return;
}

void command_cat(struct CONSOLE *console, int *fat, char *command_line) {
    struct MEMORY_MANAGER *memory_manager = (struct MEMORY_MANAGER *) MEMORY_MANAGER_ADDRESS;
    struct FILE_INFO *f_info = file_search(command_line + 4, (struct FILE_INFO *) (ADR_DISKIMG + 0x002600), 224);
    char *p;
    char s[32];

    if (f_info != 0) { // ファイルが見つかった場合
        p = (char *) memman_alloc_4k(memory_manager, f_info->size);
        file_load_file(f_info->cluster_no, f_info->size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
        console_putnstr(console, p, f_info->size);
        memman_free_4k(memory_manager, (int) p, f_info->size);
    }
    else {             // ファイルが見つからなかった場合
        sprintf(s, "%s : not found.", command_line + 4);
        console_putstr(console, s);
    }
    console_newline(console);
    return;
}

int  command_app(struct CONSOLE *console, int *fat, char *command_line) {
    struct MEMORY_MANAGER *memory_manager = (struct MEMORY_MANAGER *) MEMORY_MANAGER_ADDRESS;
    struct FILE_INFO *f_info;
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;
    struct TASK *task = task_now();
    struct SHEET_CONTROL *sheet_ctl;
    struct SHEET *sheet;
    char filename[16];
    char *p;
    char *q;
    int i;
    int segment_size;
    int data_size;
    int esp;
    int data_exe;

    // コマンドラインからファイル名を生成
    for (i = 0; i < 13; i++) {
        if (command_line[i] <= ' ') {
            break;
        }
        filename[i] = command_line[i];
    }
    filename[i] = 0;    // とりあえずファイル名の後ろを終端にする

    // ファイルを探す
    f_info = file_search(filename, (struct FILE_INFO *) (ADR_DISKIMG + 0x002600), 224);
    if (f_info == 0 && filename[i - 1] != '.') {
        // 見つからなかったので、'.EXE'を末尾につけて検索
        filename[i    ] = '.';
        filename[i + 1] = 'E';
        filename[i + 2] = 'X';
        filename[i + 3] = 'E';
        filename[i + 4] = 0;
        f_info = file_search(filename, (struct FILE_INFO *) (ADR_DISKIMG + 0x002600), 224);
    }

    if (f_info != 0) { // ファイルが見つかった場合
        /* ファイルが見つかった場合 */
        p = (char *) memman_alloc_4k(memory_manager, f_info->size);
        *((int *) 0xfe8) = (int) p;
        file_load_file(f_info->cluster_no, f_info->size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
        if (f_info->size >= 36 && strncmp(p + 4, "Hari", 4) == 0 && *p == 0x00) {
            segment_size = *((int *) (p + 0x0000));
            esp          = *((int *) (p + 0x000c));
            data_size    = *((int *) (p + 0x0010));
            data_exe     = *((int *) (p + 0x0014));
            q = (char *) memman_alloc_4k(memory_manager, segment_size);
            task->ds_base = (int) q;
            set_segment_descriptor(gdt + (task->selector / 8) + 1000, f_info->size - 1, (int) p, AR_CODE32_ER + 0x60);
            set_segment_descriptor(gdt + (task->selector / 8) + 2000, segment_size - 1, (int) q, AR_DATA32_RW + 0x60);
            for (i = 0; i < data_size; i++) {
                q[esp + i] = p[data_exe + i];
            }
            start_app(0x1b, task->selector + 1000*8, esp, task->selector + 2000*8, &(task->tss.esp0));
            sheet_ctl = (struct SHEET_CONTROL *) *((int *) 0x0fe4);
            for (i = 0; i < MAX_SHEETS; i++) {
                sheet = &(sheet_ctl->sheets[i]);
                if ((sheet->flags & (SHEET_AUTO_CLOSE | SHEET_USE)) == (SHEET_AUTO_CLOSE | SHEET_USE)
                        && sheet->task == task) {
                    // アプリが開きっぱなしのシートを発見
                    sheet_free(sheet);
                }
            }
            timer_cancel_all(&(task->fifo));
            memman_free_4k(memory_manager, (int) q, segment_size);
        }
        else {
            console_putstr(console, "Error: Unknown Exec Format.");
        }
        memman_free_4k(memory_manager, (int) p, f_info->size);
        return 1;
    }
    // ファイルが見つからなかった場合
    return 0;
}

int *exec_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx, int eax) {
    struct TASK *task = task_now();
    struct CONSOLE *console = task->console;
    int ds_base = task->ds_base;
    int key_data;
    int d;
    struct SHEET_CONTROL *sheet_ctl = (struct SHEET_CONTROL *) *((int *) 0x0fe4);
    struct SHEET *sheet;
    int *reg = &eax + 1; // EAXの次の番地
        // 保存のため、PUSHADを強引に書き換える
        // reg[0] : EDI,  reg[1] : ESI,  reg[2] : EBP,  reg[3] : ESP
        // reg[4] : EBX,  reg[5] : EDX,  reg[6] : ECX,  reg[7] : EAX

    if (edx == 1) {
        console_putchar(console, eax & 0xff, 1);
    }
    else if (edx == 2) {
        console_putstr(console, (char *) ebx + ds_base);
    }
    else if (edx == 3) {
        console_putnstr(console, (char *) ebx + ds_base, ecx);
    }
    else if (edx == 4) {
        return &(task->tss.esp0);
    }
    else if (edx == 5) {
        sheet = sheet_alloc(sheet_ctl);
        sheet->task = task;
        sheet->flags |= SHEET_AUTO_CLOSE;
        sheet_set_buffer(sheet, (char *) ebx + ds_base, esi, edi, eax);
        make_window8((char *) ebx + ds_base, esi, edi, (char *) ecx + ds_base, 0);
        sheet_slide(sheet, (sheet_ctl->screen_x - esi) / 2, (sheet_ctl->screen_y - edi) / 2);
        sheet_updown(sheet, sheet_ctl->top);    // マウスと同じレイヤ（マウスはこの上のレイヤになる）
        reg[7] = (int) sheet;
    }
    else if (edx == 6) {
        sheet = (struct SHEET *) (ebx & 0xfffffffe);
        putfonts8_ascii(sheet->buffer, sheet->width, esi, edi, eax, (char *) ebp + ds_base);
        if ((ebx & 1) == 0) {
            sheet_refresh(sheet, esi, edi, esi + ecx * 8, edi + 16);
        }
    }
    else if (edx == 7) {
        sheet = (struct SHEET *) (ebx & 0xfffffffe);
        boxfill8(sheet->buffer, sheet->width, ebp, eax, ecx, esi, edi);
        if ((ebx & 1) == 0) {
            sheet_refresh(sheet, eax, ecx, esi + 1, edi + 1);
        }
    }
    else if (edx == 8) {
        memman_init((struct MEMORY_MANAGER *) (ebx + ds_base));
        ecx &= 0xfffffff0;  // 16バイト単位に
        memman_free((struct MEMORY_MANAGER *) (ebx + ds_base), eax, ecx);
    }
    else if (edx == 9) {
        ecx = (ecx + 0x0f) & 0xfffffff0; // 16バイト単位に切り上げる
        reg[7] = memman_alloc((struct MEMORY_MANAGER *) (ebx + ds_base), ecx);
    }
    else if (edx == 10) {
        ecx = (ecx + 0x0f) & 0xfffffff0; // 16バイト単位に切り上げる
        memman_free((struct MEMORY_MANAGER *) (ebx + ds_base), eax, ecx);
    }
    else if (edx == 11) {
        sheet = (struct SHEET *) (ebx & 0xfffffffe);
        sheet->buffer[sheet->width * edi + esi] = eax;
        if ((ebx & 1) == 0) {
            sheet_refresh(sheet, esi, edi, esi + 1, edi +1);
        }
    }
    else if (edx == 12) {
        sheet = (struct SHEET *) ebx;
        sheet_refresh(sheet, eax, ecx, esi, edi);
    }
    else if (edx == 13) {
        sheet = (struct SHEET *) (ebx & 0xfffffffe);
        exec_api_draw_line(sheet, eax, ecx, esi, edi, ebp);
        if ((ebx & 1) == 0) {
            sheet_refresh(sheet, eax, ecx, esi + 1, edi + 1);
        }
    }
    else if (edx == 14) {
        sheet_free((struct SHEET *) ebx);
    }
    else if (edx == 15) {
        for (;;) {
            io_cli();
            if (fifo32_status(&(task->fifo)) == 0 ) {
                if (eax != 0) {
                    task_sleep(task);   // FIFOが空ならスリープ
                }
                else {
                    io_sti();
                    reg[7] = -1;
                    return 0;
                }
            }
            key_data = fifo32_get(&(task->fifo));
            io_sti();
            if (key_data <= 1) {    // カーソル用タイマ
                // アプリ実行中はカーソルが出ないので
                // 常に表示用の1を次の値にセットしておく
                timer_init(console->timer, &(task->fifo), 1);
                timer_set_time(console->timer, 50);
            }
            if (key_data == 2) {    // カーソルON
                console->cursor_c = COL8_FFFFFF;
            }
            if (key_data == 2) {    // カーソルOFF
                console->cursor_c = -1;
            }
            if (key_data >= 0x0100) {  // キーボードデータ（タスクA経由）
                reg[7] = key_data - 0x0100;
                return 0;
            }
        }
    }
    else if (edx == 16) {
        reg[7] = (int) timer_alloc();
        ((struct TIMER *) reg[7])->flags2 = 1;   // タイマの自動停止ON
    }
    else if (edx == 17) {
        timer_free((struct TIMER *) ebx);
    }
    else if (edx == 18) {
        timer_init((struct TIMER *) ebx, &(task->fifo), eax + 0x0100);
    }
    else if (edx == 19) {
        timer_set_time((struct TIMER *) ebx, eax);
    }
    else if (edx == 20) {
        if (eax == 0) {  // mute
            d = io_in8(0x61);
            io_out8(0x61, d & 0x0d);
        }
        else {
            d = 1193180000 / eax;
            io_out8(0x43, 0xb6);
            io_out8(0x42, d & 0xff);
            io_out8(0x42, d >> 8);
            d = io_in8(0x61);
            io_out8(0x61, (d | 0x03) & 0x0f);
        }
    }
    return 0;
}

void exec_api_draw_line(struct SHEET *sheet, int x0, int y0, int x1, int y1, int color) {
    int i;
    int x, y;
    int len;
    int dx, dy;

    dx = x1 - x0;
	dy = y1 - y0;
	x = x0 << 10;
	y = y0 << 10;
	if (dx < 0) {
		dx = - dx;
	}
	if (dy < 0) {
		dy = - dy;
	}
	if (dx >= dy) {
		len = dx + 1;
		if (x0 > x1) {
			dx = -1024;
		} else {
			dx =  1024;
		}
		if (y0 <= y1) {
			dy = ((y1 - y0 + 1) << 10) / len;
		} else {
			dy = ((y1 - y0 - 1) << 10) / len;
		}
	} else {
		len = dy + 1;
		if (y0 > y1) {
			dy = -1024;
		} else {
			dy =  1024;
		}
		if (x0 <= x1) {
			dx = ((x1 - x0 + 1) << 10) / len;
		} else {
			dx = ((x1 - x0 - 1) << 10) / len;
		}
	}

	for (i = 0; i < len; i++) {
		sheet->buffer[(y >> 10) * sheet->width + (x >> 10)] = color;
		x += dx;
		y += dy;
	}

	return;
}

int *inthandler0d(int *esp) {
    struct TASK *task = task_now();
    struct CONSOLE *console = (struct CONSOLE *) task->console;
    char s[48];

    console_putstr(console, "\n");
    sprintf(s, "General Protected Exception. EIP:%08X\n", esp[11]);
    console_putstr(console, s);
    return &(task->tss.esp0);   // 異常終了させる
}

int *inthandler0c(int *esp) {
    struct TASK *task = task_now();
    struct CONSOLE *console = (struct CONSOLE *) task->console;
    char s[48];

    console_putstr(console, "\n");
    sprintf(s, "Stack Exception. EIP:%08X\n", esp[11]);
    console_putstr(console, s);
    return &(task->tss.esp0);   // 異常終了させる
}
