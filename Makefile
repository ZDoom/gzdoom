ifeq (Windows_NT,$(OS))
include Makefile.mgw
else
all:
	@echo Building with MinGW requires Windows NT
endif
