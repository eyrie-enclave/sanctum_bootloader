#!/bin/bash
qemu-system-riscv64 -S -s -nographic -machine spike_v1.10 -smp 1 -m 2G -bios rot.bin -kernel sm.elf
