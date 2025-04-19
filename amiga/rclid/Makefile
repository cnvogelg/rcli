SCRIPT_DIR=../scripts

include $(SCRIPT_DIR)/define.mk

CFLAGS += -Iinclude

$(eval $(call def_bin,RCLID,rclid,init.c serv.c vcon.c timer.c shell.c sockio.c))
$(eval $(call def_bin,RUNSERV,runserv))
$(eval $(call def_bin,TESTSHELL,testshell,shell.c))
$(eval $(call def_bin,TESTVCON,testvcon,shell.c vcon.c timer.c))
$(eval $(call def_bin,TESTTIMER,testtimer,timer.c))
$(eval $(call def_bin,TESTCLIENT,testclient))
$(eval $(call def_bin,TESTSOCKIO,testsockio,sockio.c serv.c timer.c))

include $(SCRIPT_DIR)/rule.mk

$(eval $(call add_bin,RCLID,rclid))
$(eval $(call add_bin,RUNSERV,runserv))
$(eval $(call add_bin,TESTSHELL,testshell))
$(eval $(call add_bin,TESTVCON,testvcon))
$(eval $(call add_bin,TESTTIMER,testtimer))
$(eval $(call add_bin,TESTCLIENT,testclient))
$(eval $(call add_bin,TESTSOCKIO,testsockio))
