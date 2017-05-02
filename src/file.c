// ファイル操作関係
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
