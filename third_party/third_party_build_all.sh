#!/bin/bash
set -e

cd "$(dirname "$0")"

FOLDER_NEED_TO_BUILD=("GmSSL" "libevent" "msgpack")

for subdir in */ ; do
	subdir=${subdir%/}
	if [[ " ${FOLDER_NEED_TO_BUILD[*]} " == *" $subdir "* ]]; then
		if [ "$subdir" == "msgpack" ]; then
			pushd "$subdir/cpp" > /dev/null
			echo "Building msgpack-c"
			# compile sources directly (avoids autotools out-of-source rebuild loop)
			rm -rf build && mkdir -p build/lib build/include/msgpack
			# generate version.h (normally created by configure)
			sed -e 's/@VERSION@/0.5.6/g; s/@VERSION_MAJOR@/0/g; s/@VERSION_MINOR@/5/g' \
				src/msgpack/version.h.in > src/msgpack/version.h
			gcc -c -O2 -I src -I . -o build/unpack.o  src/unpack.c
			gcc -c -O2 -I src -I . -o build/objectc.o src/objectc.c
			gcc -c -O2 -I src -I . -o build/version.o src/version.c
			gcc -c -O2 -I src -I . -o build/vrefbuffer.o src/vrefbuffer.c
			gcc -c -O2 -I src -I . -o build/zone.o    src/zone.c
			g++ -c -O2 -I src -I . -o build/object.o  src/object.cpp
			ar cr build/lib/libmsgpackc.a build/unpack.o build/objectc.o build/version.o build/vrefbuffer.o build/zone.o
			ar cr build/lib/libmsgpack.a  build/unpack.o build/objectc.o build/version.o build/vrefbuffer.o build/zone.o build/object.o
			cp src/msgpack/*.hpp build/include/msgpack/
			cp src/msgpack/*.h   build/include/msgpack/ 2>/dev/null || true
			popd > /dev/null
		else
			pushd "$subdir" > /dev/null
			echo "Building $subdir"
			rm -rf build && mkdir build && pushd build > /dev/null
			cmake -DBUILD_SHARED_LIBS=OFF ..
	#		cmake ..
			make -B -j$(nproc)
			popd > /dev/null
			popd > /dev/null
		fi
	fi
done
