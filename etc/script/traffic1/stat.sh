#!/bin/sh

data=`cat | grep -v '^$' | tr "\n" "," | sed -e 's/,$//'`

tmp="0"
count=0
IFS=","
for i in $data; do
  tmp=$tmp"+$i"
  count=`echo $count + 1 | bc `
done

avg=`echo "scale=5; ("$tmp")" "/" $count ";" | bc`
if test "$1" = "avg"; then
  echo $avg
  exit
fi

tmp="0"
for i in $data; do
  tmp=$tmp"+ ( $i - $avg )^2"
done

std=`echo "scale=5; sqrt(1/$count *  ($tmp))"  | bc`
if test "$1" = "std"; then
  echo $std
  exit
fi
