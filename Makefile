project := netcore

STD := c++20

library := lib$(project)

install := $(library)
targets := $(install)

$(library).type := shared
$(library).libs := ext++ timber

test.deps = $(library)
test.libs := $(project) crypto ext++ fmt gtest ssl timber

install.directories = $(include)/$(project)

files = $(include) $(src) Makefile VERSION

include mkbuild/base.mk

defines.develop = TIMBER_TIMER_ACTIVE
