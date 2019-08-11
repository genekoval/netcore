PROJECT = netcode
VERSION = 0.1.0
NAMESPACE = nova

BUILD = .
PREFIX = $(DEVROOT)
RELEASE = false

LIBRARY = lib$(PROJECT).so
LIBFILE = $(LIBRARY).$(VERSION)

INCDIR = include
SRCDIR = src
OBJDIR = $(BUILD)/obj
LIBDIR = $(BUILD)/lib

CC = g++
CCFLAGS = -I $(INCDIR) -std=gnu++17 -Wall
BUILDFLAGS =

ifeq ($(RELEASE),false)
	DEVLIB = $(DEVROOT)/lib
	CCFLAGS += -I $(DEVROOT)/include
	BUILDFLAGS += -L$(DEVLIB) -Wl,-rpath=$(DEVLIB)
endif

SRC := $(wildcard $(SRCDIR)/*.cpp)
OBJ := $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(SRC))
TARGET = $(LIBDIR)/$(LIBFILE)
HEADERS := $(wildcard $(INCDIR)/*.h)
HINSTALL = $(INCDIR)/$(NAMESPACE)

$(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	$(CC) -o $@ -c -fpic $(CCFLAGS) $^

$(TARGET) : $(OBJ)
	$(CC) -o $@ -shared $(BUILDFLAGS) $(CCFLAGS) $^

.PHONY: all clean dir install version

all : dir $(TARGET)

clean :
	rm -rf $(OBJDIR) $(LIBDIR)

dir :
	@mkdir -p $(OBJDIR) $(LIBDIR)

install :
	@mkdir -p $(PREFIX)/{$(HINSTALL),$(LIBDIR)}
	@echo COPY header files
	@cp -v $(HEADERS) $(PREFIX)/$(HINSTALL)
	@echo INSTALL $(PREFIX)/$(TARGET)
	@cp $(TARGET) $(PREFIX)/$(LIBDIR)
	@cd $(PREFIX)/$(LIBDIR) && ln -sf $(LIBFILE) $(LIBRARY)

version :
	@echo Project: $(PROJECT)
	@echo Version: $(VERSION)
