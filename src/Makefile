SCRIPT_DIR=../scripts

include $(SCRIPT_DIR)/define.mk

CFLAGS += -Iinclude

# define tools
$(eval $(call def_tool,RCLID,rclid,serv.c))
$(eval $(call def_tool,RUNSERV,runserv))
$(eval $(call def_tool,TESTSHELL,testshell,shell.c))
$(eval $(call def_tool,TESTVCON,testvcon,shell.c vcon.c))

# ----- tools -----
.PHONY: all $(TOOLS)

all: dirs $(TOOLS)

include $(SCRIPT_DIR)/rule.mk

clean: $(patsubst %,clean-%,$(TOOLS))

test: dirs $(patsubst %,test-%,$(TOOLS))

# --- tools ---
$(eval $(call add_tool,RCLID,rclid))
$(eval $(call add_tool,RUNSERV,runserv))
$(eval $(call add_tool,TESTSHELL,testshell))
$(eval $(call add_tool,TESTVCON,testvcon))

# --- compile ---
$(OBJ_PATH)/%.o: %.c
	@echo "  C   $<"
	$(H)$(CC) $(CFLAGS_$(BUILD_TYPE)) $(OBJ_NAME) $(subst $(OBJ_DIR),$($(PREFIX)OBJ_DIR),$@) $<
