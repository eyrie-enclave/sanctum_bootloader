Sanctum Security Monitor
========================

About
---------

The Sanctum security monitor, `sm`, is a modification of the berkeley bootloader
(bbl). It configures a sanctum processor, boots an untrusted riscv-linux, and
supports enclaves.

TODO
--------

- Delegate interrupts for non-enclaves
- Detect enclave calls via EDRBMP -- set to 0 outside of enclave mode
- Embed keys
- Embed

Build Steps
---------------

We assume that the RISCV environment variable is set to the RISC-V tools
install path, and that the riscv-gnu-toolchain package is installed.
Please note that building the binaries directly inside the source
directory is not supported; you need to use a separate build directory.

    $ mkdir build
    $ cd build
    $ ../configure --prefix=$RISCV --host=riscv64-unknown-elf
    $ make
    $ make install

64-bit (RV64) version `sm` are built.
