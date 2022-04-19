#!/bin/sh

if [ "$1" != "" ]; then
    SPIN2CPP=$1
    FASTSPIN="$1 -g -q --interp=rom"
else
    SPIN2CPP=../build/spin2cpp
    FASTSPIN="../build/flexspin -g -q --interp=rom"
fi

PROG_ASM="$FASTSPIN -I../Lib"
LOADP1="proploader -Q -r"

CC=propeller-elf-gcc
ok="ok"
endmsg=$ok

#
# run tests on the propeller itself
#

echo "running bytecode tests on propeller..."

if true; then
#
# C tests; obviously no need to test conversion to C
#
for i in cexec*.c
do
  j=`basename $i .c`
    
  # now compile with asm
  if $PROG_ASM -o $j.binary $i; then
    $LOADP1 $j.binary -t -q > $j.txt
  fi
  if diff -ub Expect/$j.txt $j.txt
  then
    echo $j passed for ASM
    rm -f $j.out $j.txt $j.binary $j.pasm
  else
    echo $j failed
    endmsg="TEST FAILURES"
  fi  
done
fi  # end of C tests

if false; then
#
# BASIC tests
#
for i in basexec*.bas
do
  j=`basename $i .bas`
    
  # now compile with asm
  if $PROG_ASM -o $j.binary $i; then
    $LOADP1 $j.binary -t -q > $j.txt
  fi
  if diff -ub Expect/$j.txt $j.txt
  then
    echo $j passed for ASM
    rm -f $j.out $j.txt $j.binary $j.pasm
  else
    echo $j failed
    endmsg="TEST FAILURES"
  fi  
done

fi # end of BASIC tests

#
# Spin tests
#

for i in exec*.spin
do
  j=`basename $i .spin`

  # now compile with asm
  if $PROG_ASM -o $j.binary $i; then
    $LOADP1 $j.binary -t -q > $j.txt
  fi
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
