#!/bin/sh

if [ "$1" != "" ]; then
  SPIN2CPP=$1
else
  SPIN2CPP=../build/spin2cpp
fi

PROG="$SPIN2CPP -I../Lib"
ok="ok"
endmsg=$ok

# P2 ASM mode compilation tests
for i in *.p2asm
do
  j=`basename $i .p2asm`
  $PROG --p2 --dat $i
  if  diff -ub Expect/$j.obj $j.dat
  then
      rm -f $j.dat
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done

# clean up
if [ "x$endmsg" = "x$ok" ]
then
  exit 0
else
  echo $endmsg
  exit 1
fi
