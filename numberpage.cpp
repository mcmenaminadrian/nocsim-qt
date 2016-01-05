//numberpage

/*	Page structure
	1024 bytes in total
	128 byte bit mark
	2 byte offset to start of 8 byte units
	2 byte offset to start of 16 byte units
	2 byte offset to start of 32 byte units
	2 byte offset to start of 64 byte units
	2 byte offset to start of 128 byte units
	(Total used so far: 136 bytes, 888 bytes remaining)

	initial settings:
	33 8 byte spaces (264 bytes, 626 remaining)
	23 16 byte spaces (368 bytes, 256 remaining)
	2 32 byte spaces (64 bytes, 192 remaining)
	1 64 byte space (64 bytes, 128 remaining)
	1 128 byte space (128 bytes, 0 remaining)
*/

/*	How it works:
	On creation first 140 bits marked used
	Offsets set

	Size sought - allocated
	Not available - new page */


