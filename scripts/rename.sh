#!/usr/bin/env bash

# moonfish's license: 0BSD
# copyright 2025 zamfofex

set -e

alphabet=({a..z} {a..z}{a..z} {a..z}{a..z}{a..z})
alphabet2=("${alphabet[@]}")
declare -A names

functions="main fopen fread printf fprintf sscanf fgets fflush stdin stdout stderr strcmp strncmp strcpy strtok strstr strchr malloc realloc free exit errno clock_gettime timespec tv_sec tv_nsec typedef memmove fabs sqrt log pow qsort thrd_t atomic_compare_exchange_strong atomic_fetch_add thrd_create thrd_success thrd_join sysconf"
keywords="do while for if else switch case break continue return extern static struct enum unsigned signed long short int char double float void const sizeof $functions"

while read -r name
do
	if ! [[ "$name" =~ ^[a-z] ]]
	then
		echo "$name"
		continue
	fi
	
	if [[ " $keywords " =~ " $name " ]]
	then
		echo "$name"
		continue
	fi
	
	short="${names["$name"]}"
	if [[ "$short" = "" ]]
	then
		if [[ "$name" =~ ^moonfish ]]
		then
			short="F${alphabet[0]}"
			alphabet=("${alphabet[@]:1}")
		else
			while :
			do
				short="${alphabet2[0]}"
				alphabet2=("${alphabet2[@]:1}")
				if ! [[ " $keywords " =~ " $short " ]]
				then break
				fi
			done
		fi
		
		echo "renaming '$name' to '$short'" >&2
		names["$name"]="$short"
	fi
	
	echo "$short"
done
