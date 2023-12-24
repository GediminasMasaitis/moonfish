# moonfish is licensed under the AGPL (v3 or later)
# copyright 2023 zamfofex

CFLAGS ?= -ansi -O3 -Wall -Wextra -Wpedantic
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

cc := $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

moonfish_cc := $(cc) -pthread -D_POSIX_C_SOURCE=199309L
tools_cc := $(cc) -pthread -D_POSIX_C_SOURCE=200809L

lichess_cc := $(tools_cc) -std=c99
lichess_libs := -lbearssl -lcjson

.PHONY: all clean install uninstall

all: moonfish play lichess analyse

chess.c: moonfish.h
search.c: moonfish.h
main.c: moonfish.h
tools/*.c: tools/tools.h

moonfish moonfish.exe: chess.c search.c main.c
	$(moonfish_cc) -o $@ $^

%: tools/%.c tools/utils.c chess.c
	$(or $($(@)_cc),$(tools_cc)) -o $@ $^ $($(@)_libs)

clean:
	git clean -fdx

install: all
	install -m 755 moonfish $(BINDIR)/moonfish
	install -m 755 play $(BINDIR)/moonfish-play
	install -m 755 lichess $(BINDIR)/moonfish-lichess
	install -m 755 analyse $(BINDIR)/moonfish-analyse

uninstall:
	$(RM) $(BINDIR)/moonfish $(BINDIR)/moonfish-*
