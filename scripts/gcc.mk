# use gcc and vasm
CC=m68k-amigaos-gcc -c
LD=m68k-amigaos-gcc
AS=vasmm68k_mot

#BASEREL = -fbaserel -DBASEREL

CFLAGS = -Wall -Werror -noixemul -mcrt=clib2
CFLAGS += -mcpu=68$(CPUSUFFIX) $(BASEREL) -Os
CFLAGS += -I$(VBCC_INC) -I$(NDK_INC) -I$(NET_INC) -I$(DEV_INC)
CFLAGS += $(COMMON_DEFINES)

OBJ_NAME = -o
CFLAGS_DEBUG = $(CFLAGS) -g
CFLAGS_RELEASE = $(CFLAGS)

LDFLAGS = -mcpu=68$(CPUSUFFIX) $(BASEREL) -L$(NDK_LIB)
LDFLAGS += -Os
LIBS_debug = -ldebug
LIBS = -lamiga -lc
LDFLAGS_DEBUG = $(LDFLAGS) $(LIBS_debug) -g $(LIBS) -o
LDFLAGS_RELEASE = $(LDFLAGS) $(LIBS) -o
LDFLAGS_DEV = -nostartfiles
LDFLAGS_APP =
LDFLAGS_HAS_MAP = 0

ASFLAGS = -Fhunk -quiet -phxass -m68$(CPUSUFFIX) -I$(NDK_INC_ASM)
