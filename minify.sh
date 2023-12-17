#!/usr/bin/env bash

# moonfish is licensed under the AGPL (v3 or later)
# copyright 2023 zamfofex

set -e

# for every C source file
cat moonfish.h chess.c search.c main.c |

# remove the '#' from system '#include'
sed 's/^#\(include <\)/\1/g' |

# preprocess the file, add '#' back to 'include'
gcc -E -Dinclude='#include' - |

# remove lines starting with '# '
sed '/^# /d' |

# place all '#include' lines on top
# note: this materialises the whole file!
tee xxx.txt |
( txt="$(tee)" && { grep '^ #' <<< "$txt" ; } && { grep -v '^ #' <<< "$txt" ; } ) |

# remove all line breaks (replace them with spaces)
tr '\n' ' ' |

# put line breaks around string literals
sed 's/\("\(\\"\|[^"]\)*"\)/\n\1\n/g' |

# put line breaks around character literals
sed 's/\('"'"'\(\\.\|.\)'"'"'\)/\n\1\n/g' |

# in every line that isn't a string literal,
# join all adjacent white space into a single tab
sed '/^[^"'"'"']/s/[\t ]\+/\t/g' |

# remove inserted line breaks
tr -d '\n' |

# replace tabs between alphanumeric characters with a single space
# note: there are no tabs inside string literals
sed 's/\([a-z0-9_]\)\t\([a-z0-9_]\)/\1 \2/gi' |

# remove all tab characters
tr -d '\t' |

# put line breaks back around '#include' lines again
# note: there is no white space within include file names
sed 's/\(#include<[^>]*>\)/\n\1\n/g' |

# remove all empty lines
sed '/^$/d' |

# remove duplicate lines (for '#include')
awk '!x[$0]++' |

# replace common words
sed 's/\(\b\|_\)moonfish\(\b\|_\)/_F_/g' |
sed 's/\(\b\|_\)white\(\b\|_\)/_W_/g' |
sed 's/\(\b\|_\)black\(\b\|_\)/_Y_/g' |
sed 's/\(\b\|_\)pawn\(\b\|_\)/_P_/g' |
sed 's/\(\b\|_\)knight\(\b\|_\)/_L_/g' |
sed 's/\(\b\|_\)bishop\(\b\|_\)/_B_/g' |
sed 's/\(\b\|_\)rook\(\b\|_\)/_R_/g' |
sed 's/\(\b\|_\)queen\(\b\|_\)/_Q_/g' |
sed 's/\(\b\|_\)king\(\b\|_\)/_K_/g' |
sed 's/\(\b\|_\)chess\(\b\|_\)/_X_/g' |
sed 's/\(\b\|_\)score\(\b\|_\)/_S_/g' |
sed 's/\(\b\|_\)board\(\b\|_\)/_D_/g' |
sed 's/\(\b\|_\)castle\(\b\|_\)/_O_/g' |
sed 's/\(\b\|_\)move\(\b\|_\)/_M_/g' |
sed 's/\(\b\|_\)moves\(\b\|_\)/_N_/g' |
sed 's/\(\b\|_\)from\(\b\|_\)/_A_/g' |
sed 's/\(\b\|_\)to\(\b\|_\)/_Z_/g' |
sed 's/\(\b\|_\)piece\(\b\|_\)/_G_/g' |
sed 's/\(\b\|_\)promotion\(\b\|_\)/_H_/g' |
sed 's/\(\b\|_\)color\(\b\|_\)/_C_/g' |
sed 's/\(\b\|_\)captured\(\b\|_\)/_R_/g' |
sed 's/\(\b\|_\)empty\(\b\|_\)/_E_/g' |
sed 's/\(\b\|_\)type\(\b\|_\)/_T_/g' |
sed 's/\(\b\|_\)outside\(\b\|_\)/_J_/g' |

# collapse underscores
sed 's/_\+/_/g' |
sed 's/\b_//g' |
sed 's/_\b//g' |

# restore significant underscore
# (hacky, but it works)
sed 's/\(SC_NPROCESSORS_ONLN\)/_\1/g' |

# store the result into a file
tee moonfish.c |

# and also compress it
xz -9 > moonfish.c.xz

# also make it into a runnable program
cat - moonfish.c.xz > moonfish.sh << END
#!/bin/sh
t=\`mktemp\`
tail -n +5 "\$0"|xz -d|cc -march=native -O3 -o \$t -xc - -pthread
(sleep 3;rm \$t)&exec \$t
END
chmod +x moonfish.sh
