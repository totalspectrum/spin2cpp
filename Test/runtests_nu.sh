#!/bin/sh

if [ "$1" != "" ]; then
    SPIN2CPP=$1
    FASTSPIN="$1 --asm --binary --code=hub"
else
    SPIN2CPP=../build/spin2cpp
    FASTSPIN="../build/flexspin -2nu -q"
fi

PROG_C="$SPIN2CPP -I../Lib"
PROG_ASM="$FASTSPIN -I../Lib"
LOADP2="loadp2 -b230400"
#LOADP2="propeller-load -r"

CC=propeller-elf-gcc
ok="ok"
endmsg=$ok

#
# run tests on the propeller itself
#

echo "running tests on propeller..."

if true; then
#
# BASIC tests; C++ support is still incomplete
#
for i in basexec*.bas
do
  j=`basename $i .bas`
    
#  if $PROG_C --binary --ctypes --gas -fpermissive -Os -o $j.binary $i; then
#    rm -f $j.out
#    propeller-load $j.binary -r -t -q > $j.out
#  fi
#  # the --lines=+6 skips the first 6 lines that propeller-load printed
#  tail --lines=+6 $j.out >$j.txt
#  if diff -ub Expect/$j.txt $j.txt
#  then
#    echo $j passed for C++
#    rm -f $j.out $j.txt $j.binary $j.cpp $j.h FullDuplexSerial.cpp FullDuplexSerial.h dattest.cpp dattest.h
#  else
#    echo $j failed
#    endmsg="TEST FAILURES"
#  fi

  # now compile with asm
  if $PROG_ASM -o $j.binary $i; then
      $LOADP2 $j.binary -t -q > $j.out
  else
      echo $j failed
      cp Expect/$j.txt $j.txt
      endmsg="TEST FAILURES"
  fi
  # the --lines=+6 skips the first 6 lines that propeller-load printed
  #tail --lines=+6 $j.out >$j.txt
  cp $j.out $j.txt
  if diff -ub Expect/$j.txt $j.txt
  then
    echo $j passed for ASM
    rm -f $j.out $j.txt $j.binary $j.p2asm
  else
    echo $j failed
    endmsg="TEST FAILURES"
  fi  
done
fi

if true; then
#
# C tests; obviously no need to test conversion to C
#
for i in cexec*.c
do
  j=`basename $i .c`
    
  # now compile with asm
  if $PROG_ASM -o $j.binary $i; then
    $LOADP2 $j.binary -t -q > $j.out
  else
      echo $j failed
      cp Expect/$j.txt $j.txt
      endmsg="TEST FAILURES"
  fi
  # the --lines=+6 skips the first 6 lines that propeller-load printed
  #tail --lines=+6 $j.out >$j.txt
  cp $j.out $j.txt
  if diff -ub Expect/$j.txt $j.txt
  then
    echo $j passed for ASM
    rm -f $j.out $j.txt $j.binary $j.p2asm
  else
    echo $j failed
    endmsg="TEST FAILURES"
  fi  
done
fi

if true; then
#
# Spin tests
#

for i in exec*.spin
do
  j=`basename $i .spin`
  # now compile with asm
  if $PROG_ASM -o $j.binary $i; then
    $LOADP2 $j.binary -t -q > $j.out
  else
      echo $j failed
      cp Expect/$j.txt $j.txt
      endmsg="TEST FAILURES"
  fi
  # the --lines=+6 skips the first 6 lines that propeller-load printed
  #tail --lines=+6 $j.out >$j.txt
  cp $j.out $j.txt
  if diff -ub Expect/$j.txt $j.txt
  then
    echo $j passed for ASM
    rm -f $j.out $j.txt $j.binary $j.p2asm
  else
    echo $j failed
    endmsg="TEST FAILURES"
  fi  
done
fi

# clean up
rm -f FullDuplexSerial.cpp FullDuplexSerial.h
echo $endmsg
if [ "$endmsg" != "$ok" ]; then
  exit 1
fi
