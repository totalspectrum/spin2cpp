#!/bin/sh

if [ "$1" != "" ]; then
  FASTSPIN=$1
else
  FASTSPIN=../build/flexspin
fi

PROG="$FASTSPIN -q -2b -I../Lib"
ok="ok"
endmsg=$ok

# P2 ASM mode compilation tests
# eventually name these all something like bin_xxx
for i in [a-c]*.spin2
do
  j=`basename $i .spin2`
  $PROG $i
  if  diff -ub Expect/$j.obj $j.binary
  then
      rm -f $j.binary $j.p2asm
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
