#!/bin/sh
echo 'First exec'
printf '1\n2\n3\n' | ./alines-menu -t 'Menu #1'
echo 'Second exec'
printf '1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n' | ./alines-menu -t 'Menu #2' -c -m
echo 'Done'
