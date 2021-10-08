#!/bin/sh
echo 'First exec'
printf '1\n2\n3\n' | ./alines-menu -t 'Menu #1'
echo 'Second exec'
printf '4\n5\n6\n7\n8\n9\n' | ./alines-menu -t 'Menu #2' -c -m
echo 'Done'
