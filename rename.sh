#!/usr/bin/env bash

# moonfish is licensed under the AGPL (v3 or later)
# copyright 2023, 2024 zamfofex

set -e

alphabet=({a..z} {a..z}{a..z} {a..z}{a..z}{a..z})
alphabet2=("${alphabet[@]}")
declare -A names

functions="main printf fprintf strtol fgets fflush stdin stdout stderr strcmp strcpy strtok strstr malloc realloc free exit errno clock_gettime timespec tv_sec tv_nsec pthread_create pthread_join pthread_t typedef moonfish_type_t"
keywords="do while for if else break continue return static struct enum unsigned signed long short int char void sizeof $functions"

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
		if [[ "$name" = moonfish ]]
		then short=F
		elif [[ "$name" =~ ^moonfish ]]
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
		
		names["$name"]="$short"
	fi
	
	echo "$short"
done
