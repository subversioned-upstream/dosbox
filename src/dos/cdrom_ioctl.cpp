/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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

// IOCTL support for linux CDROM

#if __linux__

#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/cdrom.h>

#include "cdrom.h"

CDROM_Interface_Linux::CDROM_Interface_Linux(void)
{
	pathname[0] = 0;
	memset(&oldLeadOut,0,sizeof(oldLeadOut));
};

CDROM_Interface_Linux::~CDROM_Interface_Linux(void)
{
	// Stop Audio, if neccessary
	StopAudio();
};

bool CDROM_Interface_Linux::SetDevice(char* path)
{
	strcpy(pathname,path);
	if (Open()) {
		Close();
		return true;
	};
	return false;
};

bool CDROM_Interface_Linux::Open(void)
{
	dhandle = open(pathname,O_RDONLY|O_NONBLOCK);
	return (dhandle>=0);
};

void CDROM_Interface_Linux::Close(void)
{
	close (dhandle);
};

bool CDROM_Interface_Linux::GetAudioTracks(int& stTrack, int& endTrack, TMSF& leadOut) 
{
    struct cdrom_tochdr header; 

	Open();
    /* read the header information */
    if (ioctl(dhandle, CDROMREADTOCHDR, &header) != 0) {
		Close();
		return false;
	}

    /* store the resulting information */
    stTrack		= header.cdth_trk0;		/* you can assume this to be zero */
    endTrack	= header.cdth_trk1;

	/* Get the leadout track */
	struct cdrom_tocentry entry; 
	
	entry.cdte_track	= CDROM_LEADOUT;	/* find the address of the leadout track */
	entry.cdte_format	= CDROM_MSF;		/* choose MSF addressing */

	if (ioctl(dhandle, CDROMREADTOCENTRY,&entry)!=0) {
		Close();
		return false;
	}

	leadOut.min	= entry.cdte_addr.msf.minute;
	leadOut.sec	= entry.cdte_addr.msf.second;
	leadOut.fr	= entry.cdte_addr.msf.frame; 	

	Close();
	return false;
};

bool CDROM_Interface_Linux::GetAudioTrackInfo	(int track, TMSF& start, unsigned char& attr)
{
	Open();
	struct cdrom_tocentry entry; 
	
	entry.cdte_track	= track;			/* find the address of the first track */
	entry.cdte_format	= CDROM_MSF;		/* choose MSF addressing */

	if (ioctl(dhandle, CDROMREADTOCENTRY,&entry)!=0) {
		Close();
		return false;
	}
	
	/* attribtute */
	attr		= (entry.cdte_adr<<4) | entry.cdte_ctrl;
	/* store the address information */
	start.min	= entry.cdte_addr.msf.minute;
	start.sec	= entry.cdte_addr.msf.second;
	start.fr	= entry.cdte_addr.msf.frame; 
	
	Close();
	return true;
};

bool CDROM_Interface_Linux::PlayAudioSector(unsigned long start,unsigned long len)
{
	struct cdrom_blk addr;
	addr.from = start;
	addr.len  = len;
	Open();
	bool res = (ioctl(dhandle, CDROMPLAYBLK, &addr)==0);
	Close();
	return res;
}

bool CDROM_Interface_Linux::StopAudio(void)
{
	Open();
	bool res = (ioctl(dhandle,CDROMSTOP,0)==0);
	Close();
	return res;
};

bool CDROM_Interface_Linux::PauseAudio(bool resume)
{
	Open();
	bool res = false;
	if (resume) res = (ioctl(dhandle, CDROMRESUME,0)==0);
	else		res = (ioctl(dhandle, CDROMPAUSE,0)==0);
	Close();
	return res;
};

bool CDROM_Interface_Linux::GetAudioSub(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos)
{
	struct cdrom_subchnl sub;

	Open();
	sub.cdsc_format = CDROM_MSF;
	if (ioctl(dhandle, CDROMSUBCHNL, &sub)!=0) {
		Close();
		return false;
	}

	/* attribute */
	attr		= (sub.cdsc_adr<<4) | sub.cdsc_ctrl;
	track		= sub.cdsc_trk;
	index		= sub.cdsc_ind;
	relPos.fr	= sub.cdsc_absaddr.msf.frame;
	relPos.sec	= sub.cdsc_absaddr.msf.second;
	relPos.min	= sub.cdsc_absaddr.msf.minute;
	absPos.fr	= sub.cdsc_reladdr.msf.frame;
	absPos.sec	= sub.cdsc_reladdr.msf.second;
	absPos.min	= sub.cdsc_reladdr.msf.minute;
	Close();
	return true;
};

bool CDROM_Interface_Linux::GetUPC(unsigned char& attr, char* upcdata)
{
	Open();
	bool res = (ioctl(dhandle, CDROM_GET_UPC, (void*)upcdata)==0);		
	Close();
	return res;
};

bool CDROM_Interface_Linux::GetAudioStatus(bool& playing, bool& pause)
{
	Open();
	struct cdrom_subchnl sub;

	sub.cdsc_format = CDROM_MSF;
	if (ioctl(dhandle, CDROMSUBCHNL, &sub)!=0) {
		Close();
		return false;
	};

	playing			= (sub.cdsc_audiostatus==CDROM_AUDIO_PLAY);
	pause			= (sub.cdsc_audiostatus==CDROM_AUDIO_PAUSED);

	Close();
	return true;
};

bool CDROM_Interface_Linux::LoadUnloadMedia(bool unload)
{
	Open();
	bool res = (ioctl(dhandle, CDROMEJECT,0)==0);
	Close();
	return res;
};

bool CDROM_Interface_Linux::GetMediaTrayStatus(bool& mediaPresent, bool& mediaChanged, bool& trayOpen)
{
	// Seems not possible to get this values using ioctl...
	int		track1,track2;
	TMSF	leadOut;
	// If we can read, there's a media
	mediaPresent = GetAudioTracks(track1, track2, leadOut);
	trayOpen	 = !mediaPresent;
	mediaChanged = (oldLeadOut.min!=leadOut.min) || (oldLeadOut.sec!=leadOut.sec) || (oldLeadOut.fr!=leadOut.fr);
	// Save old values
	oldLeadOut.min = leadOut.min;
	oldLeadOut.sec = leadOut.sec;
	oldLeadOut.fr  = leadOut.fr;
	// always success
	return true;
};

bool CDROM_Interface_Linux::ReadSectors(void* buffer, bool raw, unsigned long sector, unsigned long num)
{
	// FIXME: ToDo
	return false;
};

int CDROM_GetMountType(char* path)
// 0 - physical CDROM
// 1 - Iso file
// 2 - subdirectory
{
	// 1. Smells like a real cdrom 
	// FIXME: Better check if drive is a cdrom
	if ((strchr(path,'/')==strrchr(path,'/')) && strstr(path,"cdrom")) return 0;
	// 2. Iso file ?
	// FIXME : How to detect them ?
	// return 1;
	// 3. bah, ordinary directory
	return 2;
};

#elif defined (WIN32)


// *****************************************************************
// Windows IOCTL functions (not suitable for 95/98/Me)
// *****************************************************************

#include "cdrom.h"
#include <winioctl.h>			// Ioctl stuff
#include "ntddcdrm.h"			// Ioctl stuff

CDROM_Interface_Ioctl::CDROM_Interface_Ioctl()
{
	pathname[0] = 0;
	hIOCTL = INVALID_HANDLE_VALUE;
	memset(&oldLeadOut,0,sizeof(oldLeadOut));
};

CDROM_Interface_Ioctl::~CDROM_Interface_Ioctl()
{
	StopAudio();
};

bool CDROM_Interface_Ioctl::GetUPC(unsigned char& attr, char* upc)
{
	// FIXME : To Do
	return true;
}

bool CDROM_Interface_Ioctl::GetAudioTracks(int& stTrack, int& endTrack, TMSF& leadOut) 
{
	Open();
	CDROM_TOC toc;
	DWORD	byteCount;
	BOOL	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_TOC, NULL, 0, 
									&toc, sizeof(toc), &byteCount,NULL);
	Close();
	if (!bStat) return false;

	stTrack		= toc.FirstTrack;
	endTrack	= toc.LastTrack;
	leadOut.min = toc.TrackData[endTrack].Address[1];
	leadOut.sec	= toc.TrackData[endTrack].Address[2];
	leadOut.fr	= toc.TrackData[endTrack].Address[3];
	return true;
};

bool CDROM_Interface_Ioctl::GetAudioTrackInfo(int track, TMSF& start, unsigned char& attr)
{
	Open();
	CDROM_TOC toc;
	DWORD	byteCount;
	BOOL	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_TOC, NULL, 0, 
									&toc, sizeof(toc), &byteCount,NULL);
	Close();
	if (!bStat) return false;
	
	attr		= (toc.TrackData[track-1].Adr << 4) | toc.TrackData[track].Control;
	start.min	= toc.TrackData[track-1].Address[1];
	start.sec	= toc.TrackData[track-1].Address[2];
	start.fr	= toc.TrackData[track-1].Address[3];
	return true;
};	

bool CDROM_Interface_Ioctl::GetAudioSub(unsigned char& attr, unsigned char& track, unsigned char& index, TMSF& relPos, TMSF& absPos)
{
	Open();

	CDROM_SUB_Q_DATA_FORMAT insub;
	SUB_Q_CHANNEL_DATA sub;
	DWORD	byteCount;

	insub.Format = IOCTL_CDROM_CURRENT_POSITION;

	BOOL	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_Q_CHANNEL, &insub, sizeof(insub), 
									&sub, sizeof(sub), &byteCount,NULL);
	Close();
	if (!bStat) return false;

	attr		= (sub.CurrentPosition.ADR << 4) | sub.CurrentPosition.Control;
	track		= sub.CurrentPosition.TrackNumber;
	index		= sub.CurrentPosition.IndexNumber;
	relPos.min	= sub.CurrentPosition.TrackRelativeAddress[1];
	relPos.sec	= sub.CurrentPosition.TrackRelativeAddress[2];
	relPos.fr	= sub.CurrentPosition.TrackRelativeAddress[3];
	absPos.min	= sub.CurrentPosition.AbsoluteAddress[1];
	absPos.sec	= sub.CurrentPosition.AbsoluteAddress[2];
	absPos.fr	= sub.CurrentPosition.AbsoluteAddress[3];
	
	return true;
};

bool CDROM_Interface_Ioctl::GetAudioStatus(bool& playing, bool& pause)
{
	Open();

	CDROM_SUB_Q_DATA_FORMAT insub;
	SUB_Q_CHANNEL_DATA sub;
	DWORD byteCount;

	insub.Format = IOCTL_CDROM_CURRENT_POSITION;

	BOOL	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_READ_Q_CHANNEL, &insub, sizeof(insub), 
									&sub, sizeof(sub), &byteCount,NULL);
	Close();
	if (!bStat) return false;

	playing = (sub.CurrentPosition.Header.AudioStatus == AUDIO_STATUS_IN_PROGRESS);
	pause	= (sub.CurrentPosition.Header.AudioStatus == AUDIO_STATUS_PAUSED);

	return true;
};

bool CDROM_Interface_Ioctl::GetMediaTrayStatus(bool& mediaPresent, bool& mediaChanged, bool& trayOpen)
{
	// Seems not possible to get this values using ioctl...
	int		track1,track2;
	TMSF	leadOut;
	// If we can read, there's a media
	mediaPresent = GetAudioTracks(track1, track2, leadOut),
	trayOpen	 = !mediaPresent;
	mediaChanged = (oldLeadOut.min!=leadOut.min) || (oldLeadOut.sec!=leadOut.sec) || (oldLeadOut.fr!=leadOut.fr);
	// Save old values
	oldLeadOut.min = leadOut.min;
	oldLeadOut.sec = leadOut.sec;
	oldLeadOut.fr  = leadOut.fr;
	// always success
	return true;
};

bool CDROM_Interface_Ioctl::PlayAudioSector	(unsigned long start,unsigned long len)
{
	Open();
	CDROM_PLAY_AUDIO_MSF audio;
	DWORD	byteCount;
	// Start
	unsigned long addr	= start + 150;
	audio.StartingF = (UCHAR)(addr%75); addr/=75;
	audio.StartingS = (UCHAR)(addr%60); 
	audio.StartingM = (UCHAR)(addr/60);
	// End
	addr			= start + len + 150;
	audio.EndingF	= (UCHAR)(addr%75); addr/=75;
	audio.EndingS	= (UCHAR)(addr%60); 
	audio.EndingM	= (UCHAR)(addr/60);

	BOOL	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_PLAY_AUDIO_MSF, &audio, sizeof(audio), 
									NULL, 0, &byteCount,NULL);
	Close();
	return bStat>0;
};

bool CDROM_Interface_Ioctl::PauseAudio(bool resume)
{
	Open();
	BOOL bStat; 
	DWORD byteCount;
	if (resume) bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_RESUME_AUDIO, NULL, 0, 
										NULL, 0, &byteCount,NULL);	
	else		bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_PAUSE_AUDIO, NULL, 0, 
										NULL, 0, &byteCount,NULL);
	Close();
	return bStat>0;
};

bool CDROM_Interface_Ioctl::StopAudio(void)
{
	Open();
	BOOL bStat; 
	DWORD byteCount;
	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_STOP_AUDIO, NULL, 0, 
							NULL, 0, &byteCount,NULL);	
	Close();
	return bStat>0;
};

bool CDROM_Interface_Ioctl::LoadUnloadMedia(bool unload)
{
	Open();
	BOOL bStat; 
	DWORD byteCount;
	if (unload) bStat = DeviceIoControl(hIOCTL,IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, 
										NULL, 0, &byteCount,NULL);	
	else		bStat = DeviceIoControl(hIOCTL,IOCTL_STORAGE_LOAD_MEDIA, NULL, 0, 
										NULL, 0, &byteCount,NULL);	
	Close();
	return bStat>0;
};

bool CDROM_Interface_Ioctl::ReadSectors(void* buffer, bool raw, unsigned long sector, unsigned long num)
{
	// TODO : How to copy cooked without current overhead ?
	BOOL bStat;
	DWORD byteCount;
	RAW_READ_INFO in;
	char* inPtr;

	in.DiskOffset.LowPart	= sector;
	in.DiskOffset.HighPart	= 0;
	in.SectorCount			= num;
	in.TrackMode			= CDDA;
	
	if (!raw)	inPtr = new char[num*RAW_SECTOR_SIZE];
	else		inPtr = (char*)buffer;

	bStat = DeviceIoControl(hIOCTL,IOCTL_CDROM_RAW_READ, &in, sizeof(in), 
							inPtr, num*RAW_SECTOR_SIZE, &byteCount,NULL);

	if (!raw) {
		char* source = inPtr;
		source+=16; // jump 16 bytes
		char* outPtr = (char*)buffer;
		for (unsigned long i=0; i<num; i++) {
			memcpy(outPtr,source,COOKED_SECTOR_SIZE);
			outPtr+=COOKED_SECTOR_SIZE;
			source+=RAW_SECTOR_SIZE;
		};
		delete[] inPtr;
	};
	
	return (byteCount!=num*RAW_SECTOR_SIZE) && (bStat>0);
}

bool CDROM_Interface_Ioctl::SetDevice(char* path)
{
	if (GetDriveType(path)==DRIVE_CDROM) {
		char letter [3] = { 0, ':', 0 };
		letter[0] = path[0];
		strcpy(pathname,"\\\\.\\");
		strcat(pathname,letter);
		if (Open()) {
			Close();
			return true;
		};
	}
	return false;
}

bool CDROM_Interface_Ioctl::Open(void)
{
	hIOCTL	= CreateFile(pathname,			// drive to open
						GENERIC_READ,		// read access
						FILE_SHARE_READ |	// share mode
						FILE_SHARE_WRITE, 
						NULL,				// default security attributes
						OPEN_EXISTING,		// disposition
						0,					// file attributes
						NULL);				// do not copy file attributes
	return (hIOCTL!=INVALID_HANDLE_VALUE);
};

void CDROM_Interface_Ioctl::Close(void)
{
	CloseHandle(hIOCTL);
};

/*
int CDROM_GetMountType(char* path)
// 0 - physical CDROM
// 1 - Iso file
// 2 - subdirectory
{
	// 1. Smells like a real cdrom 
	if ((strlen(path)<=3) && (path[2]=='\\') && (strchr(path,'\\')==strrchr(path,'\\')) && 	(GetDriveType(path)==DRIVE_CDROM)) return 0;
	// 2. Iso file ?
	// FIXME : How to detect them ?
	// return 1;
	// 3. bah, ordinary directory
	return 2;
};
*/
#else

// Get Mounttype function for unsupported systems
// always return 0 - physical cdrom

int CDROM_GetMountType(char* path)
// Always return 0;
{
	return 0;
};

#endif