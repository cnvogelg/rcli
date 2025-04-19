.PHONY: opt compiler dist dist-all clean-obj clean-dist dirs
.PHONY: all $(BINS)

all: dirs $(BINS)

clean: $(patsubst %,clean-%,$(BINS))

test: dirs $(patsubst %,test-%,$(BINS))

opt:
	@$(MAKE) BUILD_TYPE=RELEASE

gcc:
	@$(MAKE) COMPILER=gcc

compiler:
	$(H)$(MAKE) COMPILER=sc
	$(H)$(MAKE) COMPILER=vbcc
	$(H)$(MAKE) COMPILER=gcc

dist:
	$(H)$(MAKE) BUILD_TYPE=RELEASE CPUSUFFIX=000
	$(H)$(MAKE) BUILD_TYPE=RELEASE CPUSUFFIX=020
	$(H)$(MAKE) BUILD_TYPE=RELEASE CPUSUFFIX=040

dist-all:
	$(H)$(MAKE) COMPILER=vbcc dist
	$(H)$(MAKE) COMPILER=gcc dist

clean-obj:
	rm -f $(OBJS)

clean-dist:
	rm -rf $(BIN_DIR) $(OBJ_DIR)

dirs: $(OBJ_PATH) $(BIN_PATH)

rtest:
	$(H)$(MAKE) BUILD_TYPE=RELEASE test

# --- dirs ---
$(BIN_PATH):
	$(H)mkdir -p $(BIN_PATH)

$(OBJ_PATH):
	$(H)mkdir -p $(OBJ_PATH)

$(OBJ_PATH)/%.o: %.c
	@echo "  C   $<"
	$(H)$(CC) $(CFLAGS_$(BUILD_TYPE)) $(OBJ_NAME) $(subst $(OBJ_DIR),$($(PREFIX)OBJ_DIR),$@) $<
