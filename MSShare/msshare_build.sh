#!/bin/bash
set -e

protoc -I=. --cpp_out=. ./MSMsg.proto

echo "Build msproto success!"

mv MSMsg.pb.h include/
