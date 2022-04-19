#!/bin/sh

TEST_C="no"

if [ "$1" != "" ]; then
    SPIN2CPP=$1
    FASTSPIN="$1 --asm --binary --code=hub"
else
    SPIN2CPP=../build/spin2cpp
    FASTSPIN="../build/flexspin -g -q"
fi

PROG_C="$SPIN2CPP -I../Lib"
PROG_ASM="$FASTSPIN -I../Lib"
#LOADER="loadp2 -b230400"
LOADER="proploader -Q -D loader-baud-rate=115200 -D baud-rate=115200 -r -t"

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
    
#  if $PROG_C --binary --ctypes --gas -fpermissive -Os -o $j.binary $i; then
#    rm -f $j.out
#    propeller-load $j.binary -r -t -q > $j.out
#  fi
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
    $LOADER $j.binary -t -q > $j.txt
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

#
# C tests; obviously no need to test conversion to C
#
for i in cexec*.c
do
  j=`basename $i .c`
    
  # now compile with asm
  if $PROG_ASM -o $j.binary $i; then
    $LOADER $j.binary -t -q > $j.txt
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

#
# Spin tests
#

for i in exec*.spin
do
  j=`basename $i .spin`
  j=`basename $j .bas`
  if [ "$TEST_C" != "no" ]; then
    echo "TEST_C = ($TEST_C)"
    if $PROG_C --binary --ctypes --gas -fpermissive -Os -o $j.binary $i; then
      rm -f $j.out
      $LOADER $j.binary -t -q > $j.txt
    fi
    if diff -ub Expect/$j.txt $j.txt
    then
      echo $j passed for C++
      rm -f $j.out $j.txt $j.binary $j.cpp $j.h FullDuplexSerial.cpp FullDuplexSerial.h dattest.cpp dattest.h setabort.cpp setabort.h
    else
      echo $j failed
      endmsg="TEST FAILURES"
    fi
  fi
  
  # now compile with asm
  if $PROG_ASM -o $j.binary $i; then
    $LOADER $j.binary -t -q > $j.txt
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
