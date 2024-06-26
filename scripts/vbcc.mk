# use vbcc and vasm
CC=vc +aos68km -c
LD=vc +aos68km
AS=vasmm68k_mot

# NDK includes/libs
NDK_DIR ?= $(AMIGA_DIR)/ndk3.2r4
NDK_INC = $(NDK_DIR)/include_h
NDK_LIB = $(NDK_DIR)/lib
NDK_INC_ASM = $(NDK_DIR)/include_i

# netinclude
NET_INC ?= $(AMIGA_DIR)/roadshow/netinclude
DEV_INC ?= $(AMIGA_DIR)/roadshow/include

VBCC_TARGET_AMIGAOS ?= $(VBCC)/targets/m68k-amigaos/
VBCC_INC = $(VBCC_TARGET_AMIGAOS)/include
VBCC_LIB = $(VBCC_TARGET_AMIGAOS)/lib

CFLAGS = -c99 -cpu=68$(CPUSUFFIX) -Os -+ -sc
CFLAGS += -I$(VBCC_INC) -I$(NDK_INC) -I$(NET_INC) -I$(DEV_INC)
CFLAGS += $(COMMON_DEFINES)

OBJ_NAME = -o
CFLAGS_DEBUG = $(CFLAGS) -g -DDEBUG=$(DEBUG_LEVEL)
CFLAGS_RELEASE = $(CFLAGS)

LDFLAGS = -cpu=68$(CPUSUFFIX) -sc -L$(VBCC_LIB) -L$(NDK_LIB)
LDFLAGS += -lvc
LIBS_debug = -ldebug
LIBS = -lamiga
LDFLAGS_DEBUG = $(LDFLAGS) $(LIBS_debug) -g $(LIBS) -o
LDFLAGS_RELEASE = $(LDFLAGS) $(LIBS) -o
LDFLAGS_DEV = -nostdlib
LDFLAGS_APP =
LDFLAGS_HAS_MAP = 0

ASFLAGS = -Fhunk -quiet -phxass -m68$(CPUSUFFIX) -I$(NDK_INC_ASM)

AR = cat
AR_OUT = >
LIB_EXT = .lib
