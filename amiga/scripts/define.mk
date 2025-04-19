BIN_DIR=../bin
OBJ_DIR=../obj

# build varian
CPUSUFFIX   = 000
BUILD_TYPE  = DEBUG
COMPILER    ?= vbcc

DEBUG_DEFINES = -DDEBUG_RCLID -DDEBUG_INIT #-DDEBUG_SOCKIO -DDEBUG_VCON -DDEBUG_SHELL

# output directory
BUILD_DIR = $(COMPILER)_$(BUILD_TYPE)_$(CPUSUFFIX)
OBJ_PATH = $(OBJ_DIR)/$(BUILD_DIR)
BIN_PATH = $(BIN_DIR)/$(BUILD_DIR)

# where do the amiga files reside?
# expects the following dirs:
# wb             - HD install of workbench 3.1
# sc             - install directory of SAS C 6.58 compiler
# roadshow       - Roadshow SDK installation
AMIGA_DIR?=$(HOME)/projects/amidev
NETINC=$(AMIGA_DIR)/roadshow
export AMIGA_DIR

include $(SCRIPT_DIR)/ndk.mk

# include compiler setup
ifeq "$(COMPILER)" "vbcc"
include $(SCRIPT_DIR)/vbcc.mk
else
ifeq "$(COMPILER)" "gcc"
include $(SCRIPT_DIR)/gcc.mk
else
$(error Invalid compiler $(COMPILER))
endif
endif

# show compile
ifeq "$(V)" "1"
H :=
else
H := @
endif

# where to put test files
TEST_DIR ?= $(HOME)/cvproj/amiga/shared/rcli

# macros

# --- def_bin ---
# $1 = var
# $2 = bin name
define def_bin
$1_PATH = $(BIN_PATH)/$2
$1_SRC = $2.c $3
$1_OBJ = $(patsubst %.c,$(OBJ_PATH)/%.o,$2.c $3)

BINS += $2
BIN_PATHS += $(BIN_PATH)/$2
endef

# $1 = var
# $2 = bin name
define add_bin
$2: $($1_PATH)

clean-$2:
	@echo "  CLEAN $2"
	$(H)rm -f $($1_PATH) $($1_OBJ)

test-$2: $2
	@echo "  TEST $2"
	$(H)cp $($1_PATH) $(TEST_DIR)/

$($1_PATH): $($1_OBJ)
	@echo "  LNK $$@"
	$(H)$(LD) $(LDFLAGS_PRE) $(subst $(OBJ_DIR),$($(PREFIX)OBJ_DIR),$($1_OBJ)) \
	$(LDFLAGS_APP) $(LDFLAGS_$(BUILD_TYPE)) $(subst $(BIN_DIR),$($(PREFIX)BIN_DIR),$$@)
endef
