# moonfish is licensed under the AGPL (v3 or later)
# copyright 2023, 2024 zamfofex

CFLAGS ?= -ansi -O3 -Wall -Wextra -Wpedantic
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

cc := $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

moonfish_cc := $(cc) -pthread -D_POSIX_C_SOURCE=199309L
tools_cc := $(cc) -pthread -D_POSIX_C_SOURCE=200809L

lichess_cc := $(tools_cc) -std=c99
lichess_libs := -lbearssl -lcjson

.PHONY: all clean install uninstall

all: moonfish play lichess analyse uci-ugi ugi-uci

moonfish moonfish.exe moonfish.wasm: moonfish.h chess.c search.c main.c
	$(moonfish_cc) -o $@ $(filter %.c,$^)

%: moonfish.h tools/tools.h tools/%.c tools/utils.c chess.c
	$(or $($(@)_cc),$(tools_cc)) -o $@ $(filter %.c,$^) $($(@)_libs)

ugi-uci: moonfish.h tools/tools.h tools/ugi.h tools/utils.c tools/ugi.c tools/ugi-uci.c chess.c
	$(tools_cc) -o $@ $(filter %.c,$^)

uci-ugi: tools/tools.h tools/ugi.h tools/utils.c tools/ugi.c tools/uci-ugi.c
	$(tools_cc) -o $@ $(filter %.c,$^)

clean:
	git clean -fdx

install: all
	install -m 755 moonfish $(BINDIR)/moonfish
	install -m 755 play $(BINDIR)/moonfish-play
	install -m 755 lichess $(BINDIR)/moonfish-lichess
	install -m 755 analyse $(BINDIR)/moonfish-analyse
	install -m 755 ugi-uci $(BINDIR)/ugi-uci
	install -m 755 uci-ugi $(BINDIR)/uci-ugi

uninstall:
	$(RM) $(BINDIR)/moonfish $(BINDIR)/moonfish-* $(BINDIR)/ugi-uci $(BINDIR)/uci-ugi
