#!/bin/sh

echo "name, Awan, Mathy, dF, F1, F2, F3, F4, F5"
for NAME in $1/*.wav
do
    echo -n "$NAME, "
    ./larynx_analyze $NAME
done
