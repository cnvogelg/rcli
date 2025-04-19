SCRIPT_DIR=../scripts

include $(SCRIPT_DIR)/define.mk

CFLAGS += -Iinclude

$(eval $(call def_bin,TPUT,tput,termio.c))
$(eval $(call def_bin,TGET,tget,termio.c))

include $(SCRIPT_DIR)/rule.mk

$(eval $(call add_bin,TPUT,tput))
$(eval $(call add_bin,TGET,tget))
