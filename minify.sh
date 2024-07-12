#!/usr/bin/env bash

# moonfish is licensed under the AGPL (v3 or later)
# copyright 2023, 2024 zamfofex

set -e

# for each C source file
cat moonfish.h chess.c search.c mini.c |

# replace 'unsigned char' with 'int'
sed 's/\bunsigned char\b/int/g' |

# remove 'signed' and 'unsigned'
sed 's/\tsigned /\t/g' |
sed 's/\tunsigned /\t/g' |

# remove 'long'
sed 's/\blong\b \?//g' |

# remove top-level 'static', 'int' and 'void'
sed 's/^static\b//g' |
sed 's/^int\b//g' |
sed 's/^void\b//g' |

#remove redundant 'int'
sed 's/\bstatic int\b/static/g' |

# remove the '#' from system '#include'
sed 's/^#\(include <\)/\1/g' |

# preprocess the file, add '#' back to 'include'
# note: this materialises the whole file
gcc -E -Dinclude='#include' -Dmoonfish_mini - |

# remove lines starting with '# '
sed '/^# /d' |

# remove leading white space from '#include' lines
sed 's/^[\t ]\+#/#/g' |

# place all '#include' lines on top
# note: this materialises the whole file
( txt="$(tee)" && { grep '^#' <<< "$txt" || : ; grep -v '^#' <<< "$txt" ; } ) |

# put line breaks around string literals
sed 's/\("\(\\.\|[^"]\)*"\)/\n\1\n/g' |

# put line breaks around character literals
sed 's/\('"'"'\(\\.\|.\)*'"'"'\)/\n\1\n/g' |

# in every line that isn't a string literal or '#include' line,
# put line breaks around identifiers
sed '/^[^"'"'"'#]/s/[a-z0-9_]\+/\n\0\n/gi' |

# rename identifiers to be shorter
./rename.sh |

# replace all white space with tabs (except inside string literals)
# note: this makes the next 'sed' materialise the whole file
sed '/^[^"'"'"']/s/[\t ]\+/\t/g' |
tr '\n' '\t' |

# replace tabs between alphanumeric characters with a single space
# note: there are no tabs inside string literals
sed 's/\([a-z0-9_]\)\t\+\([a-z0-9_]\)/\1 \2/gi' |

# remove all tab characters
tr -d '\t' |

# put line breaks back around '#include' lines again
# (also remove whitespace betwen '#include' and '<')
# note: there is no white space within include file names
sed 's/#include<[^>]*>/\n\0\n/g' |

# remove all empty lines
sed '/^$/d' |

# remove duplicate lines (for '#include')
awk '!x[$0]++' |

# store the result into a file
tee moonfish.c |

# and also compress it
xz -e9qFraw > moonfish.c.xz

# finally, make it into an executable program
cat - moonfish.c.xz > moonfish.sh << END
#!/bin/sh
t=\`mktemp\`
${CC:-cc} -O3 -o \$t -xc <(tail -n+5 "\$0"|unxz -Fraw)
(sleep 3;rm \$t)&exec \$t
END
chmod +x moonfish.sh
