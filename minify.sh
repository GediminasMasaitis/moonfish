#!/usr/bin/env bash

# moonfish is licensed under the AGPL (v3 or later)
# copyright 2023 zamfofex

set -e

# for every C source file
cat moonfish.h chess.c search.c main.c |

# remove the '#' from system '#include'
sed 's/^#\(include <\)/\1/g' |

# preprocess the file, add '#' back to 'include', remove debug statements
gcc -E -Dinclude='#include' -D'perror(...)=' -D'fprintf(...)=' -D'free(...)=' -Dmoonfish_mini - |

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

# replace long/common words (ugly but effective)
sed '/^[^"'"'"']/s/\(\b\|_\)moonfish\(\b\|_\)/\1F\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)white\(\b\|_\)/\1W\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)black\(\b\|_\)/\1Y\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)pawn\(\b\|_\)/\1P\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)knight\(\b\|_\)/\1L\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)bishop\(\b\|_\)/\1B\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)rook\(\b\|_\)/\1R\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)queen\(\b\|_\)/\1Q\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)king\(\b\|_\)/\1K\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)chess\(\b\|_\)/\1X\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)score\(\b\|_\)/\1S\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)board\(\b\|_\)/\1D\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)castle\(\b\|_\)/\1O\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)move\(\b\|_\)/\1M\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)moves\(\b\|_\)/\1N\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)from\(\b\|_\)/\1A\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)to\(\b\|_\)/\1Z\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)piece\(\b\|_\)/\1G\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)promotion\(\b\|_\)/\1H\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)color\(\b\|_\)/\1C\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)captured\(\b\|_\)/\1U\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)empty\(\b\|_\)/\1E\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)type\(\b\|_\)/\1T\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)outside\(\b\|_\)/\1J\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)repetition\(\b\|_\)/\1V\2/g' |
sed '/^[^"'"'"']/s/\(\b\|_\)account\(\b\|_\)/\1I\2/g' |

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

# store the result into a file
tee moonfish.c |

# and also compress it
xz -9 -e > moonfish.c.xz

# also make it into a runnable program
cat - moonfish.c.xz > moonfish.sh << END
#!/bin/sh
t=\`mktemp\`
tail -n +5 "\$0"|xz -d|cc -march=native -O3 -o \$t -xc - -pthread
(sleep 3;rm \$t)&exec \$t
END
chmod +x moonfish.sh
