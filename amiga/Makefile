.DEFAULT_GOAL=all

DIRS=rclid tools

%:
	@for dir in $(DIRS) ; do \
		$(MAKE) -C $$dir $@ ;\
	done
