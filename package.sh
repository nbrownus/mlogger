#!/bin/bash
shopt -s extglob

make clean docs

rm -rf ./release
MLOGGER_VERSION=`sed -n 's/.*(\(.\+\?\)).*/\1/p' debian/changelog | head -n1`
mkdir -p ./release/source
cp -R ./!(release|package.sh|dist) ./release/source/
cd ./release
tar -czf "mlogger_$MLOGGER_VERSION.orig.tar.gz" -C ./source .
cd ./source
debuild -uc -us
