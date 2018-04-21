OUTPUT_ARCH( "riscv" )

ENTRY( _entry )

SECTIONS
{
  . = 0x1000;

  /* the bootloader should be position-independent
     to allow a variety of devicetree string sizes */

  .text :
  {
    PROVIDE( _entry = .);
    *(.reset)
    *(*)
  }

	PROVIDE(_dtb = .);
  INCLUDE common/header.lds
}
