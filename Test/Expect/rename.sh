#!/bin/sh
for i in test*.c* test*.h
do	 
    sed -i 's/static char dat/static unsigned char dat/g' $i
done
