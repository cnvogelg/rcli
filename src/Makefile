SCRIPT_DIR=../scripts

include $(SCRIPT_DIR)/define.mk

CFLAGS += -Iinclude

# define tools
$(eval $(call def_tool,RCLID,rclid,init.c serv.c vcon.c timer.c shell.c sockio.c))
$(eval $(call def_tool,RUNSERV,runserv))
$(eval $(call def_tool,TPUT,tput,termio.c))
$(eval $(call def_tool,TGET,tget,termio.c))
$(eval $(call def_tool,TESTSHELL,testshell,shell.c))
$(eval $(call def_tool,TESTVCON,testvcon,shell.c vcon.c timer.c))
$(eval $(call def_tool,TESTTIMER,testtimer,timer.c))
$(eval $(call def_tool,TESTCLIENT,testclient))
$(eval $(call def_tool,TESTSOCKIO,testsockio,sockio.c serv.c timer.c))

# ----- tools -----
.PHONY: all $(TOOLS)

all: dirs $(TOOLS)

include $(SCRIPT_DIR)/rule.mk

clean: $(patsubst %,clean-%,$(TOOLS))

test: dirs $(patsubst %,test-%,$(TOOLS))

# --- tools ---
$(eval $(call add_tool,RCLID,rclid))
$(eval $(call add_tool,RUNSERV,runserv))
$(eval $(call add_tool,TPUT,tput))
$(eval $(call add_tool,TGET,tget))
$(eval $(call add_tool,TESTSHELL,testshell))
$(eval $(call add_tool,TESTVCON,testvcon))
$(eval $(call add_tool,TESTTIMER,testtimer))
$(eval $(call add_tool,TESTCLIENT,testclient))
$(eval $(call add_tool,TESTSOCKIO,testsockio))

# --- compile ---
$(OBJ_PATH)/%.o: %.c
	@echo "  C   $<"
	$(H)$(CC) $(CFLAGS_$(BUILD_TYPE)) $(OBJ_NAME) $(subst $(OBJ_DIR),$($(PREFIX)OBJ_DIR),$@) $<
