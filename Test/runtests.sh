#!/bin/sh

if [ "$1" != "" ]; then
  PROG=$1
else
  PROG=../build/spin2cpp
fi
CC=propeller-elf-gcc
ok="ok"
endmsg=$ok

# C mode compilation tests
for i in ctest*.spin
do
  j=`basename $i .spin`
  $PROG --ccode --noheader -DCOUNT=4 $i
  if  diff -ub Expect/$j.h $j.h && diff -ub Expect/$j.c $j.c
  then
      rm -f $j.h $j.c
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done

# C++ compilation tests
# These use the -n flag to normalize identifier names because
# I'm too lazy to convert all the identifiers; it also gives -n
# a good workout.

for i in test*.spin
do
  j=`basename $i .spin`
  $PROG -n --noheader -DCOUNT=4 $i
  if  diff -ub Expect/$j.h $j.h && diff -ub Expect/$j.cpp $j.cpp
  then
      rm -f $j.h $j.cpp
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done

#
# debug directive tests
#
for i in gtest*.spin
do
  j=`basename $i .spin`
  $PROG -g --noheader -DCOUNT=4 $i
  if  diff -ub Expect/$j.h $j.h && diff -ub Expect/$j.cpp $j.cpp
  then
      rm -f $j.h $j.cpp
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done

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

#
# now see about tests on the propeller itself
#
if [ "x$endmsg" = "x$ok" ]
then
  rm -f *.h *.cpp
else
  echo $endmsg
  echo "skipping execute tests"
  exit 1
fi

# execute tests
echo "running tests on propeller..."
for i in exec*.spin
do
  j=`basename $i .spin`
  $PROG --elf -Os -u __serial_exit -o $j.elf $i
  rm -f $j.out
  propeller-load $j.elf -r -t -q > $j.out
  # the --lines=+6 skips the first 6 lines that propeller-load printed
  tail --lines=+6 $j.out >$j.txt
  if diff -ub Expect/$j.txt $j.txt
  then
    echo $j passed
    rm -f $j.out $j.txt $j.elf $j.cpp $j.h FullDuplexSerial.cpp FullDuplexSerial.h dattest.cpp dattest.h
  else
    echo $j failed
    endmsg="TEST FAILURES"
  fi
done

# clean up
rm -f FullDuplexSerial.cpp FullDuplexSerial.h
echo $endmsg
