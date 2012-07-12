#!/bin/sh
PROG=../spin2cpp
CC=propeller-elf-gcc
ok="ok"
endmsg=$ok

# compilation tests
for i in test*.spin
do
  j=`basename $i .spin`
  $PROG $i
  if  diff -ub Expect/$j.h $j.h && diff -ub Expect/$j.cpp $j.cpp
  then
      rm -f $j.h $j.cpp
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done

if [ "x$endmsg" = "x$ok" ]
then
  rm -f *.h *.cpp
fi

# run tests
for i in exec*.spin
do
  j=`basename $i .spin`
  $PROG --main $i
  $CC -Os -u __serial_exit -o $j.elf $j.cpp FullDuplexSerial.cpp
  rm -f $j.out
  propeller-load $j.elf -r -t -q > $j.out
  tail --lines=+6 $j.out >$j.txt
  if diff -ub Expect/$j.txt $j.txt
  then
    echo $j passed
  else
    echo $j failed
    endmsg="TEST FAILURES"
  fi
  rm -f $j.out $j.txt $j.elf $j.cpp $j.h
done

echo $endmsg
