// ファイル操作関係
#include <string.h>
#include "bootpack.h"

// ディスクイメージ内のFATの圧縮をとく
void file_read_fat(int *fat, unsigned char * img) {
    int i, j = 0;
    for (i = 0; i < 2880; i += 2) {
        fat[i + 0] = (img[j + 0]      | img[j + 1] << 8) & 0xfff;
        fat[i + 1] = (img[j + 1] >> 4 | img[j + 2] << 4) & 0xfff;
        j += 3;
    }
    return;
}

void file_load_file(int cluster_no, int size, char *buffer, int *fat, char *img) {
    int i;
    for (;;) {
        if (size <= 512) {
            for (i = 0; i < size; i++) {
                buffer[i] = img[cluster_no * 512 + i];
            }
            break;
        }
        for (i = 0; i < 512; i++) {
            buffer[i] = img[cluster_no * 512 + i];
        }
        size -= 512;
        buffer += 512;
        cluster_no = fat[cluster_no];
    }
    return;
}

struct FILE_INFO *file_search(char *name, struct FILE_INFO * f_info, int max) {
    int i, j;
    char s[12] = {};
    // ファイル名を準備する
    for (j = 0; j < 11; j++) {
        s[j] = ' ';
    }
    j = 0;
    for (i = 0; name[i] != 0; i++) {
        if (name[i] == '.' && j <= 8) {
            j = 8;
        } else {
            s[j] = name[i];
            if ('a' <= s[j] && s[j] <= 'z') {
                // 小文字は大文字に直す
                s[j] -= 0x20;
            }
            j++;
        }
    }
    // ファイルを探す
    for (i = 0; i < max; i++) {
        if (f_info[i].name[0] == 0x00) {
            return 0;
        }
        if ((f_info[i].type & 0x18) == 0) {
            if (strncmp(f_info[i].name, s, 11) == 0) {
                return f_info + i; // ファイルが見つかった
            }
        }
    }
    return 0;   // 見つからなかった
}
