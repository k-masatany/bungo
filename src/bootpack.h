// asmhead.nasm
struct BOOT_INFO {              // 0x0ff0-0x0fff
	char cyls;                 // ブートセクタはどこまでディスクを読んだのか
	char leds;                 // ブート時のキーボードのLEDの状態
	char vmode;                // ビデオモード  何ビットカラーか
	char reserve;
	short screen_x, screen_y;  // 画面解像度
	char *vram;
};
struct FILE_INFO {
    unsigned char name[8];      // ファイル名
    unsigned char ext[3];       // 拡張子
    unsigned char type;         // ファイルタイプ
    char reserve[10];           // 予約領域
    unsigned short time;        // 時刻
    unsigned short date;        // 日付
    unsigned short clust_no;    // クラスタ番号（セクタ番号）
    unsigned int size;          // ファイルサイズ
};
#define ADR_BOOTINFO	0x00000ff0
#define ADR_DISKIMG 	0x00100000

// naskfunc.nasm
void io_hlt(void);
void io_cli(void);
void io_sti(void);
void io_stihlt(void);
int io_in8(int port);
void io_out8(int port, int data);
int  io_load_eflags(void);
void io_store_eflags(int eflags);
void load_gdtr(int limit, int addr);
void load_idtr(int limit, int addr);
int load_cr0(void);
void store_cr0(int cr0);
void load_tr(int tr);
void asm_inthandler20(void);
void asm_inthandler21(void);
void asm_inthandler27(void);
void asm_inthandler2c(void);
unsigned int memtest_sub(unsigned int start, unsigned int end);
void farjmp(int eip, int cs);

// graphic.c
void init_palette(void);
void set_palette(int start, int end, unsigned char *rgb);
void boxfill8(unsigned char *vram, int xsize, unsigned char c, int x0, int y0, int x1, int y1);
void init_screen8(char *vram, int x, int y);
void putfont8(char *vram, int xsize, int x, int y, char c, char *font);
void putfonts8_ascii(char *vram, int xsize, int x, int y, char c, unsigned char *s);
void init_mouse_cursor8(char *mouse, char back_color);
void putblock8_8(char *vram, int vxsize, int pxsize, int pysize, int px0, int py0, char *buf, int bxsize);

#define COL8_000000		0
#define COL8_FF0000		1
#define COL8_00FF00		2
#define COL8_FFFF00		3
#define COL8_0000FF		4
#define COL8_FF00FF		5
#define COL8_00FFFF		6
#define COL8_FFFFFF		7
#define COL8_C6C6C6		8
#define COL8_840000		9
#define COL8_008400		10
#define COL8_848400		11
#define COL8_000084		12
#define COL8_840084		13
#define COL8_008484		14
#define COL8_848484		15

// descriptor_table.c
struct SEGMENT_DESCRIPTOR {
	short limit_low, base_low;
	char base_mid, access_right;
	char limit_high, base_high;
};

struct GATE_DESCRIPTOR {
    short offset_low, selector;
	char dw_count, access_right;
	short offset_high;
};

void init_gdtidt(void);
void set_segment_descriptor(struct SEGMENT_DESCRIPTOR *sd, unsigned int limit, int base, int ar);
void set_gate_descriptor(struct GATE_DESCRIPTOR *gd, int offset, int selector, int ar);
#define ADR_IDT			0x0026f800
#define LIMIT_IDT		0x000007ff
#define ADR_GDT			0x00270000
#define LIMIT_GDT		0x0000ffff
#define ADR_BOTPAK		0x00280000
#define LIMIT_BOTPAK	0x0007ffff
#define AR_DATA32_RW	0x4092
#define AR_CODE32_ER	0x409a
#define AR_TSS32		0x0089
#define AR_INTGATE32	0x008e

// interrupt.c
void init_pic(void);
void inthandler21(int *esp);
void inthandler27(int *esp);
void inthandler2c(int *esp);
#define PIC0_ICW1		0x0020
#define PIC0_OCW2		0x0020
#define PIC0_IMR		0x0021
#define PIC0_ICW2		0x0021
#define PIC0_ICW3		0x0021
#define PIC0_ICW4		0x0021
#define PIC1_ICW1		0x00a0
#define PIC1_OCW2		0x00a0
#define PIC1_IMR		0x00a1
#define PIC1_ICW2		0x00a1
#define PIC1_ICW3		0x00a1
#define PIC1_ICW4		0x00a1

// fifo.c
#define FIFO_BUF_SIZE   128
#define KEY_CMD_BUF     32
struct FIFO32 {
	int *buf;              // バッファ
	int p;                 // 書き込み位置
    int q;                 // 読み込み位置
    int size;              // バッファサイズ
    int free;              // 空き領域
    int flags;             // あふれフラグ
    struct TASK *task;     // データが入ったときに起こすタスク
};
void fifo32_init(struct FIFO32 *fifo, int size, int *buf, struct TASK *task);
int  fifo32_put(struct FIFO32 *fifo, int data);
int  fifo32_get(struct FIFO32 *fifo);
int  fifo32_status(struct FIFO32 *fifo);

// keybord.c
#define PORT_KEY_DATA			  0x0060
#define PORT_KEY_COMMAND		  0x0064
void inthandler21(int *esp);
void wait_KBC_sendready(void);
void init_keyboard(struct FIFO32 *fifo, int data);

// mouse.c
struct MOUSE_DECODER {
    unsigned char phase;
    unsigned char buffer[3];
    int x;
    int y;
    int button;
};
void inthandler2c(int *esp);
void enable_mouse(struct FIFO32 *fifo, int data, struct MOUSE_DECODER *mouse_decoder);
int mouse_decode(struct MOUSE_DECODER *mouse_decoder, unsigned char data);

// memory.c
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
unsigned int memman_alloc_4k(struct MEMORY_MANAGER *manager, unsigned int size);
int memman_free_4k(struct MEMORY_MANAGER *manager, unsigned int address, unsigned int size);

// sheet.c
#define MAX_SHEETS      256
struct SHEET {
    unsigned char *buffer;          // 描画内容を記憶しているバッファへのポインタ
    int width;                      // x方向長さ
    int height;                     // y方向長さ
    int x;                          // x座標
    int y;                          // y座標
    int color_invisible;            // 透明色番号
    int layer;                      // レイヤー：0が下層
    int flags;                      // 設定情報
    struct SHEET_CONTROL *control;  // 所属
};
struct SHEET_CONTROL {
    unsigned char *vram;            // Video RAM
    unsigned char *map;
    int screen_x;
    int screen_y;
    int top;                        // 一番上にあるSHEETの高さ
    struct SHEET *sheets_head[MAX_SHEETS];  // 各SHEET構造体へのポインタを保存する
    struct SHEET  sheets[MAX_SHEETS];       // 各SHEETの情報
};
struct SHEET_CONTROL *sheet_control_init(struct MEMORY_MANAGER *mem_manager, unsigned char *vram, int screen_x, int screen_y );
struct SHEET *sheet_alloc(struct SHEET_CONTROL *sheet_ctl);
void sheet_set_buffer(struct SHEET *sheet, unsigned char *buffer, int width, int height, int color_invisible);
void sheet_updown(struct SHEET *sheet, int layer);
void sheet_refresh(struct SHEET *sheet, int x0, int y0, int x1, int y1);
void sheet_slide(struct SHEET *sheet, int x, int y);
void sheet_free(struct SHEET *sheet);

// timer.c
#define MAX_TIMER       512
struct TIMER {
    unsigned int timeout;
    unsigned int flags;
    struct FIFO32 *fifo;
    int data;
    struct TIMER *next;
};
struct TIMER_CONTROL {
    unsigned int count;
    unsigned int next;
    struct TIMER *timers_head;
    struct TIMER timers[MAX_TIMER];
};
extern struct TIMER_CONTROL timer_ctl;
void init_pit(void);
struct TIMER *timer_alloc(void);
void timer_free(struct TIMER *timer);
void timer_init(struct TIMER *timer, struct FIFO32 *fifo, int data);
void timer_set_time(struct TIMER *timer, unsigned int timeout);
void inthandler20(int *esp);

// multitask.c
#define MAX_TASKS       1000
#define MAX_TASKS_LV    100
#define MAX_TASKLEVELS  10
#define TASK_GDT0   3       // TASKをGDTの何番から割り当てるか
struct TSS32 {
    int backlink, esp0, ss0, esp1, ss1, esp2, ss2, cr3;
    int eip, eflags, eax, ebx, ecx, edx, esp, ebp, esi, edi;
    int es, cs, ss, ds, fs, gs;
    int ldtr, iomap;
};
struct TASK {
    int selector;       // GDTの番号
    int flags;
    int level;
    int priority;
    struct TSS32 tss;
    struct FIFO32 fifo;
};
struct TASK_LEVEL {
    int running;    // 動作しているタスクの数
    int now;        // 動作中のタスク
    struct TASK *tasks_head[MAX_TASKS_LV];
};
struct TASK_CONTROL {
    int now_level;          // 現在動作中のLv
    char need_lv_change;    // 次の切り替え時にLvを切り替えるかどうか
    struct TASK_LEVEL levels[MAX_TASKLEVELS];
    struct TASK tasks[MAX_TASKS];
};
extern struct TIMER *task_timer;
struct TASK *task_init(struct MEMORY_MANAGER *memory_manager);
struct TASK *task_alloc(void);
void task_run(struct TASK *task, int level, int priority);
void task_switch(void);
void task_sleep(struct TASK *task);
struct TASK *task_now(void);
