#!/bin/bash

DIR="test"

if [ ! -e "$DIR" ]; then
    rm -rf "$DIR"
    mkdir $DIR
fi

mv *.a "$DIR" -vv
mv *.dll "$DIR" -vv
cp *.h "$DIR" -vv
make clean
