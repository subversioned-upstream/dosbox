/*
 *  Copyright (C) 2002  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef DOS_H_
#define DOS_H_

#include <dos_system.h>
#include <mem.h>

#pragma pack (1)

struct CommandTail{
  Bit8u count;				/* number of bytes returned */
  char buffer[127];			 /* the buffer itself */
} GCC_ATTRIBUTE(packed);

struct sPSP {
	Bit8u	exit[2];			/* CP/M-like exit poimt */
	Bit16u	next_seg;			/* Segment of first byte beyond memory allocated or program */
	Bit8u	fill_1;				/* single char fill */
	Bit8u	far_call;			/* far call opcode */
	RealPt	cpm_entry;			/* CPM Service Request address*/
	RealPt	int_22;				/* Terminate Address */
	RealPt	int_23;				/* Break Address */
	RealPt	int_24;				/* Critical Error Address */
	Bit16u	psp_parent;			/* Parent PSP Segment */
	Bit8u	files[20];			/* File Table - 0xff is unused */
	Bit16u	environment;		/* Segment of evironment table */
	RealPt	stack;				/* SS:SP Save point for int 0x21 calls */
	Bit16u	max_files;			/* Maximum open files */
	RealPt	file_table;			/* Pointer to File Table PSP:0x18 */
	RealPt	prev_psp;			/* Pointer to previous PSP */
	RealPt	dta;				/* Pointer to current Process DTA */
	Bit8u	fill_2[16];			/* Lot's of unused stuff i can't care aboue */
	Bit8u	service[3];			/* INT 0x21 Service call int 0x21;retf; */
	Bit8u	fill_3[9];			/* This has some blocks with FCB info */
	Bit8u	fcb1[16];			/* first FCB */
	Bit8u	fcb2[16];			/* second FCB */
	Bit8u	fill_4[4];			/* unused */
	CommandTail cmdtail;		

} GCC_ATTRIBUTE(packed);

struct ParamBlock {
	union {
		struct {
			Bit16u loadseg;
			Bit16u relocation;
		} overlay;
		struct {
			Bit16u envseg;
			RealPt cmdtail;
			RealPt fcb1;
			RealPt fcb2;
			RealPt initsssp;
			RealPt initcsip;
		} exec;
	};
} GCC_ATTRIBUTE(packed);


struct MCB {
	Bit8u type;
	Bit16u psp_segment;
	Bit16u size;	
	Bit8u unused[3];
	Bit8u filename[8];
} GCC_ATTRIBUTE(packed);

#pragma pack ()

struct DOS_Date {
	Bit16u year;
	Bit8u month;
	Bit8u day;
};


struct DOS_Version {
	Bit8u major,minor,revision;
};

struct DOS_Block {
	DOS_Date date;
	DOS_Version version;
	Bit16u firstMCB;
	Bit16u errorcode;
	Bit16u psp;
	Bit16u env;
	RealPt cpmentry;
	RealPt dta;
	Bit8u return_code,return_mode;
	
	bool verify;
	bool breakcheck;
	struct  {
		RealPt indosflag;
	} tables;
};

class MemStruct {
public:
	Bit8u GetIt(Bit8u,PhysPt addr) {
		return mem_readb(pt+addr);
	};
	Bit16u GetIt(Bit16u,PhysPt addr) {
		return mem_readw(pt+addr);
	};
	Bit32u GetIt(Bit32u,PhysPt addr) {
		return mem_readd(pt+addr);
	};
	void SaveIt(Bit8u,PhysPt addr,Bit8u val) {
		mem_writeb(pt+addr,val);
	};
	void SaveIt(Bit16u,PhysPt addr,Bit16u val) {
		mem_writew(pt+addr,val);
	};
	void SaveIt(Bit32u,PhysPt addr,Bit32u val) {
		mem_writed(pt+addr,val);
	};
	

protected:
PhysPt pt;

};

#define sGet(s,m) GetIt(((s *)Phys2Host(pt))->m,(PhysPt)&(((s *)0)->m))
#define sSave(s,m,val) SaveIt(((s *)Phys2Host(pt))->m,(PhysPt)&(((s *)0)->m),val)

class DOS_PSP :public MemStruct {
public:
	DOS_PSP						()						{ seg=0; pt=0;		};
	DOS_PSP						(Bit16u segment)		{ NewPt(segment);	};
	void	NewPt				(Bit16u segment);
	void	MakeNew				(Bit16u memSize);
	
	void	CopyFileTable		(DOS_PSP* srcpsp);
	Bit16u	FindFreeFileEntry	(void);
	void	CloseFiles			(void);

	void	SaveVectors			(void);
	void	RestoreVectors		(void);
	void	SetSize				(Bit16u size)			{ sSave(sPSP,next_seg,size);		};
	Bit16u	GetSize				()						{ return sGet(sPSP,next_seg);		};
	void	SetDTA				(RealPt ptdta)			{ sSave(sPSP,dta,ptdta);			};
	RealPt	GetDTA				(void)					{ return sGet(sPSP,dta);			};
	void	SetEnvironment		(Bit16u envseg)			{ sSave(sPSP,environment,envseg);	};
	Bit16u	GetEnvironment		(void)					{ return sGet(sPSP,environment);	};
	Bit16u	GetSegment			(void)					{ return seg;						};
	void	SetFileHandle		(Bit16u index, Bit8u handle);
	Bit8u	GetFileHandle		(Bit16u index);
	void	SetParent			(Bit16u parent)			{ sSave(sPSP,psp_parent,parent);	};
	Bit16u	GetParent			(void)					{ return sGet(sPSP,psp_parent);		};
	void	SetStack			(RealPt stackpt)		{ sSave(sPSP,stack,stackpt);		};
	RealPt	GetStack			(void)					{ return sGet(sPSP,stack);			};
	void	SetInt22			(RealPt int22pt)		{ sSave(sPSP,int_22,int22pt);		};
	RealPt	GetInt22			(void)					{ return sGet(sPSP,int_22);			};
	void	SetFCB1				(RealPt src);
	void	SetFCB2				(RealPt src);
	void	SetCommandTail		(RealPt src);	

private:
	Bit16u	seg;
	sPSP*	psp;

public:
	static	Bit16u rootpsp;
};

enum { MCB_FREE=0x0000,MCB_DOS=0x0008 };
enum { RETURN_EXIT=0,RETURN_CTRLC=1,RETURN_ABORT=2,RETURN_TSR=3};

#define DOS_FILES 50
#define DOS_DRIVES 26

/* internal Dos Tables */
extern DOS_Block dos;
extern DOS_File * Files[DOS_FILES];
extern DOS_Drive * Drives[DOS_DRIVES];


void DOS_SetError(Bit16u code);

/* File Handling Routines */

enum { STDIN=0,STDOUT=1,STDERR=2,STDAUX=3,STDNUL=4,STDPRN=5};
enum { HAND_NONE=0,HAND_FILE,HAND_DEVICE};



/* Routines for File Class */
void DOS_SetupFiles (void);

bool DOS_ReadFile(Bit16u handle,Bit8u * data,Bit16u * amount);
bool DOS_WriteFile(Bit16u handle,Bit8u * data,Bit16u * amount);
bool DOS_SeekFile(Bit16u handle,Bit32u * pos,Bit32u type);
bool DOS_CloseFile(Bit16u handle);
bool DOS_DuplicateEntry(Bit16u entry,Bit16u * newentry);
bool DOS_ForceDuplicateEntry(Bit16u entry,Bit16u newentry);
/* Routines for Drive Class */
bool DOS_OpenFile(char * name,Bit8u flags,Bit16u * entry);
bool DOS_CreateFile(char * name,Bit16u attribute,Bit16u * entry);
bool DOS_UnlinkFile(char * name);
bool DOS_FindFirst(char *search,Bit16u attr);
bool DOS_FindNext(void);
bool DOS_Canonicalize(char * name,char * big);
bool DOS_CreateTempFile(char * name,Bit16u * entry);
bool DOS_FileExists(char * name);
/* Drive Handing Routines */
Bit8u DOS_GetDefaultDrive(void);
void DOS_SetDefaultDrive(Bit8u drive);
bool DOS_SetDrive(Bit8u drive);
bool DOS_GetCurrentDir(Bit8u drive,char * bugger);
bool DOS_ChangeDir(char * dir);
bool DOS_MakeDir(char * dir);
bool DOS_RemoveDir(char * dir);
bool DOS_Rename(char * oldname,char * newname);
bool DOS_GetFreeDiskSpace(Bit8u drive,Bit16u * bytes,Bit16u * sectors,Bit16u * clusters,Bit16u * free);
bool DOS_GetFileAttr(char * name,Bit16u * attr);
/* IOCTL Stuff */
bool DOS_IOCTL(Bit8u call,Bit16u entry);
bool DOS_GetSTDINStatus();
Bit8u DOS_FindDevice(char * name);
void DOS_SetupDevices(void);
/* Execute and new process creation */
bool DOS_NewPSP(Bit16u pspseg,Bit16u size);
bool DOS_Execute(char * name,ParamBlock * block,Bit8u flags);
bool DOS_Terminate(bool tsr);

/* Memory Handling Routines */
void DOS_SetupMemory(void);
bool DOS_AllocateMemory(Bit16u * segment,Bit16u * blocks);
bool DOS_ResizeMemory(Bit16u segment,Bit16u * blocks);
bool DOS_FreeMemory(Bit16u segment);
void DOS_FreeProcessMemory(Bit16u pspseg);
Bit16u DOS_GetMemory(Bit16u pages);


/* FCB stuff */
bool DOS_FCBOpen(Bit16u seg,Bit16u offset);
bool DOS_FCBClose(Bit16u seg,Bit16u offset);
bool DOS_FCBFindFirst(Bit16u seg,Bit16u offset);
bool DOS_FCBFindNext(Bit16u seg,Bit16u offset);
Bit8u DOS_FCBRead(Bit16u seg,Bit16u offset, Bit16u numBlocks);
bool DOS_FCBWrite(Bit16u seg,Bit16u offset,Bit16u numBlocks);
bool DOS_FCBCreate(Bit16u seg,Bit16u offset);
Bit8u DOS_FCBRandomRead(Bit16u seg,Bit16u offset,Bit16u numRec);
bool DOS_FCBRandomWrite(Bit16u seg,Bit16u offset,Bit16u numRec);
bool DOS_FCBGetFileSize(Bit16u seg,Bit16u offset,Bit16u numRec);
bool DOS_FCBDeleteFile(Bit16u seg,Bit16u offset);
bool DOS_FCBRenameFile(Bit16u seg, Bit16u offset);
Bit8u FCB_Parsename(Bit16u seg,Bit16u offset,Bit8u parser ,char *string, Bit8u *change);
/* Extra DOS Interrupts */
void DOS_SetupMisc(void);

/* The DOS Tables */
void DOS_SetupTables(void);
/* Internal DOS Setup Programs */
void DOS_SetupPrograms(void);


INLINE Bit16u long2para(Bit32u size) {
	if (size>0xFFFF0) return 0xffff;
	if (size&0xf) return (Bit16u)((size>>4)+1);
	else return (Bit16u)(size>>4);
};

INLINE Bit8u RealHandle(Bit16u handle) {
	
	DOS_PSP psp(dos.psp);	
	return psp.GetFileHandle((Bit8u)handle);
};

/* Dos Error Codes */
#define DOSERR_NONE 0
#define DOSERR_FUNCTION_NUMBER_INVALID 1
#define DOSERR_FILE_NOT_FOUND 2
#define DOSERR_PATH_NOT_FOUND 3
#define DOSERR_TOO_MANY_OPEN_FILES 4
#define DOSERR_ACCESS_DENIED 5
#define DOSERR_INVALID_HANDLE 6
#define DOSERR_MCB_DESTROYED 7
#define DOSERR_INSUFFICIENT_MEMORY 8
#define DOSERR_MB_ADDRESS_INVALID 9
#define DOSERR_ENVIRONMENT_INVALID 10
#define DOSERR_FORMAT_INVALID 11
#define DOSERR_ACCESS_CODE_INVALID 12
#define DOSERR_DATA_INVALID 13
#define DOSERR_RESERVED 14
#define DOSERR_FIXUP_OVERFLOW 14
#define DOSERR_INVALID_DRIVE 15
#define DOSERR_REMOVE_CURRENT_DIRECTORY 16
#define DOSERR_NOT_SAME_DEVICE 17
#define DOSERR_NO_MORE_FILES 18

/* Remains some classes used to access certain things */
class DOS_FCB {
public:
	DOS_FCB(PhysPt pt){
		off=pt;
	}
	DOS_FCB(Bit16u seg, Bit16u offset){
		off=PhysMake(seg,offset);
	}
	void Set_drive(Bit8u a);
	void Set_filename(char* a); //writes an the first 8 bytes of a as the filename
	void Set_ext(char* a);
	void Set_current_block(Bit16u a);
	void Set_record_size(Bit16u a);
	void Set_filesize(Bit32u a);
	void Set_date(Bit16u a);
	void Set_time(Bit16u a);
	void Set_current_record(Bit8u a);
	void Set_random_record(Bit32u a);
	// others nog yet handled
	Bit8u Get_drive(void);
	void Get_filename(char* a);
	void Get_ext(char* a);
	Bit16u Get_current_block(void);
	Bit16u Get_record_size(void);
	Bit32u Get_filesize(void);
	Bit16u Get_date(void);
	Bit16u Get_time(void);
	Bit8u  Get_current_record(void);
	Bit32u Get_random_record(void);
private:
	PhysPt off;
};

class DOS_ParamBlock {
public:
	DOS_ParamBlock(PhysPt pt){
		off=pt;
	};
	void InitExec(RealPt cmdtail);
	Bit16u loadseg(void);
	Bit16u relocation(void);
	Bit16u envseg(void);
	RealPt initsssp(void);
	RealPt initcsip(void);
	RealPt fcb1(void);
	RealPt fcb2(void);
	RealPt cmdtail(void);
private:
	PhysPt off;
};

class DOS_InfoBlock {
public:
	DOS_InfoBlock			(void) { seg=0; dib=0; };

	void	SetLocation		(Bit16u segment);
	void	SetFirstMCB		(RealPt pt);
	void	GetDIBPointer	(Bit16u& segment, Bit16u& offset);
	
private:
	#pragma pack (push,1)
	struct SDosInfoBlock {		
		Bit8u	stuff1[22];			// some stuff, hopefully never used....
		RealPt	firstMCB;			// first memory control block
		RealPt	firstDPB;			// first drive parameter block
		RealPt	firstFileTable;		// first system file table
		RealPt	activeClock;		// active clock device header
		RealPt	activeCon;			// active console device header
		Bit16u	maxSectorLength;	// maximum bytes per sector of any block device;
		RealPt	discInfoBuffer;		// pointer to disc info buffer
		RealPt  curDirStructure;	// pointer to current array of directory structure
		RealPt	fcbTable;			// pointer to system FCB table
		// some more stuff, hopefully never used.
	} GCC_ATTRIBUTE(packed);
	#pragma pack (pop)

	SDosInfoBlock*	dib;
	Bit16u			seg;
};

extern DOS_InfoBlock dosInfoBlock;

#endif

