#!/bin/bash

# Each PTE is 64 bits
#

# TODO: Consider word order
# TODO: Consider endianness

for i in `seq 0 511`
do
  let pte=255+$i*268435456
  printf '%016x\n' $pte
done

#for (i=0; i<512; i++) {
#  (i << 28) + 0xFF
#}

