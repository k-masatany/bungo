// コンソール関係
#include <stdio.h>
#include <string.h>
#include "bootpack.h"

void console_task(struct SHEET *sheet, unsigned int total_memory) {
    int data;
    int fifo_buffer[FIFO_BUF_SIZE];
    char command_line[32];

    struct TIMER *timer;
    struct TASK *task = task_now();
    struct MEMORY_MANAGER *memory_manager = (struct MEMORY_MANAGER *) MEMORY_MANAGER_ADDRESS;
    int *fat = (int *) memman_alloc_4k(memory_manager, 4 * 2880);

    struct CONSOLE console;
    console.sheet = sheet;
    console.cursor_x =  8;
    console.cursor_y = 28;
    console.cursor_c = -1;
    *((int *) 0x0fec) = (int) &console;

    fifo32_init(&task->fifo, FIFO_BUF_SIZE, fifo_buffer, task);
    timer = timer_alloc();
    timer_init(timer, &task->fifo, 1);
    timer_set_time(timer, 50);
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
                    timer_init(timer, &task->fifo, 0);
                    if (console.cursor_c >= 0) {
                        console.cursor_c = COL8_FFFFFF;
                    }
                }
                else {
                    timer_init(timer, &task->fifo, 1);
                    if (console.cursor_c >= 0) {
                        console.cursor_c = COL8_000000;
                    }
                }
                timer_set_time(timer, 50);
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
                if (data == 8 + 0x0100) {   // BS
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
                    if (console.cursor_x < 240) {
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
            *((int *) 0x0fe8) = (int) q;
            set_segment_descriptor(gdt + 1003, f_info->size - 1, (int) p, AR_CODE32_ER + 0x60);
            set_segment_descriptor(gdt + 1004, segment_size - 1, (int) q, AR_DATA32_RW + 0x60);
            for (i = 0; i < data_size; i++) {
                q[esp + i] = p[data_exe + i];
            }
            start_app(0x1b, 1003 * 8, esp, 1004 * 8, &(task->tss.esp0));
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
    struct CONSOLE *console = (struct CONSOLE *) *((int *) 0x0fec);
    int ds_base = *((int *) 0xfe8);
    struct TASK *task = task_now();
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
        sheet_set_buffer(sheet, (char *) ebx + ds_base, esi, edi, eax);
        make_window8((char *) ebx + ds_base, esi, edi, (char *) ecx + ds_base, 0);
        sheet_slide(sheet, 100, 50);
        sheet_updown(sheet, 3);
        reg[7] = (int) sheet;
    }
    else if (edx == 6) {
        sheet = (struct SHEET *) ebx;
        putfonts8_ascii(sheet->buffer, sheet->width, esi, edi, eax, (char *) ebp + ds_base);
        sheet_refresh(sheet, esi, edi, esi + ecx * 8, edi + 16);
    }
    else if (edx == 7) {
        sheet = (struct SHEET *) ebx;
        boxfill8(sheet->buffer, sheet->width, ebp, eax, ecx, esi, edi);
        sheet_refresh(sheet, eax, ecx, esi + 1, edi + 1);
    }

    return 0;
}

int *inthandler0d(int *esp) {
    struct CONSOLE *console = (struct CONSOLE *) *((int *) 0x0fec);
    struct TASK *task = task_now();
    char s[48];

    console_putstr(console, "\n");
    sprintf(s, "General Protected Exception. EIP:%08X\n", esp[11]);
    console_putstr(console, s);
    return &(task->tss.esp0);   // 異常終了させる
}

int *inthandler0c(int *esp) {
    struct CONSOLE *console = (struct CONSOLE *) *((int *) 0x0fec);
    struct TASK *task = task_now();
    char s[48];

    console_putstr(console, "\n");
    sprintf(s, "Stack Exception. EIP:%08X\n", esp[11]);
    console_putstr(console, s);
    return &(task->tss.esp0);   // 異常終了させる
}
