ifeq (Windows_NT,$(OS))
include Makefile.mgw
else
include Makefile.linux
endif
