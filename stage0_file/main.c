#include <lv2/lv2.h>
#include <lv2/memory.h>
#include <lv2/io.h>
#include <lv2/libc.h>
#include <lv2/thread.h>
#include <lv2/patch.h>
#include <lv1/lv1.h>

//#define STAGE2_FILE_STAT	0x8e000050
#define STAGE2_FILE_NREAD	0x8e000008
#define STAGE2_LOCATION		0x8a110000

void main(void)
{
	void *stage2 = NULL;
	f_desc_t f;
	int (* func)(void);	

//	CellFsStat *stat=(CellFsStat *)STAGE2_FILE_STAT;

	for (int i = 0; i < 128; i++)
	{
		uint64_t pte0 = *(uint64_t *)(MKA(0xf000000 | (i<<7)));
		uint64_t pte1 = *(uint64_t *)(MKA(0xf000000 | ((i<<7)+8)));
		
		lv1_write_htab_entry(0, i << 3, pte0, (pte1 & 0xff0000) | 0x190);
	}

	uint64_t stackframe=0x4D59535441434B46ULL;
	#if defined (FIRMWARE_4_82) || defined (FIRMWARE_4_84)	
	*(uint64_t *)0x8000000000670000ULL=stackframe; //dumps to identify in payload
	#elif defined (FIRMWARE_4_82DEX) || defined (FIRMWARE_4_84DEX)
	*(uint64_t *)0x80000000006B0000ULL=stackframe;
	#endif
	
	uint64_t size;
//	size=stat->st_size;
	size=*(uint64_t *)STAGE2_FILE_NREAD;
	if(size>0x110000) // Thanks to @aldostools, this will prevent hang if binary does not have stage2
	{
		size=size-0x110000;
		uint64_t base=0x8000000000630000ULL;
		while(base<0x8000000000700000ULL)
		{
			if(*(uint64_t *)base==stackframe)
			{
				stage2=(void*)base;
				break;
			}
			base+=0x10000;
		}
		if (stage2)
		{		
			memcpy(stage2,(void *)STAGE2_LOCATION,size);
		}
	}
	
	if (stage2)
	{
		uint32_t sce_bytes=0x53434500;
		*(uint32_t *)0x8a000000=sce_bytes; //hen check
		
		f.addr = stage2;	
		f.toc = (void *)MKA(TOC);
		func = (void *)&f;	
		func();		
	}

}
