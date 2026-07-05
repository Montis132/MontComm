#!/bin/bash
set -e

protoc -I=. --cpp_out=. ./SCMsg.proto
echo "Build scproto success!"
mv SCMsg.pb.h include/

rm -rf build && mkdir build && pushd build > /dev/null
cmake -DBUILD_SHARED_LIBS=OFF ..
make -B
popd > /dev/null
