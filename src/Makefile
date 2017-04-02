TOOL_PATH 		= ..\..\z_tools
INC_PATH		= ..\..\z_tools\bungo

MAKE     		= $(TOOL_PATH)\make.exe -r
NASK     		= $(TOOL_PATH)\nask.exe
CC1      		= $(TOOL_PATH)\cc1.exe -I$(INC_PATH) -Os -Wall -quiet
GAS2NASK 		= $(TOOL_PATH)\gas2nask.exe -a
OBJ2BIM  		= $(TOOL_PATH)\obj2bim.exe
BIM2HRB  		= $(TOOL_PATH)\bim2hrb.exe
GOLIB           = $(TOOL_PATH)\golib00.exe
RULEFILE 		= $(TOOL_PATH)\bungo/bungo.rul
EDIMG    		= $(TOOL_PATH)\edimg.exe
IMGTOL   		= $(TOOL_PATH)\imgtol.com

BUILD_PATH		= .\build
IPL_SRC 		= ipl.nasm
IPL_BINARY 		= $(BUILD_PATH)\ipl.bin
IPL_LIST 		= $(BUILD_PATH)\ipl.lst
ASMHEAD_SRC 		= asmhead.nasm
ASMHEAD_BINARY 	= $(BUILD_PATH)\asmhead.bin
ASMHEAD_LIST 	= $(BUILD_PATH)\asmhead.lst
BOOTPACK_SRC    = bootpack.c
BOOTPACK_GASM   = $(BUILD_PATH)\bootpack.gasm
BOOTPACK_NASM   = $(BUILD_PATH)\bootpack.nasm
BOOTPACK_OBJ    = $(BUILD_PATH)\bootpack.obj
BOOTPACK_LIST   = $(BUILD_PATH)\bootpack.lst
BOOTPACK_BIM	= $(BUILD_PATH)\bootpack.bim
BOOTPACK_MAP	= $(BUILD_PATH)\bootpack.map
BOOTPACK_HRB	= $(BUILD_PATH)\bootpack.hrb
NASKFUNK_SRC    = naskfunc.nasm
NASKFUNK_OBJ	= $(BUILD_PATH)\naskfunc.obj
NASKFUNK_LIST   = $(BUILD_PATH)\naskfunc.lst
BUNGO_SYS		= $(BUILD_PATH)\bungo.sys
BUNGO_IMG		= $(BUILD_PATH)\bungo.img

# Default Make image.
default :
	$(MAKE) img

# Make IPL and Image rule.
$(IPL_BINARY): $(IPL_SRC) Makefile
	$(NASK) $(IPL_SRC) $(IPL_BINARY) $(IPL_LIST)

$(ASMHEAD_BINARY): $(ASMHEAD_SRC) Makefile
	$(NASK) $(ASMHEAD_SRC) $(ASMHEAD_BINARY) $(ASMHEAD_LIST)

$(BOOTPACK_GASM) : $(BOOTPACK_SRC) Makefile
	$(CC1) -o $(BOOTPACK_GASM) $(BOOTPACK_SRC)

$(BOOTPACK_NASM) : $(BOOTPACK_GASM) Makefile
	$(GAS2NASK) $(BOOTPACK_GASM) $(BOOTPACK_NASM)

$(BOOTPACK_OBJ) : $(BOOTPACK_NASM) Makefile
	$(NASK) $(BOOTPACK_NASM) $(BOOTPACK_OBJ) $(BOOTPACK_LIST)

$(NASKFUNK_OBJ) : $(NASKFUNK_SRC) Makefile
	$(NASK) $(NASKFUNK_SRC) $(NASKFUNK_OBJ) $(NASKFUNK_LIST)

$(BOOTPACK_BIM) : $(BOOTPACK_OBJ) $(NASKFUNK_OBJ) Makefile
	$(OBJ2BIM) @$(RULEFILE) out:$(BOOTPACK_BIM) stack:3136k map:$(BOOTPACK_MAP) \
		$(BOOTPACK_OBJ) $(NASKFUNK_OBJ)
# 3MB+64KB=3136KB

$(BOOTPACK_HRB) : $(BOOTPACK_BIM) Makefile
	$(BIM2HRB) $(BOOTPACK_BIM) $(BOOTPACK_HRB) 0

$(BUNGO_SYS) : $(ASMHEAD_BINARY) $(BOOTPACK_HRB) Makefile
	copy /B /Y $(ASMHEAD_BINARY)+$(BOOTPACK_HRB) $(BUNGO_SYS)

$(BUNGO_IMG) : $(IPL_BINARY) $(BUNGO_SYS) Makefile
	$(EDIMG)   imgin:$(TOOL_PATH)\fdimg0at.tek \
		wbinimg src:$(IPL_BINARY) len:512 from:0 to:0 \
		copy from:$(BUNGO_SYS) to:@: \
		imgout:$(BUNGO_IMG)

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
	qemu-system-x86_64 -boot a -m 128 -fda $(BUNGO_IMG)

clean :
	-del $(BUILD_PATH)\*.bin
	-del $(BUILD_PATH)\*.lst
	-del $(BUILD_PATH)\*.nasm
	-del $(BUILD_PATH)\*.gasm
	-del $(BUILD_PATH)\*.obj
	-del $(BOOTPACK_MAP)
	-del $(BOOTPACK_BIM)
	-del $(BOOTPACK_HRB)
	-del $(BUNGO_SYS)
	-del $(LIB_PATH)\*.nasm
	-del $(LIB_PATH)\*.gasm
	-del $(LIB_PATH)\*.obj

src_only :
	$(MAKE) clean
	-del $(BUNGO_IMG)
	-del $(BUNGO_LIB)
