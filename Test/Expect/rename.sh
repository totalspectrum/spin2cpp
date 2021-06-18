#!/bin/sh
for i in ../stest*.c
do	 
    b=`basename $i .c`
    echo mv $b.pasm $b_c.pasm
done
