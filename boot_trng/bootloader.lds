OUTPUT_ARCH( "riscv" )

ENTRY( _entry )

SECTIONS
{
  . = 0x2000; /* boot loader lives in boot ROM after the device tree */

  .text :
  {
    PROVIDE( _entry = .);
    *(.reset)
    *(*)
  }

	PROVIDE( _dtb = . );
	
  INCLUDE common/header.lds
}
