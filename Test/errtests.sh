#!/bin/sh

if [ "$1" != "" ]; then
  PROG=$1
else
  PROG=../build/spin2cpp
fi
CC=propeller-elf-gcc
ok="ok"
endmsg=$ok

#
# check error messages
#
for i in error*.spin
do
  j=`basename $i .spin`
  $PROG --noheader -DCOUNT=4 $i >$j.err 2>&1
  if  diff -ub Expect/$j.err $j.err
  then
      rm -f $j.err
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done

for i in error*.bas
do
  j=`basename $i .bas`
  $PROG --noheader -DCOUNT=4 $i >$j.err 2>&1
  if  diff -ub Expect/$j.err $j.err
  then
      rm -f $j.err
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done

#
# now see about tests on the propeller itself
#
if [ "x$endmsg" = "x$ok" ]
then
  rm -f *.h *.cpp
else
  echo $endmsg
  exit 1
fi

