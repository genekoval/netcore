project = netcode
version = 0.1.0

library = lib$(project)

install := $(library)
targets := $(install)

$(library).type = shared

include $(DEVROOT)/include/mkbuild/base.mk
