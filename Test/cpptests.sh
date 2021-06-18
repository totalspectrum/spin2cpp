#!/bin/sh

if [ "$1" != "" ]; then
  SPIN2CPP=$1
else
  SPIN2CPP=../${BUILD:='./build'}/spin2cpp
fi

PROG="$SPIN2CPP -I../Lib"
CC=propeller-elf-gcc
ok="ok"
endmsg=$ok

# C mode compilation tests
for i in ctest*.spin
do
  j=`basename $i .spin`
  $PROG --ccode --ctypes --noheader -DCOUNT=4 $i
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
  $PROG --ctypes -n --noheader -DCOUNT=4 $i
  if  diff -ub Expect/$j.h $j.h && diff -ub Expect/$j.cpp $j.cpp
  then
      rm -f $j.h $j.cpp
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done

# some Spin2 examples
for i in test*.spin2
do
  j=`basename $i .spin2`
  $PROG --ctypes -n --noheader -DCOUNT=4 $i
  if  diff -ub Expect/$j.h $j.h && diff -ub Expect/$j.cpp $j.cpp
  then
      rm -f $j.h $j.cpp
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done

# C++ output for Basic code
for i in btest*.bas
do
  j=`basename $i .bas`
  $PROG --fixed --noheader -DCOUNT=4 $i
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
  $PROG --ctypes -g --noheader -DCOUNT=4 $i
  if  diff -ub Expect/$j.h $j.h && diff -ub Expect/$j.cpp $j.cpp
  then
      rm -f $j.h $j.cpp
      echo $j passed
  else
      echo $j failed
      endmsg="TEST FAILURES"
  fi
done

# clean up
rm -f FullDuplexSerial.cpp FullDuplexSerial.h
if [ "x$endmsg" = "x$ok" ]
then
  rm -f [A-Za-r]*.h [A-Za-r]*.cpp
else
  echo $endmsg
  exit 1
fi
