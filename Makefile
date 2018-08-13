# Parameters
export BOOT_ROM_BASE :=     (0x1000)
export BOOT_ROM_SIZE :=     (0x1000)

export DRAM_BASE :=         (0x80000000)
export DRAM_SIZE :=         2048*1024*1024

export REGION_SIZE :=       32*1024*1024

export FROMHOST :=          0x80000000
export TOHOST :=            0x80000008

export MEM_LOADER_BASE :=   0x1000000
export MSIP_BASE :=         0x2000000

export BOOTLOADER_BASE :=   0x80002000
export BOOTLOADER_SIZE :=   131072

export NUM_CORES :=         1
export M_STACK_SIZE :=      0x1000

export SM_PAYLOAD =         ../riscv-linux/vmlinux

export XLEN =      	 				64

#ERASE_DRAM :=              TRUE

CC = riscv64-unknown-elf-gcc
LD = riscv64-unknown-elf-ld
OBJCOPY= riscv64-unknown-elf-objcopy
READELF= riscv64-unknown-elf-readelf
STRIP= riscv64-unknown-elf-strip
CFLAGS = \
	-march=rv64g -mabi=lp64 \
	-nostdlib -nostartfiles -fno-common -std=gnu11 \
	-static -fPIC \
	-g -O0 \
	-Wall\

.PHONY: all
all: rot.bin sm.elf

.PHONY: clean
clean:
	rm -f .depends
	rm -f payload.bin
	rm -f *.hex
	rm -f *.bin
	rm -f *.elf
	rm -f *.o
	rm -f sanctum.lds
	rm -f root_of_trust/root_of_trust.lds
	rm -f bootloader/bootloader.lds
	rm -f sm/sm.lds
	rm -f sm/*hex
	rm -f sm/.bin
	rm -f common/sanctum_config.h

# Sanctum's relies on symbols defined in the makefile, so we pre-process these.
%.lds: %.lds.in
	envsubst < $< > $@

%.h: %.h.in
	envsubst < $< > $@

# Identity Page Tables Binary
.PHONY: idpt
idpt: idpt.bin

%.hex: %.sh
	./$< > $@

%.bin: %.hex
	cat $< | xxd -r -p > $@

payload.bin: ${SM_PAYLOAD}
	if $(READELF) -h $< 2> /dev/null > /dev/null; then $(OBJCOPY) -O binary $< $@; else cp $< $@; fi

# Sanctum ELF
meta_headers = \
	common/sanctum_config.h \

srcs_rot= \
	root_of_trust/bootloader_hash.S \
	root_of_trust/root_of_trust.S \
	root_of_trust/root_of_trust.c \
	common/htif.c \
	common/stacks.S \
	common/sha3/sha3.c \
	common/util/memcpy.c \

lds_rot=root_of_trust/root_of_trust.lds

lds_rot_loadable=root_of_trust/root_of_trust_loadable.lds


srcs_bootloader = \
	bootloader/bootloader.c \
	common/randomart.c \
	common/htif.c \
	common/stacks.S \
	common/sha3/sha3.c \
	common/ed25519/keypair.c \
	common/ed25519/sign.c \
	common/ed25519/fe.c \
	common/ed25519/ge.c \
	common/ed25519/sc.c \
	common/util/memcpy.c \
	common/util/memset.c \

lds_bootloader=bootloader/bootloader.lds


srcs_sm = \
	sm/payload/payload.S \
	sm/trusted_bootloader/bootloader.S \
	sm/logo.c \
	sm/init.S \
	sm/init.c \
	sm/minit.c \
	sm/trap_or_interrupt.S \
	sm/enclave_trap.c \
	sm/os_trap.c \
	sm/monitor/*.c \
	sm/devices/*.c \
	sm/machine/fp_asm.S \
	sm/machine/emulation.c \
	sm/machine/fdt.c \
	sm/machine/vm.c \
	sm/machine/misaligned_ldst.c \
	common/randomart.c \
	common/htif.c \
	common/stacks.S \
	common/util/strcmp.c \
	common/util/strlen.c \
	common/util/memcpy.c \
	common/util/memset.c \
	common/util/snprintf.c \
	common/platform/riscy.c \
	common/sha3/sha3.c \
	common/aes/aes.c \
	common/ed25519/sign.c \
	common/ed25519/fe.c \
	common/ed25519/ge.c \
	common/ed25519/sc.c \

lds_sm=sm/sm.lds

SM_DEFINES := \
	-D __riscv_xlen=$(XLEN) \

srcs_sanctum = $(sort $(srcs_rot) $(srcs_bootloader) $(srcs_sm)) # sort also de-duplicates

# NOTE: $^ omits duplicates!
rot.elf: rot.o
	$(OBJCOPY) --only-section=.rot $^ $@

rot.o: $(srcs_rot) $(lds_rot) bootloader_hash.bin
	$(CC) $(CFLAGS) -I common/ -L . -T $(lds_rot) -Wl,-v -o $@ $(srcs_rot)

bootloader_hash.hex: bootloader.bin
	truncate -s $(BOOTLOADER_SIZE) $^
	python sha3_512.py $^ > $@

bootloader.elf: bootloader.o
	$(STRIP) $^ -o $@

bootloader.o: $(srcs_bootloader) $(lds_bootloader)
	$(CC) $(CFLAGS) -I common/ -L . -T $(lds_bootloader) -Wl,-v -o $@ $(srcs_bootloader)

sm.elf: $(srcs_sm) $(lds_sm) idpt bootloader.bin payload.bin
	$(CC) -I sm/ -I common/ $(CFLAGS) $(SM_DEFINES) -L . -T $(lds_sm) -Wl,-v -o $@ $(srcs_sm)

%.bin: %.elf
	$(OBJCOPY) -O binary --only-section=.$* $< $@

# Discover dependencies from sources
depends: .depends

.depends: $(srcs_sanctum) $(meta_headers)
	rm -f ./.depends
	$(CC) $(CFLAGS) -I common/ -MM $^ -MF  ./.depends;

include .depends

#-------------------------------------------------------------------------
# Makefile debugging
#-------------------------------------------------------------------------
# This handy rule will display the contents of any make variable by
# using the target debug-<varname>. So for example, make debug-junk will
# display the contents of the junk variable.

debug-% :
	@echo $* = $($*)
