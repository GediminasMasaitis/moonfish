CFLAGS ?= -ansi -O3 -Wall -Wextra -Wpedantic

cc := $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

src := *.c

moonfish_cc := $(cc)
tools_cc := $(cc) -pthread -D_POSIX_C_SOURCE=200809L

.PHONY: all clean

all: moonfish play lichess analyse

ifneq ($(has_pthread),no)
moonfish_cc += -DMOONFISH_HAS_PTHREAD -pthread
endif

moonfish: moonfish.h $(src)
	$(moonfish_cc) -o moonfish $(src)

play: moonfish.h tools/tools.h tools/play.c tools/utils.c chess.c
	$(tools_cc) -o play tools/play.c tools/utils.c chess.c

lichess: tools/tools.h tools/lichess.c tools/utils.c
	$(tools_cc) -std=c99 -o lichess tools/lichess.c tools/utils.c -lbearssl -lcjson

analyse: tools/tools.h tools/analyse.c tools/utils.c chess.c
	$(tools_cc) -o analyse tools/analyse.c tools/utils.c chess.c

clean:
	$(RM) moonfish play lichess analyse
