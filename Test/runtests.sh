#!/bin/sh
PROG=../spin2c
endmsg="ok"

for i in test*.spin
do
  j=`basename $i .spin`
  $PROG $i
  if  diff -u $j.h Expect/$j.h && diff -u $j.cpp Expect/$j.cpp
  then
      rm -f $j.h $j.cpp
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done
echo $endmsg
