#
# Makefile for rockspec
#
# Install with Lua Binaries:
#  luarocks --lua-dir C:/bin/lua-5.3.5_Win64_bin MAKE=make CC=gcc LD=gcc install lua-webview
#
# Build with luaclibs:
#  luarocks --lua-dir ../../luaclibs/lua/src MAKE=make CC=gcc LD=gcc make
#  luarocks --lua-dir C:/bin/lua-5.4.2_Win64_bin MAKE=make CC=gcc LD=gcc make lua-webview-1.3-2.rockspec
#

CC ?= gcc

PLAT ?= linux
LIBNAME = webview

ifdef LUA_LIBDIR
LUA_LIBDIR_OPT=-L$(LUA_LIBDIR)
else
LUA_LIBDIR_OPT=
endif

#LUA_APP = $(LUA_BINDIR)/$(LUA)
LUA_APP = lua5.4
LUA_VERSION = $(shell $(LUA_APP) -e "print(string.sub(_VERSION, 5))")
LUA_LIBNAME = lua$(subst .,,$(LUA_VERSION))
LUA_BITS = $(shell $(LUA_APP) -e "print(string.len(string.pack('T', 0)) * 8)")
LUA_INCDIR = /usr/include/lua5.4

WEBVIEW_ARCH = x64
ifeq ($(LUA_BITS),32)
  WEBVIEW_ARCH = x86
endif

WEBVIEW_C = webview-c

CFLAGS_linux = -pedantic  \
  -fPIC \
  -Wall \
  -Wextra \
  -Wno-unused-parameter \
  -Wstrict-prototypes \
  -I$(WEBVIEW_C) \
  -I$(LUA_INCDIR) \
  -DWEBVIEW_GTK=1 \
  $(shell pkg-config --cflags gtk+-3.0 webkit2gtk-4.1 lua5.4)

LIBFLAG_linux= -shared -static-libgcc \
  -Wl,-s \
  $(LUA_LIBDIR_OPT) \
  $(shell pkg-config --libs gtk+-3.0 webkit2gtk-4.1 lua5.4)

TARGET_linux = $(LIBNAME).so


TARGET = $(TARGET_$(PLAT))

SOURCES = webview.c

OBJS = webview.o

lib: $(TARGET)

install: install-$(PLAT)
	cp $(TARGET) $(INST_LIBDIR)
	-cp webview-launcher.lua $(INST_LUADIR)

show:
	@echo PLAT: $(PLAT)
	@echo LUA_VERSION: $(LUA_VERSION)
	@echo LUA_LIBNAME: $(LUA_LIBNAME)
	@echo CFLAGS: $(CFLAGS)
	@echo LIBFLAG: $(LIBFLAG)
	@echo LUA_LIBDIR: $(LUA_LIBDIR)
	@echo LUA_BINDIR: $(LUA_BINDIR)
	@echo LUA_INCDIR: $(LUA_INCDIR)
	@echo LUA: $(LUA)
	@echo LUALIB: $(LUALIB)

show-install:
	@echo PREFIX: $(PREFIX) or $(INST_PREFIX)
	@echo BINDIR: $(BINDIR) or $(INST_BINDIR)
	@echo LIBDIR: $(LIBDIR) or $(INST_LIBDIR)
	@echo LUADIR: $(LUADIR) or $(INST_LUADIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LIBFLAG) $(LIBFLAG_$(PLAT)) -o $(TARGET)

clean:
	-$(RM) $(OBJS) $(TARGET)

$(OBJS): %.o : %.c $(SOURCES)
	$(CC) $(CFLAGS) $(CFLAGS_$(PLAT)) -c -o $@ $<
