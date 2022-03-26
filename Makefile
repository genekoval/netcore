project := netcore

STD := c++20

library := lib$(project)

install := $(library)
targets := $(install)

$(library).type := shared
$(library).libs := ext++ timber

test.deps = $(library)
test.libs := $(project) fmt gtest timber

files = $(include) $(src) Makefile VERSION

include mkbuild/base.mk

$($(test).objects): CXXFLAGS += -DTESTDIR='"$(build)"'
