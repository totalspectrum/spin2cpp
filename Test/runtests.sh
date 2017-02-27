#!/bin/sh

if [ "$1" != "" ]; then
  SPIN2CPP=$1
else
  SPIN2CPP=../build/spin2cpp
fi

PROG="$SPIN2CPP -I../Lib"
CC=propeller-elf-gcc
ok="ok"
endmsg=$ok

#
# run tests on the propeller itself
#

echo "running tests on propeller..."
for i in exec*.spin
do
  j=`basename $i .spin`
  if $PROG --binary --ctypes --gas -fpermissive -Os -o $j.binary $i; then
    rm -f $j.out
    propeller-load $j.binary -r -t -q > $j.out
  fi
  # the --lines=+6 skips the first 6 lines that propeller-load printed
  tail --lines=+6 $j.out >$j.txt
  if diff -ub Expect/$j.txt $j.txt
  then
    echo $j passed for C++
    rm -f $j.out $j.txt $j.binary $j.cpp $j.h FullDuplexSerial.cpp FullDuplexSerial.h dattest.cpp dattest.h
  else
    echo $j failed
    endmsg="TEST FAILURES"
  fi

  # now compile with asm
  # --cse forces the common subexpression optimization code
  if $PROG --code=hub --asm --cse --binary -o $j.binary $i; then
    propeller-load $j.binary -r -t -q > $j.out
  fi
  # the --lines=+6 skips the first 6 lines that propeller-load printed
  tail --lines=+6 $j.out >$j.txt
  if diff -ub Expect/$j.txt $j.txt
  then
    echo $j passed for ASM
    rm -f $j.out $j.txt $j.binary $j.pasm
  else
    echo $j failed
    endmsg="TEST FAILURES"
  fi  
done

# clean up
rm -f FullDuplexSerial.cpp FullDuplexSerial.h
echo $endmsg
