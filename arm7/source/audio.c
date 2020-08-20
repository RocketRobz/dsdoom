/*---------------------------------------------------------------------------------

DSx86 AdLib emulation ARM7 main module

Copyright (C) 2010
Patrick Aalto (Pate)

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.
2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.
3.	This notice may not be removed or altered from any source
distribution.

Used channels:
	Channel 2..10 = AdLib emulation (9 channels)

---------------------------------------------------------------------------------*/
#include <nds.h>
//#include <dswifi7.h>
//#include <maxmod7.h>
//#define ADLIBC

#define FIFO_ADLIB FIFO_USER_01

extern void 	PutAdLibBuffer(int);
extern void 	AdlibEmulator();

//---------------------------------------------------------------------------------
void VcountHandler() {
//---------------------------------------------------------------------------------
	inputGetAndSend();
}

//---------------------------------------------------------------------------------
void AdLibHandler(u32 value, void * userdata) {
//---------------------------------------------------------------------------------
	PutAdLibBuffer(value);
}

//---------------------------------------------------------------------------------
int main() {
//---------------------------------------------------------------------------------
	irqInit();
	fifoInit();

	// read User Settings from firmware
	readUserSettings();

	// Start the RTC tracking IRQ
	initClockIRQ();

	SetYtrigger(80);

	//installWifiFIFO();
	installSoundFIFO();
	//mmInstall(FIFO_MAXMOD);

	installSystemFIFO();

#ifndef ADLIBC
	fifoSetValue32Handler(FIFO_ADLIB, AdLibHandler, 0);
#endif

	TIMER0_CR = 0;
	
	irqSet(IRQ_VCOUNT, VcountHandler);

	REG_SOUNDCNT = SOUND_ENABLE;
	REG_MASTER_VOLUME = 127;

	irqEnable( IRQ_VCOUNT | IRQ_NETWORK );
#ifdef ADLIBC
	while(1) {
		swiWaitForVBlank();
	}
#else
	AdlibEmulator();		// We never return from here
#endif
	return 0;
}
