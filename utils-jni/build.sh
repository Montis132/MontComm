#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

JAVA_FILE="$SCRIPT_DIR/src/main/java/CommMngr/UtilJNI.java"
C_FILE="$SCRIPT_DIR/src/main/native/util_jni.c"
OUTPUT_LIB="libutiljni.so"

if [ -z "$JAVA_HOME" ]; then
    echo "Error: JAVA_HOME is not set"
    exit 1
fi

UTILS_DIR="$PROJECT_ROOT/utils"
THIRD_PARTY_DIR="$PROJECT_ROOT/third_party"
GMSSL_DIR="$THIRD_PARTY_DIR/GmSSL"
LIBEVENT_DIR="$THIRD_PARTY_DIR/libevent"

UTILS_BUILD_DIR="$UTILS_DIR/build"
GMSSL_BUILD_DIR="$GMSSL_DIR/build"

rm -rf "$BUILD_DIR" && mkdir -p "$BUILD_DIR"

echo "[1/3] Compiling Java and generating JNI header..."
"$JAVA_HOME/bin/javac" -h "$BUILD_DIR" -d "$BUILD_DIR" "$JAVA_FILE"

echo "[2/3] Compiling C source into shared library..."
gcc -fPIC -shared -o "$BUILD_DIR/$OUTPUT_LIB" "$C_FILE" \
    -I"$JAVA_HOME/include" \
    -I"$JAVA_HOME/include/linux" \
    -I"$UTILS_DIR/include" \
    -I"$GMSSL_DIR/include" \
    -I"$LIBEVENT_DIR/include" \
    -L"$UTILS_BUILD_DIR/lib" \
    -L"$GMSSL_BUILD_DIR/bin" \
    -lUtils -lgmssl -lpthread

echo "[3/3] Build complete."
echo "  Library: $BUILD_DIR/$OUTPUT_LIB"
echo "  Classes: $BUILD_DIR"
