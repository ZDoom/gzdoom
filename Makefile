ifeq (Windows_NT,$(OS))
  WIN=1
endif

ifeq (1,$(WIN))
include Makefile.mgw
else
include Makefile.linux
endif
