// マウスやウィンドウの重ね合わせ処理関係
#include "bootpack.h"

#define SHEET_USE		1

struct SHEET_CONTROL *sheet_control_init(struct MEMORY_MANAGER *mem_manager, unsigned char *vram, int screen_x, int screen_y ) {
    struct SHEET_CONTROL *sheet_ctl;
    int i;
    sheet_ctl = (struct SHEET_CONTROL *)memman_alloc_4k(mem_manager, sizeof(struct SHEET_CONTROL));
    if (sheet_ctl == 0) {
        return sheet_ctl;
    }

    sheet_ctl->map = (unsigned char *)memman_alloc_4k(mem_manager, screen_x * screen_y);
    if (sheet_ctl->map == 0) {
        memman_free_4k(mem_manager, (int)sheet_ctl, sizeof(struct SHEET_CONTROL));
        return sheet_ctl;
    }
    sheet_ctl->vram     = vram;
    sheet_ctl->screen_x = screen_x;
    sheet_ctl->screen_y = screen_y;
    sheet_ctl->top      = -1; // シートは1枚もない
    for (i = 0; i < MAX_SHEETS; i++) {
        sheet_ctl->sheets[i].flags = 0; // 未使用マーク
        sheet_ctl->sheets[i].control = sheet_ctl;   // 所属を記録
    }
    return sheet_ctl;
}

struct SHEET *sheet_alloc(struct SHEET_CONTROL *sheet_ctl) {
    struct SHEET *sheet;
    int i;
    for (i = 0; i < MAX_SHEETS; i++) {
        if (sheet_ctl->sheets[i].flags == 0) {
            sheet = &sheet_ctl->sheets[i];
            sheet->flags    = SHEET_USE;   // 使用中に変更
            sheet->layer  = -1;            // 非表示
            return sheet;
        }
    }
    return 0;   // すべてのSHEETが使用中
}

void sheet_set_buffer(struct SHEET *sheet, unsigned char *buffer, int width, int height, int color_invisible) {
    sheet->buffer = buffer;
    sheet->width  = width;
    sheet->height = height;
    sheet->color_invisible = color_invisible;
    return;
}

void sheet_refresh_map(struct SHEET_CONTROL *sheet_ctl, int x0, int y0, int x1, int y1, int l0) {
    int l, x, y, w, h;
    int rx0, ry0, rx1, ry1;
    unsigned char *buffer;
    unsigned char *map = sheet_ctl->map;
    unsigned char sid;
    struct SHEET *sheet;

    if (x0 < 0) { x0 = 0; }
    if (y0 < 0) { y0 = 0; }
    if (x1 > sheet_ctl->screen_x) { x1 = sheet_ctl->screen_x; }
    if (y1 > sheet_ctl->screen_y) { y1 = sheet_ctl->screen_y; }
    for (l = l0; l <= sheet_ctl->top; l++) {
        sheet  = sheet_ctl->sheets_head[l];
        sid    = sheet - sheet_ctl->sheets; // 番地を引き算してそれを下じき番号として利用
        buffer = sheet->buffer;
        rx0 = x0 - sheet->x;
        ry0 = y0 - sheet->y;
        rx1 = x1 - sheet->x;
        ry1 = y1 - sheet->y;
        if (rx0 < 0) { rx0 = 0; }
        if (ry0 < 0) { ry0 = 0; }
        if (rx1 > sheet->width ) { rx1 = sheet->width;  }
        if (ry1 > sheet->height) { ry1 = sheet->height; }
        for (h = ry0; h < ry1; h++) {
            y = sheet->y + h;
            for (w = rx0; w < rx1; w++) {
                x = sheet->x + w;
                if (buffer[h * sheet->width + w] != sheet->color_invisible) {
                    map[y * sheet_ctl->screen_x + x] = sid;
                }
            }
        }
    }
    return;
}

void sheet_refresh_sub(struct SHEET_CONTROL *sheet_ctl, int x0, int y0, int x1, int y1, int l0, int l1) {
    int l, sid;
    int x, y, w, h;
    int rx0, ry0, rx1, ry1;
    unsigned char *buffer;
    unsigned char *vram = sheet_ctl->vram;
    unsigned char *map  = sheet_ctl->map;
    struct SHEET *sheet;
    // refresh範囲が画面外にはみ出していたら補正
    if (x0 < 0) { x0 = 0; }
    if (y0 < 0) { y0 = 0; }
    if (x1 > sheet_ctl->screen_x) { x1 = sheet_ctl->screen_x; }
    if (y1 > sheet_ctl->screen_y) { y1 = sheet_ctl->screen_y; }

    for (l = l0; l <= l1; l++) {
        sheet  = sheet_ctl->sheets_head[l];
        buffer = sheet->buffer;
        sid    = sheet - sheet_ctl->sheets;
        // x0～y1を使って、refresh範囲を計算する
        rx0 = x0 - sheet->x;
        ry0 = y0 - sheet->y;
        rx1 = x1 - sheet->x;
        ry1 = y1 - sheet->y;
        if (rx0 < 0) { rx0 = 0; }
        if (ry0 < 0) { ry0 = 0; }
        if (rx1 > sheet->width ) { rx1 = sheet->width;  }
        if (ry1 > sheet->height) { ry1 = sheet->height; }
        for (h = ry0; h < ry1; h++) {
            y = sheet->y + h;
            for (w = rx0; w < rx1; w++) {
                x = sheet->x + w;
                if (map[y * sheet_ctl->screen_x + x] == sid) {
                    vram[y * sheet_ctl->screen_x + x] = buffer[h * sheet->width + w];
                }
            }
        }
    }
    return;
}

void sheet_updown(struct SHEET *sheet, int layer) {
    struct SHEET_CONTROL *sheet_ctl = sheet->control;
    int l, old = sheet->layer;  // 設定前のレイヤーを記憶する

    // 指定が低すぎだったら修正する
    if (layer > sheet_ctl->top + 1) {
        layer = sheet_ctl->top + 1;
    }
    if (layer < -1) {
        layer = -1;
    }
    sheet->layer = layer;   // レイヤーを指定

    // 以下は主にsheets[]の並べ替え
    if (old > layer) {  // 以前よりも奥に来る場合
        if (layer >= 0) {
            // 間のSheetを手前に
            for (l = old; l > layer; l--) {
                sheet_ctl->sheets_head[l] = sheet_ctl->sheets_head[l-1];
                sheet_ctl->sheets_head[l]->layer = l;
            }
            sheet_ctl->sheets_head[layer] = sheet;
            sheet_refresh_map(sheet_ctl, sheet->x, sheet->y, sheet->x + sheet->width, sheet->y + sheet->height, layer + 1);
            sheet_refresh_sub(sheet_ctl, sheet->x, sheet->y, sheet->x + sheet->width, sheet->y + sheet->height, layer + 1, old);
        }
        else {  // 非表示化
            if (sheet_ctl->top > old) {
                // 上になっているSheetを奥に
                for (l = old; l < sheet_ctl->top; l++) {
                    sheet_ctl->sheets_head[l] = sheet_ctl->sheets_head[l+1];
                    sheet_ctl->sheets_head[l]->layer = l;
                }
            }
            sheet_ctl->top--;   // 表示中のSheetが1枚減るので、一番上のLayerが1減る
            sheet_refresh_map(sheet_ctl, sheet->x, sheet->y, sheet->x + sheet->width, sheet->y + sheet->height, 0);
            sheet_refresh_sub(sheet_ctl, sheet->x, sheet->y, sheet->x + sheet->width, sheet->y + sheet->height, 0, old - 1);
        }
    }
    else if (old < layer) { // 以前よりも手前に来る場合
        if (old >= 0) {
            // 間のSheetを奥に
            for (l = old; l < layer; l++) {
                sheet_ctl->sheets_head[l] = sheet_ctl->sheets_head[l+1];
                sheet_ctl->sheets_head[l]->layer = l;
            }
            sheet_ctl->sheets_head[layer] = sheet;
        }
        else { // 非表示状態から表示状態へ
            // 上になるものを手前へ
            for (l = sheet_ctl->top; l >= layer; l--) {
                sheet_ctl->sheets_head[l+1] = sheet_ctl->sheets_head[l];
                sheet_ctl->sheets_head[l+1]->layer = l + 1;
            }
            sheet_ctl->sheets_head[layer] = sheet;
            sheet_ctl->top++;   // 表示中のSheetが増えるので、一番上のLayerが1増える
        }
        sheet_refresh_map(sheet_ctl, sheet->x, sheet->y, sheet->x + sheet->width, sheet->y + sheet->height, layer);
        sheet_refresh_sub(sheet_ctl, sheet->x, sheet->y, sheet->x + sheet->width, sheet->y + sheet->height, layer, layer);
    }
    return;
}

void sheet_refresh(struct SHEET *sheet, int x0, int y0, int x1, int y1) {
    if (sheet->layer >= 0) {    // 表示中なら、新しいSheetの情報に沿って画面を描き直す
        sheet_refresh_sub(sheet->control, sheet->x + x0, sheet->y + y0, sheet->x + x1, sheet->y + y1, sheet->layer, sheet->layer);
    }
    return;
}

void sheet_slide(struct SHEET *sheet, int x, int y) {
    int old_x = sheet->x;
    int old_y = sheet->y;
    sheet->x = x;
    sheet->y = y;
    if (sheet->layer >= 0) {
        sheet_refresh_map(sheet->control, old_x, old_y, old_x + sheet->width, old_y + sheet->height, 0);
        sheet_refresh_map(sheet->control, x, y, x + sheet->width, y + sheet->height, sheet->layer);
        sheet_refresh_sub(sheet->control, old_x, old_y, old_x + sheet->width, old_y + sheet->height, 0, sheet->layer - 1);
        sheet_refresh_sub(sheet->control, x, y, x + sheet->width, y + sheet->height, sheet->layer, sheet->layer);
    }
    return;
}

void sheet_free(struct SHEET *sheet) {
    if (sheet->layer >= 0) {
        sheet_updown(sheet, -1); // 表示中ならまず非表示にする
    }
    sheet->flags = 0;   // 未使用マーク
    return;
}
