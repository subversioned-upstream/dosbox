/*
 *  Copyright (C) 2002-2005  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dosbox.h"
#include "mem.h"
#include "dos_inc.h"
#include "callback.h"

#define UMB_START_SEG 0x9fff

static Bit16u memAllocStrategy = 0x00;

static void DOS_CompressMemory(void) {
	Bit16u mcb_segment=dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	DOS_MCB mcb_next(0);

	while (mcb.GetType()!=0x5a) {
		mcb_next.SetPt((Bit16u)(mcb_segment+mcb.GetSize()+1));
		if ((mcb.GetPSPSeg()==0) && (mcb_next.GetPSPSeg()==0)) {
			mcb.SetSize(mcb.GetSize()+mcb_next.GetSize()+1);
			mcb.SetType(mcb_next.GetType());
		} else {
			mcb_segment+=mcb.GetSize()+1;
			mcb.SetPt(mcb_segment);
		}
	}
}

void DOS_FreeProcessMemory(Bit16u pspseg) {
	Bit16u mcb_segment=dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	while (true) {
		if (mcb.GetPSPSeg()==pspseg) {
			mcb.SetPSPSeg(MCB_FREE);
		}
		if (mcb.GetType()==0x5a) break;
		mcb_segment+=mcb.GetSize()+1;
		mcb.SetPt(mcb_segment);
	}

	Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
	if (umb_start==UMB_START_SEG) {
		DOS_MCB umb_mcb(umb_start);
		while (true) {
			if (umb_mcb.GetPSPSeg()==pspseg) {
				umb_mcb.SetPSPSeg(MCB_FREE);
			}
			if (umb_mcb.GetType()!=0x4d) break;
			umb_start+=umb_mcb.GetSize()+1;
			umb_mcb.SetPt(umb_start);
		}
	} else if (umb_start!=0xffff) LOG(LOG_DOSMISC,LOG_ERROR)("Corrupt UMB chain: %x",umb_start);

	DOS_CompressMemory();
};

Bit16u DOS_GetMemAllocStrategy()
{
	return memAllocStrategy;
};

void DOS_SetMemAllocStrategy(Bit16u strat)
{
	memAllocStrategy = strat;
};

bool DOS_AllocateMemory(Bit16u * segment,Bit16u * blocks) {
	DOS_CompressMemory();
	Bit16u bigsize=0;
	Bit16u mem_strat=memAllocStrategy;
	Bit16u mcb_segment=dos.firstMCB;

	Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
	if (umb_start==UMB_START_SEG) {
		if (mem_strat&0xc0) mcb_segment=umb_start;
	} else if (umb_start!=0xffff) LOG(LOG_DOSMISC,LOG_ERROR)("Corrupt UMB chain: %x",umb_start);

	DOS_MCB mcb(0);
	DOS_MCB mcb_next(0);
	DOS_MCB psp_mcb(dos.psp()-1);
	char psp_name[9];
	psp_mcb.GetFileName(psp_name);
	bool stop=false;
	while(!stop) {
		mcb.SetPt(mcb_segment);
		if (mcb.GetPSPSeg()==0) {
			/* Check for enough free memory in current block */
			Bit16u block_size=mcb.GetSize();			
			if (block_size<(*blocks)) {
				if (bigsize<block_size) {
					bigsize=block_size;
				}
			} else if (block_size==*blocks) {
				mcb.SetPSPSeg(dos.psp());
				*segment=mcb_segment+1;
				return true;
			} else {
				// TODO: Strategy "1": Best matching block
				/* If so allocate it */
				if ((mem_strat & 0x03)==0) {	
					mcb_next.SetPt((Bit16u)(mcb_segment+*blocks+1));
					mcb_next.SetPSPSeg(MCB_FREE);
					mcb_next.SetType(mcb.GetType());
					mcb_next.SetSize(block_size-*blocks-1);
					mcb.SetSize(*blocks);
					mcb.SetType(0x4d);		
					mcb.SetPSPSeg(dos.psp());
					mcb.SetFileName(psp_name);
					//TODO Filename
					*segment=mcb_segment+1;
					return true;
				} else {
					// * Last Block *
					// New created block
					*segment = mcb_segment+1+block_size - *blocks;
					mcb_next.SetPt((Bit16u)(*segment-1));
					mcb_next.SetSize(*blocks);
					mcb_next.SetType(mcb.GetType());
					mcb_next.SetPSPSeg(dos.psp());
					mcb_next.SetFileName(psp_name);
					// Old Block
					mcb.SetSize(block_size-*blocks-1);
					mcb.SetPSPSeg(MCB_FREE);
					mcb.SetType(0x4D);
					return true;
				};
			}
		}
		/* Onward to the next MCB if there is one */
		if (mcb.GetType()==0x5a) {
			if ((mem_strat&0x80) && (umb_start==UMB_START_SEG)) {
				mcb_segment=dos.firstMCB;
				mem_strat&=(~0xc0);
			} else {
				*blocks=bigsize;
				DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
				return false;
			}
		} else mcb_segment+=mcb.GetSize()+1;
	}
	return false;
}


bool DOS_ResizeMemory(Bit16u segment,Bit16u * blocks) {
	if (segment < DOS_MEM_START+1) {
		LOG(LOG_DOSMISC,LOG_ERROR)("Program resizes %X, take care",segment);
	}
      
	DOS_MCB mcb(segment-1);
	if ((mcb.GetType()!=0x4d) && (mcb.GetType()!=0x5a)) {
		DOS_SetError(DOSERR_MCB_DESTROYED);
		return false;
	}

	DOS_CompressMemory();
	Bit16u total=mcb.GetSize();
	DOS_MCB	mcb_next(segment+total);
	if (*blocks<=total) {
		if (GCC_UNLIKELY(*blocks==total)) {
			/* Nothing to do */
			return true;
		}
		/* Shrinking MCB */
		DOS_MCB	mcb_new_next(segment+(*blocks));
		mcb.SetSize(*blocks);
		mcb_new_next.SetType(mcb.GetType());
		if (mcb.GetType()==0x5a) {
			/* Further blocks follow */
			mcb.SetType(0x4d);
		}

		mcb_new_next.SetSize(total-*blocks-1);
		mcb_new_next.SetPSPSeg(MCB_FREE);
		return true;
	}
	if (mcb.GetType()!=0x5a) {
		if (mcb_next.GetPSPSeg()==MCB_FREE) {
			total+=mcb_next.GetSize()+1;
		}
	}
	if (*blocks<total) {
		if (mcb.GetType()!=0x5a) {
			mcb.SetType(mcb_next.GetType());
		}
		mcb.SetSize(*blocks);
		mcb_next.SetPt((Bit16u)(segment+*blocks));
		mcb_next.SetSize(total-*blocks-1);
		mcb_next.SetType(mcb.GetType());
		mcb_next.SetPSPSeg(MCB_FREE);
		mcb.SetType(0x4d);
		return true;
	}
	if (*blocks==total) {
		if (mcb.GetType()!=0x5a) {
			mcb.SetType(mcb_next.GetType());
		}
		mcb.SetSize(*blocks);
		return true;
	}
	*blocks=total;
	DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
	return false;
}


bool DOS_FreeMemory(Bit16u segment) {
//TODO Check if allowed to free this segment
	if (segment < DOS_MEM_START+1) {
		LOG(LOG_DOSMISC,LOG_ERROR)("Program tried to free %X ---ERROR",segment);
		DOS_SetError(DOSERR_MB_ADDRESS_INVALID);
		return false;
	}
      
	DOS_MCB mcb(segment-1);
	if ((mcb.GetType()!=0x4d) && (mcb.GetType()!=0x5a)) {
		DOS_SetError(DOSERR_MB_ADDRESS_INVALID);
		return false;
	}
	mcb.SetPSPSeg(MCB_FREE);
	DOS_CompressMemory();
	return true;
}


void DOS_BuildUMBChain(const char* use_umbs,bool ems_active) {
	if (strcmp(use_umbs,"false")!=0) {
		Bit16u first_umb_seg=0xca00;
		Bit16u first_umb_size=0x600;

		if (strcmp(use_umbs,"max")==0) {
			first_umb_seg-=0x100;
			first_umb_size+=0x100;
		}

		dos_infoblock.SetStartOfUMBChain(UMB_START_SEG);
		dos_infoblock.SetUMBChainState(0);		// UMBs not linked yet

		DOS_MCB umb_mcb(first_umb_seg);
		umb_mcb.SetPSPSeg(0);		// currently free
		umb_mcb.SetSize(first_umb_size-1);
		umb_mcb.SetType(0x5a);

		/* Scan MCB-chain for last block */
		Bit16u mcb_segment=dos.firstMCB;
		DOS_MCB mcb(mcb_segment);
		while (mcb.GetType()!=0x5a) {
			mcb_segment+=mcb.GetSize()+1;
			mcb.SetPt(mcb_segment);
		}

		/* A system MCB has to cover the space between the
		   regular MCB-chain and the UMBs */
		Bit16u cover_mcb=(Bit16u)(mcb_segment+mcb.GetSize()+1);
		mcb.SetPt(cover_mcb);
		mcb.SetType(0x4d);
		mcb.SetPSPSeg(0x0008);
		mcb.SetSize(first_umb_seg-cover_mcb-1);
		mcb.SetFileName("SC      ");

		if (!ems_active && (strcmp(use_umbs,"max")==0)) {
			Bit16u ems_umb_seg=0xe000;
			Bit16u ems_umb_size=0x1000;

			/* Continue UMB-chain */
			umb_mcb.SetSize(first_umb_size-2);
			umb_mcb.SetType(0x4d);

			DOS_MCB umb2_mcb(ems_umb_seg);
			umb2_mcb.SetPSPSeg(0);		// currently free
			umb2_mcb.SetSize(ems_umb_size-1);
			umb2_mcb.SetType(0x5a);

			/* A system MCB has to take out the space between the previous and this UMB */
			cover_mcb=(Bit16u)(first_umb_seg+umb_mcb.GetSize()+1);
			mcb.SetPt(cover_mcb);
			mcb.SetType(0x4d);
			mcb.SetPSPSeg(0x0008);
			mcb.SetSize(ems_umb_seg-cover_mcb-1);
			mcb.SetFileName("SC      ");
		}
	} else {
		dos_infoblock.SetStartOfUMBChain(0xffff);
		dos_infoblock.SetUMBChainState(0);
	}
}

bool DOS_LinkUMBsToMemChain(Bit16u linkstate) {
	/* Get start of UMB-chain */
	Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
	if (umb_start!=UMB_START_SEG) {
		if (umb_start!=0xffff) LOG(LOG_DOSMISC,LOG_ERROR)("Corrupt UMB chain: %x",umb_start);
		return true;
	}

	if ((linkstate&1)==(dos_infoblock.GetUMBChainState()&1)) return true;
	
	/* Scan MCB-chain for last block before UMB-chain */
	Bit16u mcb_segment=dos.firstMCB;
	Bit16u prev_mcb_segment;
	DOS_MCB mcb(mcb_segment);
	while ((mcb_segment!=umb_start) && (mcb.GetType()!=0x5a)) {
		prev_mcb_segment=mcb_segment;
		mcb_segment+=mcb.GetSize()+1;
		mcb.SetPt(mcb_segment);
	}
	DOS_MCB prev_mcb(prev_mcb_segment);

	switch (linkstate) {
		case 0x0000:	// unlink
			if ((prev_mcb.GetType()==0x4d) && (mcb_segment==umb_start)) {
				prev_mcb.SetType(0x5a);
			}
			dos_infoblock.SetUMBChainState(0);
			break;
		case 0x0001:	// link
			if (mcb.GetType()==0x5a) {
				mcb.SetType(0x4d);
				dos_infoblock.SetUMBChainState(1);
			}
			break;
		default:
			LOG_MSG("Invalid link state %x when reconfiguring MCB chain",linkstate);
			return false;
	}

	return true;
}


static Bitu DOS_default_handler(void) {
	LOG(LOG_CPU,LOG_ERROR)("DOS rerouted Interrupt Called %X",lastint);
	return CBRET_NONE;
};

static	CALLBACK_HandlerObject callbackhandler;
void DOS_SetupMemory(void) {
	// Create a dummy device MCB with PSPSeg=0x0008
	DOS_MCB mcb_devicedummy((Bit16u)DOS_MEM_START);
	mcb_devicedummy.SetPSPSeg(0x0008);	// Devices
	mcb_devicedummy.SetSize(1);
	mcb_devicedummy.SetType(0x4d);		// More blocks will follow
//	mcb_devicedummy.SetFileName("SD      ");

	/* Let dos claim a few bios interrupts. Makes DOSBox more compatible with 
	 * buggy games, which compare against the interrupt table. (probably a 
	 * broken linked list implementation) */
	// BioMenace (segment of int2<0x8000)
	mem_writeb((DOS_MEM_START+1)<<4,0xcf);// iret
	RealSetVec(0x02,(DOS_MEM_START+1)<<16);
	callbackhandler.Allocate(&DOS_default_handler,"DOS default int");
	//Shadow president wants int 4 to point to this.
	real_writeb(0x70,0,(Bit8u)0xFE);   //GRP 4
	real_writeb(0x70,1,(Bit8u)0x38);   //Extra Callback instruction
	real_writew(0x70,2,callbackhandler.Get_callback());  //The immediate word
	real_writeb(0x70,4,(Bit8u)0xCF);   //An IRET Instruction
	real_writed(0,0x04*4,0x700000);
	//claim some more ints to this location
	real_writed(0,0x01*4,0x700000);
	real_writed(0,0x03*4,0x700000);
//	real_writed(0,0x0f*4,0x700000); //Always a tricky one (soundblaster irq)

	DOS_MCB mcb((Bit16u)DOS_MEM_START+2);
	mcb.SetPSPSeg(MCB_FREE);						//Free
	if (machine==MCH_TANDY) {
		mcb.SetSize(0x97FE - DOS_MEM_START - 2);
	} else mcb.SetSize(0x9FFE - DOS_MEM_START - 2);
	mcb.SetType(0x5a);								//Last Block

	dos.firstMCB=DOS_MEM_START;
	dos_infoblock.SetFirstMCB(DOS_MEM_START);
}
