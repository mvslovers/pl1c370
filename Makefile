# PL1C/370 host build.  The compiler is a host program; the run time (rt/)
# and the test programs are built for MVS by mbt (mbt.toml).
CC      ?= cc
CFLAGS  ?= -O2
LDFLAGS ?=
CFLAGS  += -std=c99 -Wall -Wextra -Werror -pedantic
# The version is [project] version of mbt.toml, the one place it is kept.
VERSION := $(shell sed -n 's/^version *= *"\(.*\)"/\1/p' mbt.toml | head -1)
OUT      = build/host
SRCS     = src/main.c src/ebcdic.c src/lex.c src/pp.c src/parse.c src/resolve.c src/lower.c src/check.c src/gen.c \
           rt/pl1flt.c rt/pl1pic.c
RT       = rt/pl1rt.c rt/pl1rtfl.c rt/pl1io.c rt/pl1ioed.c rt/pl1ioget.c rt/pl1iokey.c rt/pl1cond.c rt/pl1flt.c rt/pl1pic.c rt/pl1area.c rt/pl1cons.c

$(OUT)/pl1c370: $(SRCS) src/pl1c.h rt/pl1flt.h rt/pl1pic.h mbt.toml
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) -DPL1C_VERSION='"$(VERSION)"' $(LDFLAGS) -o $@ $(SRCS)

# Host tests: the run time replayed against oracle output.
test: $(OUT)/t_putlist $(OUT)/t_scaled $(OUT)/t_float $(OUT)/t_pic $(OUT)/t_area
	$(OUT)/t_putlist > $(OUT)/putlist.out
	python3 tests/compare.py tests/host/P3LIST.oracle $(OUT)/putlist.out
	$(OUT)/t_scaled
	$(OUT)/t_float
	$(OUT)/t_pic
	$(OUT)/t_area

$(OUT)/t_area: tests/host/t_area.c $(RT) rt/pl1rt.h
	@mkdir -p $(OUT)
	$(CC) -std=c99 -Wall -Wextra -Werror -Dasm=__asm__ -o $@ tests/host/t_area.c $(RT)

$(OUT)/t_pic: tests/host/t_pic.c $(RT) rt/pl1rt.h
	@mkdir -p $(OUT)
	$(CC) -std=c99 -Wall -Wextra -Werror -Dasm=__asm__ -o $@ tests/host/t_pic.c $(RT)

$(OUT)/t_putlist: tests/host/t_putlist.c $(RT) rt/pl1rt.h
	@mkdir -p $(OUT)
	$(CC) -std=c99 -Wall -Wextra -Werror -Dasm=__asm__ -o $@ tests/host/t_putlist.c $(RT)

$(OUT)/t_scaled: tests/host/t_scaled.c $(RT) rt/pl1rt.h
	@mkdir -p $(OUT)
	$(CC) -std=c99 -Wall -Wextra -Werror -Dasm=__asm__ -o $@ tests/host/t_scaled.c $(RT)

$(OUT)/t_float: tests/host/t_float.c $(RT) rt/pl1rt.h rt/pl1flt.h
	@mkdir -p $(OUT)
	$(CC) -std=c99 -Wall -Wextra -Werror -Dasm=__asm__ -o $@ tests/host/t_float.c $(RT)

# The host compiler as a release asset: make dist PLATFORM=linux-amd64
# gives dist/pl1c370-<version>-linux-amd64.tar.gz.
PLATFORM ?= $(shell uname -s | tr A-Z a-z)-$(shell uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/')
DISTNAME  = pl1c370-$(VERSION)-$(PLATFORM)

dist: $(OUT)/pl1c370
	rm -rf dist/$(DISTNAME) && mkdir -p dist/$(DISTNAME)/bin
	cp $(OUT)/pl1c370 dist/$(DISTNAME)/bin/
	cp LICENSE README.md dist/$(DISTNAME)/
	tar -C dist -czf dist/$(DISTNAME).tar.gz $(DISTNAME)
	rm -rf dist/$(DISTNAME)

# The run time as a release asset, laid out as mbt's library packages:
# pl1c370-<version>/lib/pl1rt.a and include/pl1abi.h.  The archive is
# mbt's (mbt build, [lib] in mbt.toml); this only packs it.
LIBNAME = pl1c370-$(VERSION)

dist-lib:
	test -f build/pl1rt.a || { echo "build/pl1rt.a missing: run mbt build" >&2; exit 1; }
	rm -rf dist/$(LIBNAME) && mkdir -p dist/$(LIBNAME)/lib dist/$(LIBNAME)/include
	cp build/pl1rt.a dist/$(LIBNAME)/lib/
	cp rt/pl1abi.h dist/$(LIBNAME)/include/
	tar -C dist -czf dist/$(LIBNAME)-lib.tar.gz $(LIBNAME)
	rm -rf dist/$(LIBNAME)

# Compiler and run time into the cc370 sysroot, beside cc370 and
# libc370: make install [PREFIX=dir], default ~/.local.  The archive is
# mbt's (mbt build), built first; this never calls mbt.  It must carry
# the ABI stamp of the pl1abi.h installed with it.
PREFIX ?= $(HOME)/.local
STAMP   = $(shell sed -n 's/^\#define PL1_ABI_STAMP *"\(.*\)"/\1/p' rt/pl1abi.h)

install: $(OUT)/pl1c370
	test -f build/pl1rt.a || { echo "build/pl1rt.a missing: run mbt build" >&2; exit 1; }
	LC_ALL=C grep -q '$(STAMP)' build/pl1rt.a || \
	    { echo "build/pl1rt.a is not ABI $(STAMP): run mbt build" >&2; exit 1; }
	mkdir -p $(PREFIX)/bin $(PREFIX)/cc370/lib $(PREFIX)/cc370/include
	cp $(OUT)/pl1c370 $(PREFIX)/bin/pl1c370
	cp build/pl1rt.a $(PREFIX)/cc370/lib/pl1rt.a
	cp rt/pl1abi.h $(PREFIX)/cc370/include/pl1abi.h

clean:
	rm -rf $(OUT) dist

.PHONY: test dist dist-lib install clean
