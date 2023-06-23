#!/usr/bin/env bash

SOURCE_FILES=$(find src include -name '*.c' -o -name '*.h')
# echo "${SOURCE_FILES}"

for f in ${SOURCE_FILES} ; do
	# echo "$f"
	clang-format -i "$f"
done
