#!/bin/sh

if [ "$1" != "" ]; then
    SPIN2CPP=$1
    FASTSPIN="$1 --asm --binary --code=hub"
else
    SPIN2CPP=../build/spin2cpp
    FASTSPIN="../build/flexspin -2 -O2 -g -q"
fi

PROG_C="$SPIN2CPP -I../Lib"
PROG_ASM="$FASTSPIN -I../Lib"
LOADP2="loadp2 -q -b230400"
#LOADP2="proploader -2 -D baud-rate=230400 -Q -r"
#LOADP2="propeller-load -r"

CC=propeller-elf-gcc
ok="ok"
endmsg=$ok

#
# run tests on the propeller itself
#

echo "running tests on propeller..."


#
# BASIC tests; C++ support is still incomplete
#
for i in basexec*.bas
do
  j=`basename $i .bas`
    
  # now compile with asm
  if $PROG_ASM -o $j.binary $i; then
    $LOADP2 $j.binary -t -q > $j.txt
  fi
  if diff -ub Expect/$j.txt $j.txt
  then
    echo $j passed for ASM
    rm -f $j.out $j.txt $j.binary $j.p2asm
  else
    echo $j failed
    endmsg="TEST FAILURES"
  fi  
done

#
# C tests; obviously no need to test conversion to C
#
for i in cexec*.c
do
  j=`basename $i .c`
    
  # now compile with asm
  if $PROG_ASM -o $j.binary $i; then
    $LOADP2 $j.binary -t -q > $j.txt
  fi
  if diff -ub Expect/$j.txt $j.txt
  then
    echo $j passed for ASM
    rm -f $j.out $j.txt $j.binary $j.p2asm
  else
    echo $j failed
    endmsg="TEST FAILURES"
  fi  
done

#
# Spin tests
#

for i in exec*.spin
do
  j=`basename $i .spin`
  # now compile with asm
  if $PROG_ASM -o $j.binary $i; then
    $LOADP2 $j.binary -t -q > $j.txt
  fi
  if diff -ub Expect/$j.txt $j.txt
  then
    echo $j passed for ASM
    rm -f $j.out $j.txt $j.binary $j.p2asm
  else
    echo $j failed
    endmsg="TEST FAILURES"
  fi  
done

#
# Spin2 tests: these are P2 only
#
for i in exec*.spin2
do
  j=`basename $i .spin2`
  # now compile with asm
  if $PROG_ASM -o $j.binary $i; then
    $LOADP2 $j.binary -t -q > $j.txt
  fi
  if diff -ub Expect/$j.txt $j.txt
  then
    echo $j passed for ASM
    rm -f $j.out $j.txt $j.binary $j.p2asm
  else
    echo $j failed
    endmsg="TEST FAILURES"
  fi  
done

# clean up
rm -f FullDuplexSerial.cpp FullDuplexSerial.h
echo $endmsg
if [ "$endmsg" != "$ok" ]; then
  exit 1
fi
