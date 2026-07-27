OUTPUT_ARCH(aarch64)
ENTRY(_start)

PHDRS
{
	code   PT_LOAD FLAGS(5) /* Read | Execute */;
	rodata PT_LOAD FLAGS(4) /* Read */;
	data   PT_LOAD FLAGS(6) /* Read | Write */;
	dyn    PT_DYNAMIC;
}

SECTIONS
{
	/* =========== CODE section =========== */
	PROVIDE(__start__ = 0x0);
	. = __start__;
	__code_start = . ;

	.crt0 :
	{
		KEEP (*(.crt0))
		. = ALIGN(8);
	} :code

	.init :
	{
		KEEP( *(.init) )
		. = ALIGN(8);
	} :code

	.plt :
	{
		*(.plt)
		*(.iplt)
		. = ALIGN(8);
	} :code

	.text :
	{
		*(.text.unlikely .text.*_unlikely .text.unlikely.*)
		*(.text.exit .text.exit.*)
		*(.text.startup .text.startup.*)
		*(.text.hot .text.hot.*)
		*(.text .stub .text.* .gnu.linkonce.t.*)
		. = ALIGN(8);
	} :code

	.fini :
	{
		KEEP( *(.fini) )
		. = ALIGN(8);
	} :code

	/* =========== RODATA section =========== */
	. = ALIGN(0x1000);
	__rodata_start = . ;

	.nx-module-name : { KEEP (*(.nx-module-name)) } :rodata

	.rodata :
	{
		*(.rodata .rodata.* .gnu.linkonce.r.*)
		. = ALIGN(8);
	} :rodata

	.eh_frame_hdr     : { __eh_frame_hdr_start = .; *(.eh_frame_hdr) *(.eh_frame_entry .eh_frame_entry.*) __eh_frame_hdr_end = .; } :rodata
	.eh_frame         : ONLY_IF_RO { KEEP (*(.eh_frame)) *(.eh_frame.*) } :rodata

	.dynamic           : { PROVIDE(__dynamic_start = .); *(.dynamic) } :rodata :dyn
	.dynsym            : { *(.dynsym) } :rodata
	.dynstr            : { *(.dynstr) } :rodata
	.rela.dyn          : { *(.rela.*) } :rodata
	.hash              : { *(.hash) } :rodata
	.gnu.hash          : { *(.gnu.hash) } :rodata
	.note.gnu.build-id : { *(.note.gnu.build-id) } :rodata

	/* =========== DATA section =========== */
	. = ALIGN(0x1000);
	__data_start = . ;

	.eh_frame : ONLY_IF_RW { KEEP (*(.eh_frame)) *(.eh_frame.*) } :data

	.preinit_array ALIGN(8) :
	{
		PROVIDE (__preinit_array_start = .);
		KEEP (*(.preinit_array))
		PROVIDE (__preinit_array_end = .);
	} :data

	.init_array ALIGN(8) :
	{
		PROVIDE (__init_array_start = .);
		KEEP( *(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)) )
		KEEP( *(.init_array .ctors) )
		PROVIDE (__init_array_end = .);
	} :data

	.fini_array ALIGN(8) :
	{
		PROVIDE (__fini_array_start = .);
		KEEP( *(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)) )
		KEEP( *(.fini_array .dtors) )
		PROVIDE (__fini_array_end = .);
	} :data

	__got_start__ = .;
	.got            : { *(.got) *(.igot) } :data
	.got.plt        : { *(.got.plt)  *(.igot.plt) } :data
	__got_end__ = .;

	.data ALIGN(8) :
	{
		*(.data .data.* .gnu.linkonce.d.*)
		SORT(CONSTRUCTORS)
	} :data

	__bss_start__ = .;
	.bss ALIGN(8) :
	{
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(8);
	} : data
	__bss_end__ = .;

	__end__ = ABSOLUTE(.) ;

	. = ALIGN(0x1000);
	__argdata__ = ABSOLUTE(.) ;

	/DISCARD/ : { *(.group .comment .note) }
	.stab          0 : { *(.stab) }
	.stabstr       0 : { *(.stabstr) }
	.debug          0 : { *(.debug) }
	.line           0 : { *(.line) }
	.debug_srcinfo  0 : { *(.debug_srcinfo) }
	.debug_sfnames  0 : { *(.debug_sfnames) }
	.debug_aranges  0 : { *(.debug_aranges) }
	.debug_pubnames 0 : { *(.debug_pubnames) }
	.debug_info     0 : { *(.debug_info) }
	.debug_abbrev   0 : { *(.debug_abbrev) }
	.debug_line     0 : { *(.debug_line) }
	.debug_frame    0 : { *(.debug_frame) }
	.debug_str      0 : { *(.debug_str) }
	.debug_loc      0 : { *(.debug_loc) }
	.debug_macinfo  0 : { *(.debug_macinfo) }
	.debug_gnu_pubnames 0 : { *(.debug_gnu_pubnames) }
	.debug_gnu_pubtypes 0 : { *(.debug_gnu_pubtypes) }
	.debug_ranges   0 : { *(.debug_ranges) }
}
