project := netcore
version := 0.1.0

STD := gnu++2a

library = lib$(project)

targets := $(library) $(project)
install := $(library)

$(library).type = shared
define $(library).libs
 ext++
 timber
endef

$(project).type = executable
define $(project).libs
 $(project)
 commline
 timber
endef
define $(project).deps
 $(library)
endef

include mkbuild/base.mk

$(project).main := $(obj)/$(project)/main.o

$($(project).main): CXXFLAGS += -DNAME=\"$(project)\" -DVERSION=\"$(version)\"

.PHONY: test.integration
test.integration:
	@node $(src)/$(project)-test/main.js $(project)
