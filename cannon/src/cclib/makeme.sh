#! /bin/sh
UNAME=`uname | awk -F\_ '{print $1}'`
PYTHON=`python -c "from sys import version_info as v;print 'python%d.%d'%(v[0],v[1])"`
VERSION=`python -c "from sys import version_info as v;print '%d.%d'%(v[0],v[1])"`
INC="/usr/include/$PYTHON/"
LIB="/usr/lib"
CC=g++
FILE="cclib.cpp sockstrm.cpp ccmodule.cpp ccmod.cpp validate.cpp"
SRCS="cclib.cpp sockstrm.cpp ccmod.cpp validate.cpp"
FLAG="-Wall -fPIC -O3 -g -ggdb"
LIBS=""
OUTPUT="cclib.so"
DYNAMIC="cclib.cc"

if [ "$UNAME" = "CYGWIN" ] ; then
	FLAG="-Wall -O3"
	LIB="/usr/lib/$PYTHON/config/"
	LIBS="-l$PYTHON"
	OUTPUT="cclib.dll"
fi

if [ "$UNAME" = "Darwin" ] ; then
	FLAG="-Wall -O3 -fPIC -dynamiclib"
	#LIB="/Library/Frameworks/Python.framework/Versions/${VERSION}/lib/${PYTHON}/config"
	LIB="/usr/lib/python${VERSION}/config"
	LIBS="$LIB/libpython$VERSION.a"
fi

if [ "$UNAME" = "SunOS" ] ; then
	LIBS="-lsocket -lnsl"
fi

rm -r -f cclib.so *.o > /dev/null
rm -r -f ../../tmp/$OUTPUT > /dev/null

if [ ! -f $INC/Python.h ]
then
	INC="/usr/local/include/$PYTHON/"
	if [ ! -f $INC/Python.h ]
	then
		echo "can not find Python.h in your system" > /dev/stderr
		exit 0
	fi
fi

$CC -shared $FLAG -I"$INC" -L"$LIB" $FILE -o ../../tmp/$OUTPUT $LIBS
$CC -shared $FLAG $SRCS -o ../../tmp/$DYNAMIC

chmod 644 *.h *.c* *.dsp > /dev/null
if [ -f ../../tmp/$OUTPUT ] 
then
	cp ../../tmp/$OUTPUT ../../bin
	if [ "$UNAME" = "CYGWIN" ] ; then
		chmod 755 ../../bin/cclib.dll
	fi
	rm -r -f $OUTPUT
fi

if [ -f ../../tmp/$DYNAMIC ] 
then
	cp ../../tmp/$DYNAMIC ../../bin
	if [ "$UNAME" = "CYGWIN" ] ; then
		chmod 755 ../../bin/$DYNAMIC
	fi
	rm -r -f $OUTPUT
fi

rm -r -f *.plg *.o > /dev/null


