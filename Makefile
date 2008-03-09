ifeq (Windows_NT,$(OS))
  WIN=1
endif
ifeq ($(findstring msys,$(shell sh --version 2>nul)),msys)
  WIN=1
endif

ifeq (1,$(WIN))
include Makefile.mgw
else
include Makefile.linux
endif
