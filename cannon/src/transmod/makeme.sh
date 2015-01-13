#!/bin/sh
UNAME=`uname | awk -F\_ '{print $1}'`
FLAG1="-fpic -g -Wall -O3"
FLAG2="-fpic"

if [ "$UNAME" = "CYGWIN" ] ; then
	FLAG1="-Wall -g -O3"
	FLAG2=""
fi
if [ "$UNAME" = "Darwin" ] ; then
	FLAG1="-Wall -g -fPIC -O3"
	FLAG2="-dynamiclib"
fi
if [ "$UNAME" = "SunOS" ] ; then
	FLAG1="-Wall -g -Wall -fpic -O3"
	FLAG2="-lsocket -lnsl"
fi

mkdir -p ../../tmp
cd ../../tmp
mkdir transmod 2> /dev/null
cd transmod
rm -r -f *.o

gcc -c -Wall $FLAG1 ../../src/transmod/*.c 
gcc *.o -shared $FLAG2 -o transmod.so

cp transmod.so ../../bin 2> /dev/null
if [ "$UNAME" = "CYGWIN" ] ; then
	chmod 755 ../../bin/transmod.so
fi
cd ../../src/transmod/
rm -r -f *.plg
chmod 644 *.c *.h *.dsp 2> /dev/null



