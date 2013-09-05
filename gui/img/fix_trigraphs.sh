#!/bin/sh

for file in *.c; do
	sed -i 's/??/\\77?/g' $file
done
