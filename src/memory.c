// メモリ関係
#include "bootpack.h"

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

// メモリ確保（4K単位）
unsigned int memman_alloc_4k(struct MEMORY_MANAGER *manager, unsigned int size) {
    unsigned int a;
    size = (size + 0xfff) & 0xfffff000; // 四捨五入
    a = memman_alloc(manager, size);
    return a;
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

// メモリ解放（4K単位）
int memman_free_4k(struct MEMORY_MANAGER * manager, unsigned int address, unsigned int size) {
    int i;
    size = (size + 0xfff) & 0xfffff000; // 四捨五入
    i = memman_free(manager, address, size);
    return i;
}
