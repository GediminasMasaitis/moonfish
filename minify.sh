#!/usr/bin/env bash

# moonfish is licensed under the AGPL (v3 or later)
# copyright 2023 zamfofex

# for every C source file
cat moonfish.h chess.c search.c main.c |

# remove the '#' from system '#include'
sed 's/^#\(include <\)/\1/g' |

# preprocess the file, add '#' back to 'include'
gcc -E -DMOONFISH_HAS_PTHREAD -Dinclude='#include' - |

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
sed 's/\('"'"'\(\\.\|.\)*'"'"'\)/\n\1\n/g' |

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

# store the result into a file
tee moonfish.c |

# and also compress it
xz -9 > moonfish.c.xz

# also make it into a runnable program
cat - moonfish.c.xz > moonfish.sh << END
#!/bin/sh
t=\`mktemp\`
tail -n +5 "\$0"|xz -d|cc -O3 -o \$t -xc -
(sleep 3;rm \$t)&exec \$t
END
chmod +x moonfish.sh
