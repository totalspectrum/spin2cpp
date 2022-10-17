#!/bin/sh

if [ "$1" != "" ]; then
  FASTSPIN=$1
else
  FASTSPIN=../build/flexspin
fi

PROG="$FASTSPIN -I../Lib"
ok="ok"
endmsg=$ok

# Bytecode mode compilation tests
for i in bctest*.spin
do
  j=`basename $i .spin`
  # NOTE: all O1 except
  #   remove_unused_funcs (0x01)
  #   remove_hub_bss (0x04)
  $PROG -1bc -q -O1,!remove-unused,!remove-bss --test-listing $i
  if  diff -ub Expect/$j.lst $j.lst
  then
      rm -f $j.lst
      rm -f $j.binary
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done
: <<'END'
for i in bctest*.spin2
do
  j=`basename $i .spin2`
  # NOTE: optimize 250 is all optimizations except
  #   remove_unused_funcs (0x01)
  #   remove_hub_bss (0x04)
  $PROG --p2 --asm --optimize 'all,!remove-unused,!remove-bss' --noheader $i
  if  diff -ub Expect/$j.p2asm $j.p2asm
  then
      rm -f $j.binary
      rm -f $j.lst
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done

for i in bctest*.bas
do
  j=`basename $i .bas`
  # NOTE: optimize 250 is all optimizations except
  #   remove_unused_funcs (0x01)
  #   remove_hub_bss (0x04)
  $PROG --asm --optimize 'all,!remove-unused,!remove-bss' --noheader $i
  if  diff -ub Expect/$j.pasm $j.pasm
  then
      rm -f $j.binary
      rm -f $j.lst
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done

for i in bctest*.c
do
  j=`basename $i .c`
  # NOTE: optimize 250 is all optimizations except
  #   remove_unused_funcs (0x01)
  #   remove_hub_bss (0x04)
  $PROG --asm --optimize 'all,!remove-unused,!remove-bss' --noheader $i
  if  diff -ub Expect/$j.pasm $j.pasm
  then
      rm -f $j.binary
      rm -f $j.lst
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done

for i in bctest*.cpp
do
  j=`basename $i .cpp`
  # NOTE: optimize 250 is all optimizations except
  #   remove_unused_funcs (0x01)
  #   remove_hub_bss (0x04)
  $PROG --asm --optimize 'all,!remove-unused,!remove-bss' --noheader $i
  if  diff -ub Expect/$j.pasm $j.pasm
  then
      rm -f $j.binary
      rm -f $j.lst
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done
END

# clean up
if [ "x$endmsg" = "x$ok" ]
then
  rm -f *.binary
  rm -f *.lst
else
  echo $endmsg
  exit 1
fi
