ifeq ($(LTIB_BUILD),1)
export INC = -I$(TOPDIR)/include $(INCLUDE)
endif

# Get all dirs with a Makefile
TMP_DIRS := $(foreach dir, $(wildcard *), $(wildcard $(dir)/Makefile))
DIRS := $(patsubst %/Makefile,%,$(TMP_DIRS))

.PHONY: all clean
.PHONY: $(DIRS)

all clean: $(DIRS)

$(DIRS):
	-$(MAKE) -C $@ $(MAKECMDGOALS)
