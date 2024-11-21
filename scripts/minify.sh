#!/bin/sh -e

# moonfish is licensed under the AGPL (v3 or later)
# copyright 2023, 2024 zamfofex

cc="${HOST_CC:-gcc}"

header='#!/bin/sh
t=`mktemp`
tail -n+5 "$0"|unxz -Fraw|'""${CC:-cc}""' -ansi -O3 -o $t -xc - -lm
(sleep 3;rm $t)&exec $t'

# for each C source file
cat moonfish.h chess.c search.c mini.c |

# remove the '#' from system '#include'
sed -E 's/^#(include <)/\1/g' |

# preprocess the file, add '#' back to 'include'
# note: this materialises the whole file
"$cc" -E -P -Dinclude='#include' -Dmoonfish_mini -D'exit(...)=' -D'perror(...)=' -D'fprintf(...)=' - |

# replace tabs with spaces
tr '\t' ' ' |

# remove leading white space from '#include' lines
sed -E 's/^ +#/#/g' |

# tokenise the file (roughly)
grep -Eoh "'[^']*'"'|"[^"]*"|#.*|[a-zA-Z0-9_]+|[^a-zA-Z0-9_"'"'"' ]+' |

# remove spaces in '#include' lines
sed '/^#/s/ //g' |

# put '#include' lines at the start
# (note: this materialises the file)
awk '/^#/ { print $0 ; next } { x = x $0 "\n" } END { print x }' |

# rename identifiers to be shorter
scripts/rename.sh |

# put spaces between identifiers
awk '/^[^a-zA-Z0-9]/ { n = 0 ; print $0 ; next } { if (++n > 1) print " " ; } 1' |

# remove line breaks (except in '#include' lines)
awk '/^#/ { print $0 ; next } { printf "%s", $0 }' |

# remove duplicate '#include' lines
awk '!x[$0]++' |

# store it into a file
tee moonfish.c |

# compress it
xz -e9qFraw |

# finally, create a shell script for it
{ echo "$header" ; tee ; } > moonfish.sh

# and make it executable
chmod +x moonfish.sh
