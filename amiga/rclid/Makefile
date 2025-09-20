SCRIPT_DIR=../scripts

include $(SCRIPT_DIR)/define.mk

CFLAGS += -Iinclude

RCLI_SRCS = init.c serv.c vcon.c timer.c run.c sockio.c conv.c line.c

$(eval $(call def_bin,RCLID,rclid,$(RCLI_SRCS)))
$(eval $(call def_bin,RUNSERV,runserv))
$(eval $(call def_bin,TESTRUN,testrun,run.c))
$(eval $(call def_bin,TESTVCON,testvcon,run.c vcon.c timer.c))
$(eval $(call def_bin,TESTTIMER,testtimer,timer.c))
$(eval $(call def_bin,TESTCLIENT,testclient))
$(eval $(call def_bin,TESTSOCKIO,testsockio,sockio.c serv.c timer.c))
$(eval $(call def_bin,TESTLINE,testline,sockio.c serv.c timer.c line.c))

include $(SCRIPT_DIR)/rule.mk

$(eval $(call add_bin,RCLID,rclid))
$(eval $(call add_bin,RUNSERV,runserv))
$(eval $(call add_bin,TESTRUN,testrun))
$(eval $(call add_bin,TESTVCON,testvcon))
$(eval $(call add_bin,TESTTIMER,testtimer))
$(eval $(call add_bin,TESTCLIENT,testclient))
$(eval $(call add_bin,TESTSOCKIO,testsockio))
$(eval $(call add_bin,TESTLINE,testline))
