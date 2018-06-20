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
- Add SM data structures, OS API, Ecnlave API
- Document memory laoyout

Build Steps
---------------

We assume that the RISCV environment variable is set to the RISC-V tools (TOOD, there are a couple of other assumptions), and that the riscv-gnu-toolchain package is installed.

    $ make

64-bit (RV64) ELF version of `sm` and `rot` are built, as well as a binary blob for the bootloader (`rot.bin`).
