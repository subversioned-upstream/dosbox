/*
 *  Copyright (C) 2002-2007  The DOSBox Team
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

/* $Id: dos_devices.cpp,v 1.18 2007-06-14 08:23:46 qbix79 Exp $ */ 

#include <string.h>
#include "dosbox.h"
#include "callback.h"
#include "regs.h"
#include "mem.h"
#include "bios.h"
#include "dos_inc.h"
#include "support.h"
#include "drives.h" //Wildcmp
/* Include all the devices */

#include "dev_con.h"


DOS_Device * Devices[DOS_DEVICES];

class device_NUL : public DOS_Device {
public:
	device_NUL() { SetName("NUL"); };
	virtual bool Read(Bit8u * data,Bit16u * size) {
		for(Bitu i = 0; i < *size;i++) 
			data[i]=0; 
		LOG(LOG_IOCTL,LOG_NORMAL)("%s:READ",GetName());
		return true;
	}
	virtual bool Write(Bit8u * data,Bit16u * size) {
		LOG(LOG_IOCTL,LOG_NORMAL)("%s:WRITE",GetName());
		return true;
	}
	virtual bool Seek(Bit32u * pos,Bit32u type) {
		LOG(LOG_IOCTL,LOG_NORMAL)("%s:SEEK",GetName());
		return true;
	}
	virtual bool Close() { return true; }
	virtual Bit16u GetInformation(void) { return 0x8084; }
	virtual bool ReadFromControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode){return false;}
	virtual bool WriteToControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode){return false;}
};

class device_LPT1 : public device_NUL {
public:
   	device_LPT1() { SetName("LPT1");}
	Bit16u GetInformation(void) { return 0x80A0; }
};

bool DOS_Device::Read(Bit8u * data,Bit16u * size) {
	return Devices[devnum]->Read(data,size);
}

bool DOS_Device::Write(Bit8u * data,Bit16u * size) {
	return Devices[devnum]->Write(data,size);
}

bool DOS_Device::Seek(Bit32u * pos,Bit32u type) {
	return Devices[devnum]->Seek(pos,type);
}

bool DOS_Device::Close() {
	return Devices[devnum]->Close();
}

Bit16u DOS_Device::GetInformation(void) { 
	return Devices[devnum]->GetInformation();
}

bool DOS_Device::ReadFromControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode) { 
	return Devices[devnum]->ReadFromControlChannel(bufptr,size,retcode);
}

bool DOS_Device::WriteToControlChannel(PhysPt bufptr,Bit16u size,Bit16u * retcode) { 
	return Devices[devnum]->WriteToControlChannel(bufptr,size,retcode);
}

DOS_File::DOS_File(const DOS_File& orig) {
	flags=orig.flags;
	time=orig.time;
	date=orig.date;
	attr=orig.attr;
	refCtr=orig.refCtr;
	open=orig.open;
	name=0;
	if(orig.name) {
		name=new char [strlen(orig.name) + 1];strcpy(name,orig.name);
	}
}

DOS_File & DOS_File::operator= (const DOS_File & orig) {
	flags=orig.flags;
	time=orig.time;
	date=orig.date;
	attr=orig.attr;
	refCtr=orig.refCtr;
	open=orig.open;
	if(name) {
		delete [] name; name=0;
	}
	if(orig.name) {
		name=new char [strlen(orig.name) + 1];strcpy(name,orig.name);
	}
	return *this;
}

Bit8u DOS_FindDevice(char const * name) {
	/* should only check for the names before the dot and spacepadded */
	// STDAUX is alias for COM1
	// A bit of a hack, but no application will probably use stdaux to determine wether a directory exists
	if (strcasecmp(name, "STDAUX") == 0) name = "COM1";

	char temp[CROSS_LEN];//TODO
	if(!name || !(*name)) return DOS_DEVICES;
	strcpy(temp,name);
	char* dot= strrchr(temp,'.');
	if(dot && *dot) *dot=0; //no ext checking

	char* leading = strrchr(temp,'\\');
	if(leading) {
		*leading = 0;
		Bit8u drive;char fulldir[DOS_PATHLENGTH];
		if (!DOS_MakeName(temp,fulldir,&drive)) return DOS_DEVICES;
		if(!Drives[drive]->TestDir(fulldir)) return DOS_DEVICES;
		*leading='\\';
	}

	/* loop through devices */
	for(Bit8u index = 0;index < DOS_DEVICES;index++) {
		if (Devices[index]) {
			if (WildFileCmp(temp,Devices[index]->name)) return index;
		}
	}
	return DOS_DEVICES;
}


void DOS_AddDevice(DOS_Device * adddev) {
//Caller creates the device. We store a pointer to it
//TODO Give the Device a real handler in low memory that responds to calls
	for(Bitu i = 0; i < DOS_DEVICES;i++) {
		if(!Devices[i]){
			Devices[i] = adddev;
			Devices[i]->SetDeviceNumber(i);
			return;
		}
	}
	E_Exit("DOS:Too many devices added");
}

void DOS_DelDevice(DOS_Device * dev) {
// We will destroy the device if we find it in our list.
// TODO:The file table is not checked to see the device is opened somewhere!
	for (Bitu i = 0; i <DOS_DEVICES;i++) {
		if(Devices[i] && !strcasecmp(Devices[i]->name,dev->name)){
			delete Devices[i];
			Devices[i] = 0;
			return;
		}
	}
}

void DOS_SetupDevices(void) {
	DOS_Device * newdev;
	newdev=new device_CON();
	DOS_AddDevice(newdev);
	DOS_Device * newdev2;
	newdev2=new device_NUL();
	DOS_AddDevice(newdev2);
	DOS_Device * newdev3;
	newdev3=new device_LPT1();
	DOS_AddDevice(newdev3);
}
