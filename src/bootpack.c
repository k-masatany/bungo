// bootpackのメイン
#include <stdio.h>
#include "bootpack.h"

#define MEMORY_MANAGER_FREES    4090    // これで約32KB
#define MEMORY_MANAGER_ADDRESS  0x003c0000

// 空き情報
struct FREE_INFO {
    unsigned int address;
    unsigned int size;
};

// メモリ管理
struct MEMORY_MANAGER {
    int frees;
    int max_frees;
    int lost_size;
    int losts;
    struct FREE_INFO free[MEMORY_MANAGER_FREES];
};

unsigned int memtest(unsigned int start, unsigned int end);
void memman_init(struct MEMORY_MANAGER *manager);
unsigned int memman_total(struct MEMORY_MANAGER *manager);
unsigned int memman_alloc(struct MEMORY_MANAGER *manager, unsigned int size);
int memman_free(struct MEMORY_MANAGER *manager, unsigned int address, unsigned int size);

void BungoMain(void)
{
    struct BOOT_INFO *boot_info = (struct BOOT_INFO *) ADR_BOOTINFO;
    struct MOUSE_DECODER mouse_decoder;
    char s[40], mouse_cursor[256];
    char keyboard_buffer[KEY_BUF_SIZE], mouse_buffer[MOUSE_BUF_SIZE];
	int mouse_x, mouse_y;
    int data;
    unsigned int memory_total;
    struct MEMORY_MANAGER *memory_manager = (struct MEMORY_MANAGER *) MEMORY_MANAGER_ADDRESS;

    init_gdtidt();
    init_pic();
    io_sti();       // IDT/PICの初期化が終わったのでCPUの割り込み禁止を解除

    fifo8_init(&keyboard_fifo, KEY_BUF_SIZE,   keyboard_buffer);
    fifo8_init(&mouse_fifo,    MOUSE_BUF_SIZE, mouse_buffer);
    io_out8(PIC0_IMR, 0xf9);    // PIC1とキーボードを許可(11111001)
    io_out8(PIC1_IMR, 0xef);    // マウスを許可(11101111)

    init_keyboard();
    enable_mouse(&mouse_decoder);

    memory_total = memtest(0x00400000, 0xbfffffff);
    memman_init(memory_manager);
    memman_free(memory_manager, 0x00001000, 0x0009e000); // 0x00001000 - 0x0009efff
    memman_free(memory_manager, 0x00400000, memory_total - 0x00400000);

    init_palette();
    init_screen(boot_info->vram, boot_info->screen_x, boot_info->screen_y);
    mouse_x = (boot_info->screen_x - 16) / 2;    // 画面中央になるように座標計算
	mouse_y = (boot_info->screen_y - 28 - 16) / 2;
	init_mouse_cursor8(mouse_cursor, COL8_008484);

    data = memtest(0x00400000, 0xbfffffff) / (1024 * 1024);
	sprintf(s, "memory %dMB", data);
	putfonts8_ascii(boot_info->vram, boot_info->screen_x, 0, 32, COL8_FFFFFF, s);

    sprintf(s, "memory %dMB   free : %dKB",
            memory_total / (1024 * 1024), memman_total(memory_manager) / 1024);
    putfonts8_ascii(boot_info->vram, boot_info->screen_x, 0, 32, COL8_FFFFFF, s);

    for (;;) {
        io_cli();
        if (fifo8_status(&keyboard_fifo) + fifo8_status(&mouse_fifo) == 0) {
            io_stihlt();
        }
        else {
            if(fifo8_status(&keyboard_fifo) != 0) {
                data = fifo8_get(&keyboard_fifo);
                io_sti();
                sprintf(s, "%02X", data);
    			boxfill8(boot_info->vram, boot_info->screen_x, COL8_008484, 0, 16, 15, 31);
    			putfonts8_ascii(boot_info->vram, boot_info->screen_x, 0, 16, COL8_FFFFFF, s);
            }
            else if (fifo8_status(&mouse_fifo) != 0) {
                data = fifo8_get(&mouse_fifo);
                io_sti();
                if (mouse_decode(&mouse_decoder, data) != 0) {
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
        			boxfill8(boot_info->vram, boot_info->screen_x, COL8_008484, 32, 16, 32 + 15 * 8 - 1, 31);
        			putfonts8_ascii(boot_info->vram, boot_info->screen_x, 32, 16, COL8_FFFFFF, s);
                    // マウスカーソルの移動
                    boxfill8(boot_info->vram, boot_info->screen_x, COL8_008484, mouse_x, mouse_y, mouse_x + 9, mouse_y + 15); // マウス消す
                    mouse_x += mouse_decoder.x;
                    mouse_y += mouse_decoder.y;
                    if (mouse_x < 0) {
                        mouse_x = 0;
                    }
                    if (mouse_x > boot_info->screen_x - 10) {
                        mouse_x = boot_info->screen_x - 10;
                    }
                    if (mouse_y < 0) {
                        mouse_y = 0;
                    }
                    if (mouse_y > boot_info->screen_y - 16) {
                        mouse_y = boot_info->screen_y - 16;
                    }
                    sprintf(s, "(%d, %d)", mouse_x, mouse_y);
                    boxfill8(boot_info->vram, boot_info->screen_x, COL8_008484, 0, 0, 79, 15);  // 座標消す
                    putfonts8_ascii(boot_info->vram, boot_info->screen_x, 0, 0, COL8_FFFFFF, s); // 座標書く
                    putblock8_8(boot_info->vram, boot_info->screen_x, 10, 16, mouse_x, mouse_y, mouse_cursor, 10); // マウス描く
                }
            }
        }
    }
}

#define EFLAGS_AC_BIT		0x00040000
#define CR0_CACHE_DISABLE	0x60000000

unsigned int memtest(unsigned int start, unsigned int end)
{
	char flg486 = 0;
	unsigned int eflg, cr0, i;

	/* 386か、486以降なのかの確認 */
	eflg = io_load_eflags();
	eflg |= EFLAGS_AC_BIT; /* AC-bit = 1 */
	io_store_eflags(eflg);
	eflg = io_load_eflags();
	if ((eflg & EFLAGS_AC_BIT) != 0) { /* 386ではAC=1にしても自動で0に戻ってしまう */
		flg486 = 1;
	}
	eflg &= ~EFLAGS_AC_BIT; /* AC-bit = 0 */
	io_store_eflags(eflg);

	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 |= CR0_CACHE_DISABLE; /* キャッシュ禁止 */
		store_cr0(cr0);
	}

	i = memtest_sub(start, end);

	if (flg486 != 0) {
		cr0 = load_cr0();
		cr0 &= ~CR0_CACHE_DISABLE; /* キャッシュ許可 */
		store_cr0(cr0);
	}

	return i;
}

void memman_init(struct MEMORY_MANAGER *manager) {
    manager->frees      = 0;
    manager->max_frees  = 0;
    manager->losts      = 0;
    manager->lost_size  = 0;
    return;
}

// 空きサイズの合計を報告
unsigned int memman_total(struct MEMORY_MANAGER *manager) {
    unsigned int i , t = 0;
    for (i = 0; i < manager->frees; i++) {
        t += manager->free[i].size;
    }
    return t;
}

// メモリ確保
unsigned int memman_alloc(struct MEMORY_MANAGER *manager, unsigned int size) {
    unsigned int i, a;
    for (i = 0; i < manager->frees; i++) {
        if (manager->free[i].size >= size) { // 十分な広さの空き領域ならば
            a = manager->free[i].address;
            manager->free[i].address += size;
            manager->free[i].size -= size;
            if(manager->free[i].size == 0) { // free[i]が空になったので前へつめる
                manager->frees--;
                for (; i< manager->frees; i++) {
                    manager->free[i] = manager->free[i+1];  // 構造体の代入
                }
            }
            return a;
        }
    }
    return 0; // 空きがない
}

// メモリ解放
int memman_free(struct MEMORY_MANAGER * manager, unsigned int address, unsigned int size) {
    int i, j;
    // まとめやすさを考えると、free[]がaddr順に並んでいるほうがいい
	// だからまず、どこに入れるべきかを決める
    for (i = 0; i < manager->frees; i++) {
        if (manager->free[i].address > address) {
            break;
        }
    }
    // free[i -1].address < address < free[i].address
    if (i > 0) { // [i-1]がある
        if (manager->free[i-1].address + manager->free[i-1].size == address) { // [i-1]の領域にまとめられる
            manager->free[i-1].size += size;
            if (i < manager->frees) { // [i+1]もある
                if (address + size == manager->free[i].address) { // [i]ともまとめられる
                    manager->free[i-1].size += manager->free[i].size;
                    // manager->free[i]の削除（free[i]が無くなったので前へつめる）
                    manager->frees--;
                    for (; i < manager->frees; i++) {
                        manager->free[i] = manager->free[i+1];
                    }
                }
            }
            return 0; // 成功
        }
    }
    // 前とはまとめられなかった
    if (i < manager->frees) { // 後ろがある
        if (address + size == manager->free[i].address) { // [i]とはまとめられる
            manager->free[i].address = address;
            manager->free[i].size   += size;
            return 0; // 成功
        }
    }
    // 前にも後ろにもまとめられなかった
    if (manager->frees < MEMORY_MANAGER_FREES) {
        // free[i]より後ろを、後ろへずらしてすき間をつくる
        for (j = manager->frees; j > i; j--) {
            manager->free[j] = manager->free[j-1];
        }
        manager->frees++;
        if (manager->max_frees < manager->frees) {
            manager->max_frees = manager->frees; // 最大値を更新
        }
        manager->free[i].address = address;
        manager->free[i].size = size;
        return 0; // 成功
    }
    // 後ろにずらせなかった
    manager->losts++;
    manager->lost_size += size;
    return -1;
}
