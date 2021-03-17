project := netcore
version := 0.1.0

STD := gnu++2a

library = lib$(project)

install := $(library)
targets := $(install)

$(library).type = shared
define $(library).libs
 ext++
 timber
endef

test.deps = $(library)
define test.libs
 $(project)
 gtest
 gtest_main
 timber
endef

include mkbuild/base.mk

$($(test).objects): CXXFLAGS += -DTESTDIR='"$(build)"'
