#!/bin/bash
if [[ $# -lt 2 ]]
then
  printf "Usage:\n"
  printf "\t%s <iterations> <input_file>\n" $0
  exit
fi

in_file="$(basename $2 .ppm)"
csv_file="img$in_file-B$1"
printf "scale1,scale2,scale3,scale4,factor\n" > "$csv_file"
for i in 2 5 8 11 16
do
  for j in {1..4}
  do
    timing=$(./main -V$j -B$1 -f$i $2 -o /dev/null | cut -d ' ' -f2)
    printf "%s," "${timing::-1}" >> "$csv_file"
  done
  printf "%d\n" $i >> "$csv_file"
done
