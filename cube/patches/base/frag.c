/*
*
*   Swiss - The Gamecube IPL replacement
*
*	frag.c
*		- Wrap normal read requests around a fragmented file table
*/

#include "../../reservedarea.h"
#include "common.h"

// Returns the amount read from the given offset until a frag is hit
u32 read_frag(void *dst, u32 len, u32 offset) {

	vu32 *fragList = (vu32*)VAR_FRAG_LIST;
	int isDisc2 = (*(vu32*)(VAR_DISC_1_LBA)) != (*(vu32*)VAR_CUR_DISC_LBA);
	int maxFrags = (*(vu32*)(VAR_DISC_1_LBA) != *(vu32*)(VAR_DISC_2_LBA)) ? ((VAR_FRAG_SIZE/12)/2) : (VAR_FRAG_SIZE/12), i = 0, j = 0;
	int fragTableStart = isDisc2 ? (maxFrags*3) : 0;
	int amountToRead = len;
	int adjustedOffset = offset;
	
	// Locate this offset in the fat table and read as much as we can from a single fragment
	for(i = 0; i < maxFrags; i++) {
		u32 fragTableIdx = fragTableStart +(i*3);
		u32 fragOffset = fragList[fragTableIdx+0];
		u32 fragSize = fragList[fragTableIdx+1] & 0x7FFFFFFF;
		u32 fragSector = fragList[fragTableIdx+2];
		u32 fragOffsetEnd = fragOffset + fragSize;
#ifdef DEBUG_VERBOSE
		usb_sendbuffer_safe("READ: dst: ",11);
		print_int_hex(dst);
		usb_sendbuffer_safe(" len: ",6);
		print_int_hex(len);
		usb_sendbuffer_safe(" ofs: ",6);
		print_int_hex(offset);
#endif
		// Find where our read starts and read as much as we can in this frag before returning
		if(offset >= fragOffset && offset < fragOffsetEnd) {
			// Does our read get cut off early?
			if(offset + len > fragOffsetEnd) {
				amountToRead = fragOffsetEnd - offset;
			}
			if(fragOffset != 0) {
				adjustedOffset = offset - fragOffset;
			}
			amountToRead = do_read(dst, amountToRead, adjustedOffset, fragSector);
#ifdef DEBUG_VERBOSE
			u32 sz = amountToRead;
			u8* ptr = (u8*)dst;
			u32 hash = 5381;
			s32 c;
			while (c = *ptr++)
            hash = ((hash << 5) + hash) + c;

			usb_sendbuffer_safe(" checksum: ",11);
			print_int_hex(hash);
			usb_sendbuffer_safe("\r\n",2);
#endif
			return amountToRead;
		}
	}
	return 0;
}

int is_frag_read(unsigned int offset, unsigned int len) {
	vu32 *fragList = (vu32*)VAR_FRAG_LIST;
	int maxFrags = (VAR_FRAG_SIZE/12), i = 0, j = 0;
	
	// If we locate that this read lies in our frag area, return true
	for(i = 0; i < maxFrags; i++) {
		int fragOffset = fragList[(i*3)+0];
		int fragSize = fragList[(i*3)+1];
		int fragSector = fragList[(i*3)+2];
		int fragOffsetEnd = fragOffset + fragSize;
		
		if(offset >= fragOffset && offset < fragOffsetEnd) {
			// Does our read get cut off early?
			if(offset + len > fragOffsetEnd) {
				return 2; // TODO Disable DVD Interrupts, perform a read for the overhang, clear interrupts.
			}
			return 1;
		}
	}
	return 0;
}

void device_frag_read(void *dst, u32 len, u32 offset)
{	
	while(len != 0) {
		u32 amountRead = read_frag(dst, len, offset);
		len-=amountRead;
		dst+=amountRead;
		offset+=amountRead;
	}
	end_read();
}

unsigned long tb_diff_usec(tb_t* end, tb_t* start)
{
	unsigned long upper, lower;
	upper = end->u - start->u;
	if (start->l > end->l)
		upper--;
	lower = end->l - start->l;
	return ((upper * ((unsigned long)0x80000000 / (TB_CLOCK / 2000000))) + (lower / (TB_CLOCK / 1000000)));
}

void calculate_speed(void* dst, u32 len, u32 *speed)
{
	tb_t start, end;
	mftb(&start);
	device_frag_read(dst, len, 0);
	mftb(&end);
	*speed = tb_diff_usec(&end, &start);
}
