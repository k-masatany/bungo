// コンソール関係
#include <stdio.h>
#include <string.h>
#include "bootpack.h"

int  console_newline(int cursor_y, struct SHEET *sheet);

void console_task(struct SHEET *sheet, unsigned int total_memory) {
    struct TIMER *timer;
    struct TASK *task = task_now();
    struct FILE_INFO *f_info = (struct FILE_INFO *) (ADR_DISKIMG + 0x002600);
    struct MEMORY_MANAGER *memory_manager = (struct MEMORY_MANAGER *) MEMORY_MANAGER_ADDRESS;
    struct SEGMENT_DESCRIPTOR *gdt = (struct SEGMENT_DESCRIPTOR *) ADR_GDT;

    int data;
    int fifo_buffer[FIFO_BUF_SIZE];
    int cursor_x = 16;
    int cursor_y = 28;
    int cursor_c = -1;
    int x, y;
    int *fat = (int *) memman_alloc_4k(memory_manager, 4 * 2880);
    char s[32];
    char command_line[32];
    char *p;

    fifo32_init(&task->fifo, FIFO_BUF_SIZE, fifo_buffer, task);
    timer = timer_alloc();
    timer_init(timer, &task->fifo, 1);
    timer_set_time(timer, 50);
    file_read_fat(fat, (unsigned char *) (ADR_DISKIMG + 0x000200));

    // プロンプト表示
    putfonts8_ascii_sheet(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "$", 1);


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
                    if (cursor_c >= 0) {
                        cursor_c = COL8_FFFFFF;
                    }
                }
                else {
                    timer_init(timer, &task->fifo, 1);
                    if (cursor_c >= 0) {
                        cursor_c = COL8_000000;
                    }
                }
                timer_set_time(timer, 50);
            }
            if (data == 2) {
                cursor_c = COL8_FFFFFF;
            }
            if (data == 3) {
                boxfill8(sheet->buffer, sheet->width, COL8_000000, cursor_x, cursor_y, cursor_x + 7, cursor_y + 15);
                cursor_c = -1;
            }
            if (0x0100 <= data && data < 0x0200) {  // キーボードデータ（タスクA経由）
                if (data == 8 + 0x0100) {   // BS
                    if (cursor_x > 16) {
                        // カーソルをスペースで消し、カーソルを1つ戻す
                        s[0] = data - 0x0100;
                        s[1] = 0;
                        putfonts8_ascii_sheet(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
                        cursor_x -= 8;
                    }
                }
                else if (data == 0x0a + 0x0100) {    // LF
                    // カーソルをスペースで消す
                    putfonts8_ascii_sheet(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
                    command_line[cursor_x / 8 - 2] = 0;
                    cursor_y = console_newline(cursor_y, sheet);
                    // コマンド実行
                    if (strcmp(command_line, "free") == 0) {
                        // 似非freeコマンド
                        sprintf(s, "total: %dMB", total_memory /1024/1024);
                        putfonts8_ascii_sheet(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, strlen(s));
                        cursor_y = console_newline(cursor_y, sheet);
                        sprintf(s, "free : %dMB", memman_total(memory_manager) /1024/1024);
                        putfonts8_ascii_sheet(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, strlen(s));
                        cursor_y = console_newline(cursor_y, sheet);
                    }
                    else if (strcmp(command_line, "clear") == 0) {
                        for (y = 28; y < 28 + 128; y++) {
                            for (x = 8; x< 8 + 240; x++) {
                                sheet->buffer[x + y * sheet->width] = COL8_000000;
                            }
                        }
                        sheet_refresh(sheet, 8, 28, 8 + 240, 28 + 128);
                        cursor_y = 28;
                    }
                    else if (strcmp(command_line, "ls") == 0) {
                        for (x = 0; x < 224; x++) {
                            if (f_info[x].name[0] == 0x00) {
                                break;
                            }
                            if (f_info[x].name[0] != 0xe5) {
                                if ((f_info[x].type & 0x18) == 0) {
                                    sprintf(s, "filename.ext %7d", f_info[x].size);
                                    for (y = 0; y < 8; y++) {
                                        s[y] = f_info[x].name[y];
                                    }
                                    s[9]  = f_info[x].ext[0];
                                    s[10] = f_info[x].ext[1];
                                    s[11] = f_info[x].ext[2];
                                    putfonts8_ascii_sheet(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, strlen(s));
                                    cursor_y = console_newline(cursor_y, sheet);
                                }
                            }
                        }
                    }
                    else if (strncmp(command_line, "cat ", 4) == 0) {
                        // ファイル名を準備する
                        for (y = 0; y < 11; y++) {
							s[y] = ' ';
						}
						y = 0;
						for (x = 4; y < 11 && command_line[x] != 0; x++) {
							if (command_line[x] == '.' && y <= 8) {
								y = 8;
							} else {
								s[y] = command_line[x];
								if ('a' <= s[y] && s[y] <= 'z') {
									// 小文字は大文字に直す
									s[y] -= 0x20;
								}
								y++;
							}
						}
						// ファイルを探す
						for (x = 0; x < 224; ) {
							if (f_info[x].name[0] == 0x00) {
								break;
							}
							if ((f_info[x].type & 0x18) == 0) {
								for (y = 0; y < 11; y++) {
									if (f_info[x].name[y] != s[y]) {
										goto cat_next_file;
									}
								}
								break; // ファイルが見つかった
							}
		cat_next_file:
							x++;
						}
						if (x < 224 && f_info[x].name[0] != 0x00) {
							// ファイルが見つかった場合
							y = f_info[x].size;
							p = (char *) memman_alloc_4k(memory_manager, f_info[x].size);
                            file_load_file(f_info[x].cluster_no, f_info[x].size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
							cursor_x = 8;
							for (y = 0; y < f_info[x].size; y++) {
								// 1文字ずつ出力
								s[0] = p[y];
								s[1] = 0;
                                if (s[0] == 0x09) { // TAB
                                    for (;;) {
                                        putfonts8_ascii_sheet(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, " ", 1);
                                        cursor_x += 8;
                                        if (cursor_x == 8 + 240) {
                                            cursor_x = 8;
                                            cursor_y = console_newline(cursor_y, sheet);
                                        }
                                        if (((cursor_x - 8) & 0x1f) == 0) {
                                            break;
                                        }
                                    }
                                }
                                else if (s[0] == 0x0a) {    // LF
                                    cursor_x = 8;
                                    cursor_y = console_newline(cursor_y, sheet);
                                }
                                else if (s[0] == 0x0d) {    // CR(無視)

                                }
                                else {  // 普通の文字
    								putfonts8_ascii_sheet(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, s, 1);
    								cursor_x += 8;
    								if (cursor_x == 8 + 240) {
    									cursor_x = 8;
    									cursor_y = console_newline(cursor_y, sheet);
    								}
                                }
							}
                            memman_free_4k(memory_manager, (int) p, f_info[x].size);
						} else {
							// ファイルが見つからなかった場合
                            sprintf(s, "%s : File not found.", &(command_line[4]));
							putfonts8_ascii_sheet(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, strlen(s));
							cursor_y = console_newline(cursor_y, sheet);
						}
                    }
                    else if (strcmp(command_line, "halt") == 0) {
						// hltアプリケーションを起動
						for (y = 0; y < 11; y++) {
							s[y] = ' ';
						}
						s[ 0] = 'H';
                        s[ 1] = 'A';
						s[ 2] = 'L';
						s[ 3] = 'T';
						s[ 8] = 'E';
						s[ 9] = 'X';
						s[10] = 'E';
						for (x = 0; x < 224; ) {
							if (f_info[x].name[0] == 0x00) {
								break;
							}
							if ((f_info[x].type & 0x18) == 0) {
								for (y = 0; y < 11; y++) {
									if (f_info[x].name[y] != s[y]) {
										goto hlt_next_file;
									}
								}
								break; /* ファイルが見つかった */
							}
		hlt_next_file:
							x++;
						}
						if (x < 224 && f_info[x].name[0] != 0x00) {
							/* ファイルが見つかった場合 */
							p = (char *) memman_alloc_4k(memory_manager, f_info[x].size);
							file_load_file(f_info[x].cluster_no, f_info[x].size, p, fat, (char *) (ADR_DISKIMG + 0x003e00));
							set_segment_descriptor(gdt + 1003, f_info[x].size - 1, (int) p, AR_CODE32_ER);
							farjmp(0, 1003 * 8);
							memman_free_4k(memory_manager, (int) p, f_info[x].size);
						} else {
							/* ファイルが見つからなかった場合 */
							putfonts8_ascii_sheet(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "File not found.", 15);
							cursor_y = console_newline(cursor_y, sheet);
						}
                    }
                    else if (command_line[0] != 0) {
                        // コマンドではなく、空行でもない
                        sprintf(s, "%s : command not found", command_line);
                        putfonts8_ascii_sheet(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, s, strlen(s));
                        cursor_y = console_newline(cursor_y, sheet);
                    }
                    // プロンプト表示
                    putfonts8_ascii_sheet(sheet, 8, cursor_y, COL8_FFFFFF, COL8_000000, "$", 1);
                    cursor_x = 16;
                }
                else {
                    if (cursor_x < 240) {
                        // 1文字表示してから、カーソルを1つ進める
                        s[0] = data - 0x0100;
                        s[1] = 0;
                        command_line[cursor_x / 8 - 2] = s[0];
                        putfonts8_ascii_sheet(sheet, cursor_x, cursor_y, COL8_FFFFFF, COL8_000000, s, 1);
                        cursor_x += 8;
                    }
                }
            }
            // カーソル再表示
            if (cursor_c >= 0) {
                boxfill8(sheet->buffer, sheet->width, cursor_c, cursor_x, cursor_y, cursor_x + 7, cursor_y + 15);
            }
            sheet_refresh(sheet, cursor_x, cursor_y, cursor_x + 8, cursor_y + 16);
        }
    }
}

int  console_newline(int cursor_y, struct SHEET *sheet) {
    int x, y;
    if (cursor_y < 28 + 112) {  // 次の行へ
        return cursor_y + 16;
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
    return cursor_y;
}
