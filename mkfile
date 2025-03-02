# moonfish is licensed under the AGPL (v3 or later)
# copyright 2025 zamfofex

</$objtype/mkfile

moonfish: chess.$O search.$O main.$O
	$LD $LDFLAGS -o $target $prereq

%.$O: %.c moonfish.h
	$CC $CFLAGS -I/sys/include/npe -Dmoonfish_plan9 $stem.c
