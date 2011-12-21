#!/bin/sh
PROG=../spin2c
endmsg="ok"

for i in test*.spin
do
  j=`basename $i .spin`
  $PROG $i
#  diff -u $j.h Expect/$j.h && diff -u $j.cpp Expect/$j.cpp
  if diff -u $j.h Expect/$j.h
  then
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done
rm -f *.h *.cpp
echo $endmsg
