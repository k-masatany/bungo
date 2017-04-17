// asmhead.nasm
struct BOOT_INFO {              // 0x0ff0-0x0fff
	char cyls;                 // ブートセクタはどこまでディスクを読んだのか
	char leds;                 // ブート時のキーボードのLEDの状態
	char vmode;                // ビデオモード  何ビットカラーか
	char reserve;
	short screen_x, screen_y;  // 画面解像度
	char *vram;
};
#define ADR_BOOTINFO	0x00000ff0

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
void asm_inthandler21(void);
void asm_inthandler27(void);
void asm_inthandler2c(void);
unsigned int memtest_sub(unsigned int start, unsigned int end);

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
#define AR_INTGATE32	0x008e

// Interrupt.c
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
#define KEY_BUF_SIZE    32
#define MOUSE_BUF_SIZE  128
struct FIFO8 {
	unsigned char *buf;    // バッファ
	int p;                 // 書き込み位置
    int q;                 // 読み込み位置
    int size;              // バッファサイズ
    int free;              // 空き領域
    int flags;             // あふれフラグ
};
void fifo8_init(struct FIFO8 *fifo, int size, unsigned char *buf);
int fifo8_put(struct FIFO8 *fifo, unsigned char data);
int fifo8_get(struct FIFO8 *fifo);
int fifo8_status(struct FIFO8 *fifo);

// keybord.c
#define PORT_KEY_DATA			  0x0060
#define PORT_KEY_STATUS			  0x0064
#define PORT_KEY_COMMAND		  0x0064
#define KEY_STATUS_SEND_NOTREAD   0x02
#define KEY_COMMAND_WRITE_MODE    0x60
#define KBC_MODE                  0x47
extern struct FIFO8 keyboard_fifo;
void inthandler21(int *esp);
void wait_KBC_sendready(void);
void init_keyboard(void);

// mouse.c
struct MOUSE_DECODER {
    unsigned char phase;
    unsigned char buffer[3];
    int x;
    int y;
    int button;
};
void inthandler2c(int *esp);
void enable_mouse(struct MOUSE_DECODER *mouse_decoder);
int mouse_decode(struct MOUSE_DECODER *mouse_decoder, unsigned char data);
extern struct FIFO8 mouse_fifo;

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
    unsigned char *buffer;      // 描画内容を記憶しているバッファへのポインタ
    int width;                  // x方向長さ
    int height;                 // y方向長さ
    int x;                      // x座標
    int y;                      // y座標
    int color_invisible;        // 透明色番号
    int layer;                  // レイヤー：0が下層
    int flags;                  // 設定情報
};
struct SHEET_CONTROL {
    unsigned char *vram;        // Video RAM
    int screen_x;
    int screen_y;
    int top;                    // 一番上にあるSHEETの高さ
    struct SHEET *sheets_head[MAX_SHEETS];  // 各SHEET構造体へのポインタを保存する
    struct SHEET  sheets[MAX_SHEETS];       // 各SHEETの情報
};
struct SHEET_CONTROL *sheet_control_init(struct MEMORY_MANAGER *mem_manager, unsigned char *vram, int screen_x, int screen_y );
struct SHEET *sheet_alloc(struct SHEET_CONTROL *sheet_ctl);
void sheet_set_buffer(struct SHEET *sheet, unsigned char *buffer, int width, int height, int color_invisible);
void sheet_updown(struct SHEET_CONTROL *sheet_ctl, struct SHEET *sheet, int layer);
void sheet_refresh(struct SHEET_CONTROL *sheet_ctl, struct SHEET *sheet, int x0, int y0, int x1, int y1);
void sheet_slide(struct SHEET_CONTROL *sheet_ctl, struct SHEET *sheet, int x, int y);
void sheet_free(struct SHEET_CONTROL *sheet_ctl, struct SHEET *sheet);
