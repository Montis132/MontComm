#!/bin/bash
set -ex

cd "$(dirname "$0")"

FOLDER_NEED_TO_BUILD=("GmSSL" "libevent" "msgpack")

for subdir in */ ; do
	subdir=${subdir%/}
	if [[ " ${FOLDER_NEED_TO_BUILD[*]} " == *" $subdir "* ]]; then
		if [ "$subdir" == "msgpack" ]; then
			pushd "$subdir/cpp" > /dev/null
			echo "Building msgpack-c"
			if [ -f Makefile ]; then
				make distclean 2>/dev/null || true
			fi
			if [ ! -f configure ]; then
				./bootstrap
			fi
			rm -rf build && mkdir build && pushd build > /dev/null
			../configure --prefix=$(pwd) --enable-static --disable-shared
			make -B -C src
			make -C src install
			popd > /dev/null
			popd > /dev/null
		else
			pushd "$subdir" > /dev/null
			echo "Building $subdir"
			rm -rf build && mkdir build && pushd build > /dev/null
			cmake -DBUILD_SHARED_LIBS=OFF ..
			make -B -j$(nproc)
			popd > /dev/null
			popd > /dev/null
		fi
	fi
done
