#!/bin/sh
PROG=../spin2cpp
endmsg="ok"

for i in test*.spin
do
  j=`basename $i .spin`
  $PROG $i
  if  diff -u Expect/$j.h $j.h && diff -u Expect/$j.cpp $j.cpp
  then
      rm -f $j.h $j.cpp
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done
echo $endmsg
