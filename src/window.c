// window関係
#include "bootpack.h"

void make_window8(unsigned char *buffer, int width, int height, char *title, char isActive) {
    boxfill8(buffer, width, COL8_C6C6C6, 0,         0,          width - 1, 0         );
	boxfill8(buffer, width, COL8_FFFFFF, 1,         1,          width - 2, 1         );
	boxfill8(buffer, width, COL8_C6C6C6, 0,         0,          0,         height - 1);
	boxfill8(buffer, width, COL8_FFFFFF, 1,         1,          1,         height - 2);
	boxfill8(buffer, width, COL8_848484, width - 2, 1,          width - 2, height - 2);
	boxfill8(buffer, width, COL8_000000, width - 1, 0,          width - 1, height - 1);
	boxfill8(buffer, width, COL8_C6C6C6, 2,         2,          width - 3, height - 3);
	boxfill8(buffer, width, COL8_848484, 1,         height - 2, width - 2, height - 2);
	boxfill8(buffer, width, COL8_000000, 0,         height - 1, width - 1, height - 1);
    make_window_title(buffer, width, title, isActive);
    return;
}

void make_window_title(unsigned char *buffer, int width, char *title, char isActive) {
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

    if (isActive != 0) {
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

void change_window_title_color(struct SHEET *sheet, char isActive) {
    int x, y, width = sheet->width;
    char c;
    char title_fgColor_old, title_bgColor_old, title_fgColor_new, title_bgColor_new;
    char *buffer = sheet->buffer;

    if (isActive != 0) {
        title_fgColor_new = COL8_FFFFFF;
        title_bgColor_new = COL8_000084;
        title_fgColor_old = COL8_C6C6C6;
        title_bgColor_old = COL8_848484;
    }
    else {
        title_fgColor_new = COL8_C6C6C6;
        title_bgColor_new = COL8_848484;
        title_fgColor_old = COL8_FFFFFF;
        title_bgColor_old = COL8_000084;
    }
    for (y = 3; y <= 20; y++) {
        for (x = 3; x <= width - 4; x++) {
            c = buffer[y * width + x];
            if (c == title_fgColor_old && x <= width - 22) {
                c = title_fgColor_new;
            }
            else if (c == title_bgColor_old) {
                c = title_bgColor_new;
            }
            buffer[y * width + x] = c;
        }
    }
    sheet_refresh(sheet, 3, 3, width, 21);
    return;
}
