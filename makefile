# moonfish's license: 0BSD
# copyright 2025 zamfofex

.PHONY: all check clean install
.SUFFIXES:
.SUFFIXES: .c .o

# configurable variables (note: not exhaustive!)
EXE = moonfish
CFLAGS = -O3 -Wall -Wextra -Wpedantic
CPPFLAGS = -Dmoonfish_pthreads
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
RM = rm -f
LD = $(CC)

# configurable libraries
LIBM = -lm
LIBPTHREAD = -pthread
LIBATOMIC =
LIBTLS = -ltls -lssl -lcrypto
LIBCJSON = -lcjson

# further configurable variables
moonfish_libs = $(LIBM) $(LIBPTHREAD) $(LIBATOMIC)
lichess_libs = $(LIBPTHREAD) $(LIBTLS) $(LIBCJSON)
analyse_libs = $(LIBPTHREAD)
chat_libs = $(LIBTLS)

# hack for BSD Make
# (ideally, '$^' should be used directly instead)
.ALLSRC ?= $^

tools = lichess analyse chat perft
obj = chess.o search.o main.o

$(EXE): $(obj)
$(tools): chess.o tools/utils.o
lichess: tools/lichess.o tools/https.o
analyse: tools/analyse.o tools/pgn.o
chat: tools/chat.o tools/https.o
perft: tools/perft.o

$(obj): moonfish.h
tools/utils.o: moonfish.h tools/tools.h
tools/https.o: tools/https.h

$(EXE):
	$(LD) $(LDFLAGS) -o $@ $(.ALLSRC) $(moonfish_libs)

lichess analyse chat perft:
	$(LD) $(LDFLAGS) -o $@ $(.ALLSRC) $($@_libs)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

check: $(EXE) perft
	scripts/check.sh

clean:
	git clean -fdx

install: all
	install -D -m 755 moonfish $(DESTDIR)$(BINDIR)/moonfish
	install -D -m 755 lichess $(DESTDIR)$(BINDIR)/moonfish-lichess
	install -D -m 755 analyse $(DESTDIR)$(BINDIR)/moonfish-analyse
	install -D -m 755 chat $(DESTDIR)$(BINDIR)/moonfish-chat
