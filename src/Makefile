TOOL_PATH 		= ..\..\z_tools
INC_PATH		= ..\..\z_tools\bungo

MAKE     		= $(TOOL_PATH)\make.exe -r
NASK     		= $(TOOL_PATH)\nask.exe
CC1      		= $(TOOL_PATH)\cc1.exe -I$(INC_PATH) -Os -Wall -quiet
GAS2NASK 		= $(TOOL_PATH)\gas2nask.exe -a
OBJ2BIM  		= $(TOOL_PATH)\obj2bim.exe
MAKEFONT 		= $(TOOL_PATH)\makefont.exe
BIN2OBJ  		= $(TOOL_PATH)\bin2obj.exe
BIM2HRB  		= $(TOOL_PATH)\bim2hrb.exe
GOLIB           = $(TOOL_PATH)\golib00.exe
RULEFILE 		= $(TOOL_PATH)\bungo/bungo.rul
EDIMG    		= $(TOOL_PATH)\edimg.exe
IMGTOL   		= $(TOOL_PATH)\imgtol.com

VPATH			= build
BUILD_PATH		= .\build
IPL_SRC 		= ipl.nasm
IPL_BINARY 		= $(BUILD_PATH)\ipl.bin
IPL_LIST 		= $(BUILD_PATH)\ipl.lst
ASMHEAD_SRC 	= asmhead.nasm
ASMHEAD_BINARY 	= $(BUILD_PATH)\asmhead.bin
ASMHEAD_LIST 	= $(BUILD_PATH)\asmhead.lst
NASKFUNK_OBJ	= $(BUILD_PATH)\naskfunc.obj

GRAPHIC_OBJ     = $(BUILD_PATH)\graphic.obj
DESC_TBL_OBJ    = $(BUILD_PATH)\descriptor_table.obj
INTERRUPT_OBJ   = $(BUILD_PATH)\interrupt.obj
FIFO_OBJ   		= $(BUILD_PATH)\fifo.obj
KEYBORD_OBJ   	= $(BUILD_PATH)\keyboard.obj
MOUSE_OBJ   	= $(BUILD_PATH)\mouse.obj
MEMORY_OBJ   	= $(BUILD_PATH)\memory.obj
SHEET_OBJ   	= $(BUILD_PATH)\sheet.obj
TIMER_OBJ   	= $(BUILD_PATH)\timer.obj
MULTITASK_OBJ  	= $(BUILD_PATH)\multitask.obj
WINDOW_OBJ  	= $(BUILD_PATH)\window.obj
CONSOLE_OBJ  	= $(BUILD_PATH)\console.obj
FILE_OBJ  		= $(BUILD_PATH)\file.obj

BOOTPACK_OBJ    = $(BUILD_PATH)\bootpack.obj
BOOTPACK_BIM	= $(BUILD_PATH)\bootpack.bim
BOOTPACK_MAP	= $(BUILD_PATH)\bootpack.map
BOOTPACK_HRB	= $(BUILD_PATH)\bootpack.hrb
OBJS_BOOTPACK   = $(BOOTPACK_OBJ) $(NASKFUNK_OBJ) $(CHARSET_OBJ) $(GRAPHIC_OBJ) \
            	$(DESC_TBL_OBJ) $(INTERRUPT_OBJ) $(FIFO_OBJ) $(KEYBORD_OBJ) $(MOUSE_OBJ) \
				$(MEMORY_OBJ) $(SHEET_OBJ) $(TIMER_OBJ) $(MULTITASK_OBJ) \
				$(WINDOW_OBJ) $(CONSOLE_OBJ) $(FILE_OBJ)
CHARSET_SRC		= hankaku.txt
CHARSET_BINARY	= $(BUILD_PATH)\hankaku.bin
CHARSET_OBJ		= $(BUILD_PATH)\hankaku.obj
_CHARSET		= _hankaku
BUNGO_SYS		= $(BUILD_PATH)\bungo.sys
BUNGO_IMG		= $(BUILD_PATH)\bungo.img

HALT_BIN		= $(BUILD_PATH)\halt.exe

# Default Make image.
default :
	$(MAKE) img

# Make IPL and Image rule.
$(IPL_BINARY): $(IPL_SRC) Makefile
	$(NASK) $(IPL_SRC) $(IPL_BINARY) $(IPL_LIST)

$(ASMHEAD_BINARY): $(ASMHEAD_SRC) Makefile
	$(NASK) $(ASMHEAD_SRC) $(ASMHEAD_BINARY) $(ASMHEAD_LIST)

$(CHARSET_BINARY): $(CHARSET_SRC) Makefile
	$(MAKEFONT) $(CHARSET_SRC) $(CHARSET_BINARY)

$(CHARSET_OBJ): $(CHARSET_BINARY) Makefile
	$(BIN2OBJ) $(CHARSET_BINARY) $(CHARSET_OBJ) $(_CHARSET)

$(BOOTPACK_BIM): $(OBJS_BOOTPACK) Makefile
	$(OBJ2BIM) @$(RULEFILE) out:$(BOOTPACK_BIM) stack:3136k map:$(BOOTPACK_MAP) \
		$(OBJS_BOOTPACK)
# 3MB+64KB=3136KB

$(BOOTPACK_HRB): $(BOOTPACK_BIM) Makefile
	$(BIM2HRB) $(BOOTPACK_BIM) $(BOOTPACK_HRB) 0

$(BUNGO_SYS): $(ASMHEAD_BINARY) $(BOOTPACK_HRB) Makefile
	copy /B /Y $(ASMHEAD_BINARY)+$(BOOTPACK_HRB) $(BUNGO_SYS)

$(BUNGO_IMG): $(IPL_BINARY) $(BUNGO_SYS) Makefile
	$(EDIMG)   imgin:$(TOOL_PATH)\fdimg0at.tek \
		wbinimg src:$(IPL_BINARY) len:512 from:0 to:0 \
		copy from:$(BUNGO_SYS) to:@: \
		copy from:hankaku.txt to:@: \
		copy from:bootpack.c to:@: \
		copy from:$(HALT_BIN) to:@: \
		imgout:$(BUNGO_IMG)

$(NASKFUNK_OBJ):
	$(NASK) naskfunc.nasm $(BUILD_PATH)\naskfunc.obj $(BUILD_PATH)\naskfunc.lst

# 一般規則
%.obj:
	$(CC1) -o $*.gasm $(*F).c
	$(GAS2NASK) $*.gasm $*.nasm
	$(NASK) $*.nasm $*.obj $*.lst

# ライブラリ生成用
LIB_PATH    = .\startup

BUNGO_LIB   = $(LIB_PATH)\bungolibc.lib
LIB_GASM    = $(LIB_PATH)\startup.gasm
LIB_NASM    = $(LIB_PATH)\startup.nasm
LIB_OBJ     = $(LIB_PATH)\startup.obj

$(LIB_GASM) : startup.c Makefile
	$(CC1) -o $(LIB_GASM) startup.c

$(LIB_NASM) : $(LIB_GASM) Makefile
	$(GAS2NASK) $(LIB_GASM) $(LIB_NASM)

$(LIB_OBJ) : $(LIB_NASM) Makefile
	$(NASK) $(LIB_NASM) $(LIB_OBJ)

$(BUNGO_LIB) : $(LIB_OBJ) Makefile
	$(GOLIB) out:$(BUNGO_LIB) $(LIB_OBJ)

# Shortcut commands.
img :
	$(MAKE) $(BUNGO_IMG)

lib :
	$(MAKE) $(BUNGO_LIB)

run :
	$(MAKE) img
	qemu-system-x86_64 -boot a -m 128 -localtime -vga std -fda $(BUNGO_IMG)

clean :
	-del $(BUILD_PATH)\*.bin
	-del $(BUILD_PATH)\*.lst
	-del $(BUILD_PATH)\*.nasm
	-del $(BUILD_PATH)\*.gasm
	-del $(BUILD_PATH)\*.obj
	-del $(BUILD_PATH)\*.map
	-del $(BUILD_PATH)\*.bim
	-del $(BUILD_PATH)\*.hrb
	-del $(BUILD_PATH)\*.sys
	-del $(LIB_PATH)\*.nasm
	-del $(LIB_PATH)\*.gasm
	-del $(LIB_PATH)\*.obj

src_only :
	$(MAKE) clean
	-del $(BUNGO_IMG)
	-del $(BUNGO_LIB)

rerun:
	$(MAKE) clean
	$(MAKE) run
