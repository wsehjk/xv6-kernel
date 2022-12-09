#! /usr/bin/env bash
func(){
	echo "the number of arguments is $#"
	i=1
	for arg in "$@"
	do
		echo "$i: $arg"
		i=$((i+1))
	done
}
func $@ 
echo "the name of script is $0"

