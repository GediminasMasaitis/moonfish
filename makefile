# moonfish is licensed under the AGPL (v3 or later)
# copyright 2023, 2024 zamfofex

CFLAGS ?= -ansi -O3 -Wall -Wextra -Wpedantic
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

cc := $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

moonfish_cc := $(cc) -pthread -D_POSIX_C_SOURCE=199309L
tools_cc := $(cc) -pthread -D_POSIX_C_SOURCE=200809L
wasm_cc := $(cc) -D_POSIX_C_SOURCE=199309L

tools_src := moonfish.h tools/tools.h tools/utils.c chess.c
ugi_src := $(tools_src) tools/ugi.h tools/ugi.c tools/ugi-uci.c

.PHONY: all clean install uninstall check

all: moonfish play lichess analyse battle ribbon chat uci-ugi ugi-uci book

moonfish moonfish.exe: moonfish.h chess.c search.c main.c
	$(moonfish_cc) -o $@ $(filter %.c,$^)

moonfish.wasm: moonfish.h chess.c search.c main.c
	$(wasm_cc) -o $@ $(filter %.c,$^)

%: $(tools_src) tools/%.c
	$(tools_cc) -o $@ $(filter %.c,$^)

lichess: $(tools_src) tools/lichess.c tools/https.c
	$(tools_cc) -o $@ $(filter %.c,$^) -ltls -lssl -lcrypto -lcjson

chat: $(tools_src) tools/chat.c tools/https.c
	$(tools_cc) -o $@ $(filter %.c,$^) -ltls -lssl -lcrypto

ugi-uci: $(ugi_src)
	$(tools_cc) -o $@ $(filter %.c,$^)

uci-ugi: $(ugi_src)
	$(tools_cc) -o $@ $(filter %.c,$^)

learn: $(tools_src) search.c tools/learn.c
	$(tools_cc) -Dmoonfish_learn -o $@ $(filter %.c,$^)

check: perft
	./check.sh

clean:
	git clean -fdx

install: all
	install -D -m 755 moonfish $(BINDIR)/moonfish
	install -D -m 755 play $(BINDIR)/moonfish-play
	install -D -m 755 lichess $(BINDIR)/moonfish-lichess
	install -D -m 755 analyse $(BINDIR)/moonfish-analyse
	install -D -m 755 battle $(BINDIR)/moonfish-battle
	install -D -m 755 ribbon $(BINDIR)/moonfish-ribbon
	install -D -m 755 chat $(BINDIR)/moonfish-chat
	install -D -m 755 ugi-uci $(BINDIR)/ugi-uci
	install -D -m 755 uci-ugi $(BINDIR)/uci-ugi

uninstall:
	$(RM) $(BINDIR)/moonfish $(BINDIR)/moonfish-* $(BINDIR)/ugi-uci $(BINDIR)/uci-ugi
