#!/bin/sh
SH_DIR=$(dirname $0)
REPO_DIR=$1
TARGET_PRODUCT=$2
TARGET_SO_32BIT=$REPO_DIR/out/target/product/$TARGET_PRODUCT/vendor/lib/liblowlightshot.so
TARGET_SO_64BIT=$REPO_DIR/out/target/product/$TARGET_PRODUCT/vendor/lib64/liblowlightshot.so
echo cp $TARGET_SO_32BIT $SH_DIR/../lib32/
echo cp $TARGET_SO_64BIT $SH_DIR/../lib64/
cp $TARGET_SO_32BIT $SH_DIR/../lib32/
cp $TARGET_SO_64BIT $SH_DIR/../lib64/
