# moonfish's license: 0BSD
# copyright 2025 zamfofex

</$objtype/mkfile

moonfish lichess chat:
	$LD $LDFLAGS -o $target $prereq

%.$O: %.c moonfish.h
	$CC $CFLAGS -I/sys/include/npe -Dmoonfish_plan9 -o $stem.$O $stem.c

moonfish: chess.$O search.$O main.$O
lichess: chess.$O tools/lichess.$O tools/utils.$O tools/https.$O
chat: chess.$O tools/chat.$O tools/utils.$O tools/https.$O

tools/lichess.$O tools/chat.$O: tools/tools.h tools/https.h

tools/utils.$O: tools/tools.h
tools/https.$O: tools/https.h
