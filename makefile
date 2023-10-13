CFLAGS ?= -ansi -O3 -Wall -Wextra -Wpedantic

cc := $(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)

src := *.c

moonfish_cc := $(cc)
tools_cc := $(cc) -pthread -D_POSIX_C_SOURCE=200809L

moonfish:

ifeq ($(inbuilt_network),yes)

src += net/tanh.c tanh.o
moonfish_cc += -D_POSIX_C_SOURCE=200809L -DMOONFISH_INBUILT_NET

tanh.o: tanh.moon
	$(LD) -r -b binary -o tanh.o tanh.moon

tanh.moon: tanh.pickle
	python3 convert.py tanh.pickle

endif

ifneq ($(has_pthread),no)

moonfish_cc += -DMOONFISH_HAS_PTHREAD -pthread

endif

moonfish: moonfish.h $(src)
	$(moonfish_cc) -o moonfish $(src)

play: moonfish.h tools/tools.h tools/play.c tools/utils.c chess.c
	$(tools_cc) -o play tools/play.c tools/utils.c chess.c

lichess: tools/tools.h tools/lichess.c tools/utils.c tools/play.c
	$(tools_cc) -std=c99 -o lichess tools/lichess.c tools/utils.c -lbearssl -lcjson

.PHONY: all clean

all: moonfish play lichess

clean:
	$(RM) moonfish play lichess tanh.moon tanh.o
