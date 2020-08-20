/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *      Startup and quit functions. Handles signals, inits the
 *      memory management, then calls D_DoomMain. Also contains
 *      I_Init which does other system-related startup stuff.
 *
 *-----------------------------------------------------------------------------
 */

#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef WIFI_DEBUG
#include <user_debugger.h>
#endif

#include "doomdef.h"
#include "m_argv.h"
#include "d_main.h"
#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "lprintf.h"
#include "m_random.h"
#include "doomstat.h"
#include "g_game.h"
#include "m_misc.h"
#include "m_menu.h"
#include "i_sound.h"
#include "i_main.h"
#include "lprintf.h"
#ifdef USE_SDL
#include "SDL.h"
#endif
#include "doomstat.h"
#include "st_lib.h"
#include "d_deh.h"					// Jefklak 20/11/06 - allow grabbing of s_STRINGCAP vars
#include "version.h"				// ^ DS DOOM version number

#include <fat.h>
#include <dswifi9.h>
#include "KipSVN.h"					// KipVN Vars
char * KIP_iwadChoice;				// KipSVN - Includes the global selected WAD
char * KIP_version;					// KipSVN - The version that the SVN was based off
char * KIP_cmdline;					// KipSVN - Command line
int KIP_width;						// KipSVN - Includes screen width
int KIP_height;						// KipSVN - Includes screen height
int KIP_screenmode;					// KipSVN - Includes what sceen to run it in
const char * KIP_colours_0 = "\x1b[30m"; //Black
const char * KIP_colours_1 = "\x1b[34m"; //Dark Blue
const char * KIP_colours_2 = "\x1b[32m"; //Dark Green
const char * KIP_colours_3 = "\x1b[36m"; //Dark Aqua
const char * KIP_colours_4 = "\x1b[31m"; //Dark Red
const char * KIP_colours_5 = "\x1b[35m"; //Purple
const char * KIP_colours_6 = "\x1b[33m"; //Dark Yellow
const char * KIP_colours_7 = "\x1b[37m"; //Gray
const char * KIP_colours_8 = "\x1b[30;1m"; //Dark Gray
const char * KIP_colours_9 = "\x1b[34;1m"; //Blue

const char * KIP_colours_a = "\x1b[32;1m"; //Green
const char * KIP_colours_b = "\x1b[36;1m"; //Aqua
const char * KIP_colours_c = "\x1b[31;1m"; //Red
const char * KIP_colours_d = "\x1b[35;1m"; //Pink
const char * KIP_colours_e = "\x1b[33;1m"; //Yellow
const char * KIP_colours_f = "\x1b[37;1m"; //White

PrintConsole bottomScreen;

#ifdef WIFI_DEBUG
void debug_print_stub(char *string)
{
	printf(string);
}
#endif

// sgIP_dbgprint only needed in debug version
void sgIP_dbgprint(char * txt, ...) {	}

// interrupt handler to receive fifo messages from arm7
void arm9_fifo()
{ // check incoming fifo messages
	u32 value = REG_IPC_FIFO_RX;
	if(value == 0x87654321) Wifi_Sync();
}
//---------------------------------------------------------------------------------

int broken_pipe;

/* Most of the following has been rewritten by Lee Killough
 *
 * I_GetTime
 * killough 4/13/98: Make clock rate adjustable by scale factor
 * cphipps - much made static
 */

int realtic_clock_rate = 100;
static int_64_t I_GetTime_Scale = 1<<24;

static int I_GetTime_Scaled(void)
{
  return (int)( (int_64_t) I_GetTime_RealTime() * I_GetTime_Scale >> 24);
}



static int  I_GetTime_FastDemo(void)
{
  static int fasttic;
  return fasttic++;
}



static int I_GetTime_Error(void)
{
  I_Error("I_GetTime_Error: GetTime() used before initialization");
  return 0;
}



int (*I_GetTime)(void) = I_GetTime_Error;

void I_Init(void)
{
  /* killough 4/14/98: Adjustable speedup based on realtic_clock_rate */
  if (fastdemo)
    I_GetTime = I_GetTime_FastDemo;
  else
    if (realtic_clock_rate != 100)
      {
        I_GetTime_Scale = ((int_64_t) realtic_clock_rate << 24) / 100;
        I_GetTime = I_GetTime_Scaled;
      }
    else
      I_GetTime = I_GetTime_RealTime;

  {
    /* killough 2/21/98: avoid sound initialization if no sound & no music */
    extern boolean nomusicparm, nosfxparm;
    if (!(nomusicparm && nosfxparm))
      I_InitSound();
  }
}

/* cleanup handling -- killough:
 */
static void I_SignalHandler(int s)
{
/*
  char buf[2048];

#ifdef SIGPIPE
  // CPhipps - report but don't crash on SIGPIPE
  if (s == SIGPIPE) {
    fprintf(stderr, "Broken pipe\n");
    broken_pipe = 1;
    return;
  }
#endif
  signal(s,SIG_IGN);  // Ignore future instances of this signal.

  strcpy(buf,"Exiting on signal: ");
  I_SigString(buf+strlen(buf),2000-strlen(buf),s);

  // If corrupted memory could cause crash, dump memory
  // allocation history, which points out probable causes
  if (s==SIGSEGV || s==SIGILL || s==SIGFPE)
    Z_DumpHistory(buf);

  I_Error("I_SignalHandler: %s", buf);*/
}

/* killough 2/22/98: Add support for ENDBOOM, which is PC-specific
 *
 * this converts BIOS color codes to ANSI codes.
 * Its not pretty, but it does the job - rain
 * CPhipps - made static
 */

inline static int convert(int color, int *bold)
{
  if (color > 7) {
    color -= 8;
    *bold = 1;
  }
  switch (color) {
  case 0:
    return 0;
  case 1:
    return 4;
  case 2:
    return 2;
  case 3:
    return 6;
  case 4:
    return 1;
  case 5:
    return 5;
  case 6:
    return 3;
  case 7:
    return 7;
  }
  return 0;
}

/* CPhipps - flags controlling ENDOOM behaviour */
enum {
  endoom_colours = 1,
  endoom_nonasciichars = 2,
  endoom_droplastline = 4
};

unsigned int endoom_mode;

static void PrintVer(void)
{
  char vbuf[200];
  lprintf(LO_INFO,"%s\n",I_GetVersionString(vbuf,200));
}

/* I_EndDoom
 * Prints out ENDOOM or ENDBOOM, using some common sense to decide which.
 * cphipps - moved to l_main.c, made static
 */
static void I_EndDoom(void)
{
  int lump_eb, lump_ed, lump = -1;

  /* CPhipps - ENDOOM/ENDBOOM selection */
  lump_eb = W_CheckNumForName("ENDBOOM");/* jff 4/1/98 sign our work    */
  lump_ed = W_CheckNumForName("ENDOOM"); /* CPhipps - also maybe ENDOOM */

  if (lump_eb == -1)
    lump = lump_ed;
  else if (lump_ed == -1)
    lump = lump_eb;
  else
  { /* Both ENDOOM and ENDBOOM are present */
#define LUMP_IS_NEW(num) (!((lumpinfo[num].source == source_iwad) || (lumpinfo[num].source == source_auto_load)))
    switch ((LUMP_IS_NEW(lump_ed) ? 1 : 0 ) |
      (LUMP_IS_NEW(lump_eb) ? 2 : 0)) {
    case 1:
      lump = lump_ed;
      break;
    case 2:
      lump = lump_eb;
      break;
    default:
      /* Both lumps have equal priority, both present */
      lump = (P_Random(pr_misc) & 1) ? lump_ed : lump_eb;
      break;
    }
  }

  if (lump != -1)
  {
    const char (*endoom)[2] = (void*)W_CacheLumpNum(lump);
    int i, l = W_LumpLength(lump) / 2;

    /* cph - colour ENDOOM by rain */
    int oldbg = -1, oldcolor = -1, bold = 0, oldbold = -1, color = 0;
#ifndef _WIN32
    if (endoom_mode & endoom_nonasciichars)
      /* switch to secondary charset, and set to cp437 (IBM charset) */
      printf("\e)K\016");
#endif /* _WIN32 */

    /* cph - optionally drop the last line, so everything fits on one screen */
    if (endoom_mode & endoom_droplastline)
      l -= 80;
    lprintf(LO_INFO,"\n");
    for (i=0; i<l; i++)
    {
#ifdef _WIN32
      I_ConTextAttr(endoom[i][1]);
#elif defined (DJGPP)
      textattr(endoom[i][1]);
#else
      if (endoom_mode & endoom_colours)
      {
        if (!(i % 80))
        {
          /* reset everything when we start a new line */
          oldbg = -1;
          oldcolor = -1;
          printf("\e[0m\n");
        }
        /* foreground color */
        bold = 0;
        color = endoom[i][1] % 16;
        if (color != oldcolor)
        {
          oldcolor = color;
          color = convert(color, &bold);
          if (oldbold != bold)
          {
            oldbold = bold;
      printf("\e[%cm", bold + '0');
      if (!bold) oldbg = -1;
          }
          /* we buffer everything or output is horrendously slow */
          printf("\e[%dm", color + 30);
          bold = 0;
        }
        /* background color */
        color = endoom[i][1] / 16;
        if (color != oldbg)
        {
          oldbg = color;
          color = convert(color, &bold);
          printf("\e[%dm", color + 40);
        }
      }
#endif
      /* cph - portable ascii printout if requested */
      if (isascii(endoom[i][0]) || (endoom_mode & endoom_nonasciichars))
        lprintf(LO_INFO,"%c",endoom[i][0]);
      else /* Probably a box character, so do #'s */
        lprintf(LO_INFO,"#");
    }
#ifndef _WIN32
    lprintf(LO_INFO,"\b"); /* hack workaround for extra newline at bottom of screen */
    lprintf(LO_INFO,"\r");
    if (endoom_mode & endoom_nonasciichars)
      putchar('\017'); /* restore primary charset */
#endif /* _WIN32 */
    W_UnlockLumpNum(lump);
  }
#ifndef _WIN32
  if (endoom_mode & endoom_colours)
    puts("\e[0m"); /* cph - reset colours */
  PrintVer();
#else /* _WIN32 */
  I_uSleep(3000000); // CPhipps - don't thrash cpu in this loop
#endif /* _WIN32 */
}

static int has_exited;

/* I_SafeExit
 * This function is called instead of exit() by functions that might be called
 * during the exit process (i.e. after exit() has already been called)
 * Prevent infinitely recursive exits -- killough
 */

void I_SafeExit(int rc)
{
  if (!has_exited)    /* If it hasn't exited yet, exit now -- killough */
    {
      has_exited=rc ? 2 : 1;
      exit(rc);
    }
}

// Jefklak 19/11/06 - add ShutdownGraphics() call (why wasn't this used before?)
// Because if you shut down the graphics you can't see the error messages you moron!

// also turn DS power off.
void I_Quit (void)
{
  if (!has_exited)
    has_exited=1;   /* Prevent infinitely recursive exits -- killough */

  if (has_exited == 1) {
    I_EndDoom();
    if (demorecording)
      G_CheckDemoStatus();
    M_SaveDefaults ();
  }

}

#ifdef SECURE_UID
uid_t stored_euid = -1;
#endif

void StartWifi()
{
#ifdef WIFI_DEBUG
	set_verbosity(VERBOSE_INFO | VERBOSE_ERROR | VERBOSE_TRACE);
#endif

#ifdef WIFI
		
	iprintf("Connecting via WFC data...\n");
   	
	if(!Wifi_InitDefault(WFC_CONNECT)) {
		iprintf("Failed to connect!");
	} else {
	
		iprintf("Connected\n\n");
	} // if connected, you can now use the berkley sockets interface to connect to the internet!
	
#ifdef WIFI_DEBUG
	debugger_connect_tcp(192, 168, 1, 105);	//your IP here
	debugger_init();
	user_debugger_update();
#else
	defaultExceptionHandler();
#endif
#endif
}

void setAutoMap() {
	videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
	vramSetBankC(VRAM_C_SUB_BG);

	REG_BG3CNT_SUB = BG_BMP8_512x512;
	REG_BG3PA_SUB = KIP_width; //1 << 8;
	//REG_BG3PA_SUB = (320 * 256)/256; //1 << 8;
	REG_BG3PB_SUB = 0; // BG SCALING X
	REG_BG3PC_SUB = 0; // BG SCALING Y
	REG_BG3PD_SUB = 255; // << 8; must be 255 for some reason
	//REG_BG3PD_SUB = (200*256)/192; // << 8;
	REG_BG3X_SUB = 0;
	REG_BG3Y_SUB = 0;
	dmaFillWords(0,BG_GFX_SUB,128*1024);
}

// Jefklak 19/11/06 - Switches lower DS screen back to console or vice versa.
int gen_screen_swap = 0;
int gen_console_enable = 1;
void switchConsole()
{
	// ### LOWER SCREEN #### //
	if(gen_console_enable)
	{
		videoSetModeSub(MODE_0_2D|DISPLAY_BG1_ACTIVE);
		vramSetBankC(VRAM_C_SUB_BG);

		REG_BG1CNT_SUB = BG_MAP_BASE(31);
		BG_PALETTE_SUB[255] = RGB15(31,31,31);

		consoleInit(&bottomScreen,1, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
		
		FG = 0;

		lprintf(LO_INFO, "%s (FG buffer is %i)\n", s_CONSOLESWAPON, FG);
		if(gamestate == GS_LEVEL)
			players[consoleplayer].message = s_CONSOLESWAPON;
	}
	else
	{
		if(KIP_screenmode == 0)
		{
			setAutoMap();
		}
		if(!KIP_screenmode == 2)
		{
			gen_console_enable = true;
		}
		/**
		 * adjusted in st_lib.h (static int instead of #define)
		 * hu_lib.h includes st_lib.h and uses same FG screen[x] identifier
		 * if no console, draw stats to lower screen!
		 * Also increase upper screen display size.
		 **/
		FG = 1;
		if(gamestate == GS_LEVEL)
			players[consoleplayer].message = s_CONSOLESWAPOFF;
	}

	if(gamestate == GS_LEVEL)
		M_SizeDisplay(FG);
}

char *DS_USERNAME = NULL;
void DSgetUserName()
{
	int i;
	int nameLen = PersonalData->nameLen;
	DS_USERNAME = malloc(nameLen + 1);

	// safety fail
	if(nameLen <= 0)
	{
		DS_USERNAME = "Player1";
		return;
	}

	for(i=0; i < nameLen; i++)
	{
		// pretend to get ascii-bits from utf-16 name
		DS_USERNAME[i] = (char)PersonalData->name[i] & 255;
	}
	// zero terminate the string
	DS_USERNAME[i] = 0;
}

void systemErrorExit(int rc) {
   printf("exit with code %d\n",rc);
   printf("Press A to Quit\n");

   while(1) {
      swiWaitForVBlank();
      scanKeys();
      if (keysDown() & KEY_A) break;
   }
   
}

static int old_console;

void keyboardStart() {
	if(KIP_screenmode == 0)
	{
		old_console = gen_console_enable;
		gen_console_enable = 1;
		REG_DISPCNT_SUB &= ~DISPLAY_BG3_ACTIVE;
		dmaFillWords(0,BG_GFX_SUB,128*1024);
		dmaCopy(BG_PALETTE_SUB,BG_MAP_RAM_SUB(28),512);
	}
	keyboardInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x512, 29, 1, false, true);
	if(KIP_screenmode == 0)
	{
		BG_PALETTE_SUB[255] = RGB15(31,31,31);
	}
	if(KIP_screenmode == 2)
	{
		if(old_console==0) consoleInit(&bottomScreen,1, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
		consoleSetWindow(&bottomScreen, 0,0,32,14);
	}
	keyboardShow();
}

void singleScreen() //KipSVN
{
	old_console = gen_console_enable;
	gen_console_enable = 1;
	REG_DISPCNT_SUB &= ~DISPLAY_BG3_ACTIVE;
	dmaFillWords(0,BG_GFX_SUB,128*1024);
	dmaCopy(BG_PALETTE_SUB,BG_MAP_RAM_SUB(28),512);
	BG_PALETTE_SUB[255] = RGB15(31,31,31);
	if(KIP_screenmode == 2)
	{
		if(old_console==0) consoleInit(&bottomScreen,1, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
		consoleSetWindow(&bottomScreen, 0,0,32,14);
	}
}

void keyboardEnd() { 
	keyboardHide();
	if(KIP_screenmode == 0)
	{
		gen_console_enable = old_console;
		if (gen_console_enable == 0) {
			dmaCopy(BG_MAP_RAM_SUB(28),BG_PALETTE_SUB,512);
			setAutoMap();
			I_FinishUpdate();
		} else consoleSetWindow(&bottomScreen, 0,0,32,24);
	}else
	{
		I_FinishUpdate();
	}

}

//int main(int argc, const char * const * argv)
int main(int argc, char **argv)
{
	KIP_version = VER_DSDOOM;				// KipSVN - Set the version
	KIP_width = 256;					// KipSVN - Set the width
	KIP_height = 192;					// KipSVN - Set the height
	KIP_screenmode = 0; 				// KipSVN - Set whether to run in consolemode or singlescreen etc
	KIP_iwadChoice = "DOOM1.WAD";		// KipSVN - Set the default IWAD
	myargc = argc;
	myargv = argv;
	powerOn(POWER_ALL);
	soundEnable();
	defaultExceptionHandler();

	
	TIMER0_DATA=0;	// Set up the timer
	TIMER1_DATA=0;
	TIMER0_CR=TIMER_DIV_1024 | TIMER_ENABLE;
	TIMER1_CR=TIMER_CASCADE | TIMER_ENABLE;
 
	// Jefklak 19/11/06 - adjust upper/lower screen stuff
	// ### UPPER SCREEN #### //
	videoSetMode(MODE_5_2D|DISPLAY_BG3_ACTIVE);	// BG3 only - extended rotation
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);		// same as VRAM_A_MAIN_BG
	vramSetBankB(VRAM_B_MAIN_BG_0x06020000);		// use second bank for main screen - 256 KiB

	REG_BG3CNT = BG_BMP8_512x512;					// BG3 Control register, 8 bits
	//KipSVN
    REG_BG3PA = KIP_width; //1 << 8;
    //REG_BG3PA = (KIP_width * 256)/256; //1 << 8;
    REG_BG3PB = 0; // BG SCALING X
    REG_BG3PC = 0; // BG SCALING Y
    REG_BG3PD = 255; // << 8; must be 255 for some reason or it crops
	
	//REG_BG3PD = (KIP_height*256)/192; // << 8;
    REG_BG3X = 0;
    REG_BG3Y = 0;

	// clear upper screen (black) instead of junk
	switchConsole();
	memset(BG_GFX, 0, 512 * 512 * 2);
	swiWaitForVBlank();
	DSgetUserName();	// essential, retrieves username via Fifo buffer.

	consoleClear();
	//Build %s\n\n", DS_USERNAME, VER_DSDOOM);
	iprintf("Welcome %s!\n\n" , DS_USERNAME);
	//iprintf("This is DSDOOM SVN Version: %s\n", KIP_version);
	//iprintf("Based of DSDOOM Build %s\n\n", VER_DSDOOM);
	/*iprintf("This is DSDOOM SVN Version: %s\n", VER_DSDOOM);
	iprintf("Based of DSDOOM Build 1.2.1\n\n");
	iprintf("prBoom ported by:\nTheChuckster & WinterMute\n");
	iprintf("Updated by happy_bunny for latest tools/libs.\n");
	iprintf("some additions by JefKlak\n");
	iprintf("SVN by Kippykip and elhobbs\n\n");
	if (!fatInitDefault())
	{
		iprintf("Unable to initialize media device!\n");
	} else {
		iprintf("fatInitDefault(): initialized.\n");
	}*/
	//Make this BETR
	if (!fatInitDefault())
	{
		iprintf("Unable to Read/Write! Halted");
		while(true){}
	} else 
	{
		iprintf("Read/Write access!\n\n");
	}
	//KipSVN 1.4 - There is an entire new menu now so...
	/*iprintf("    Choose your game type\n\n");
	iprintf("         Standard game\n         Network game");
	
	int line = 16;
	while(1) {
		iprintf("\x1b[%d;7H]\x1b[15C[",line);
		swiWaitForVBlank();
		scanKeys();
		int keys = keysDown();
		iprintf("\x1b[%d;7H \x1b[15C ",line);
		if ((keys & KEY_UP) && line > 16)
		{
			line--;
		}
		if ((keys & KEY_DOWN) && line < 17)
		{
			line++;
		}
		if ( keys & KEY_A )
		{
			if(line == 16)
			{
				netgame = false;
			}else
			{
				netgame = true;
			}
			break;
		}
	}
	//New WAD Chooser - KipSVN
	consoleClear();
	//while(1){}
	
	
	line = 8;
	iprintf("\x1b[0;8HChoose IWAD\n\n");
	iprintf("\x1b[8;0H          Doom1.wad\n          Doom.wad\n          Doom2.wad\n          Plutonia.wad\n          Tnt.wad\n          Custom");
	while(1) 
	{
		iprintf("\x1b[%d;7H]\x1b[15C[",line);
		swiWaitForVBlank();
		scanKeys();
		int keys = keysDown();
		iprintf("\x1b[%d;7H \x1b[15C ",line);
		//if ( (keys & KEY_UP) && line == 13 ) line = 12;
		//if ( (keys & KEY_DOWN) && line == 12 ) line = 13;
		if ((keys & KEY_UP) && line > 8)
		{
			line--;
		}
		if ((keys & KEY_DOWN) && line < 12)
		{
			line++;
		}
		if ( keys & KEY_A )
		{
			if(line == 8)
			{
				KIP_iwadChoice = "doom1.wad";
			}
			if(line == 9)
			{
				KIP_iwadChoice = "doom.wad";
			}
			if(line == 10)
			{
				KIP_iwadChoice = "doom2.wad";
			}
			if(line == 11)
			{
				KIP_iwadChoice = "plutonia.wad";
			}
			if(line == 12)
			{
				KIP_iwadChoice = "tnt.wad";
			}
			break;
		}
	}*/
	consoleClear();
	//KipSVN The new menu - it's a bit messy but it does the job
	int K_currentMenu = 0; //0 is the main screen, 1 is the Game Type, 2 is screenmode, 3 is IWAD, 4 is Custom IWAD, 5 is Command Line, 6 is About
	int K_currentCursor = 5;
	while(1) 
	{
		//Lets get the Keys!
		swiWaitForVBlank();
		scanKeys();
		int keys = keysDown();
		
		if(K_currentMenu == 0) //Main menu
		{
			//Text stuff	
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 0, 11, KIP_colours_e, "DSDoom Menu");
			
			//Netgame Text
			if(netgame)
			{
				iprintf("\x1b[%d;%dH%s%s%s%s\x1b[37;1m", 6, 5, KIP_colours_a, "Game Type:  ", KIP_colours_b ,"Multiplayer");
			}else
			{
				iprintf("\x1b[%d;%dH%s%s%s%s\x1b[37;1m", 6, 5, KIP_colours_a, "Game Type: ", KIP_colours_b ,"Singleplayer");
			}
			
			//Screenmode Text
			if(KIP_screenmode == 0)
			{
				iprintf("\x1b[%d;%dH%s%s%s%s\x1b[37;1m", 7, 6, KIP_colours_a, "Screen Mode:    ", KIP_colours_b, "Dual");
			}
			if(KIP_screenmode == 1)
			{
				iprintf("\x1b[%d;%dH%s%s%s%s\x1b[37;1m", 7, 6, KIP_colours_a, "Screen Mode:  ", KIP_colours_b, "Single");
			}
			if(KIP_screenmode == 2)
			{
				iprintf("\x1b[%d;%dH%s%s%s%s\x1b[37;1m", 7, 6, KIP_colours_a, "Screen Mode: ", KIP_colours_b, "Console");
			}
			
			//IWAD Text
			iprintf("\x1b[%d;%dH%s%s%s%s\x1b[37;1m", 8, 7, KIP_colours_a, "IWAD: ", KIP_colours_b, KIP_iwadChoice);
			
			//CMDLine Text
			iprintf("\x1b[%d;%dH%s%s%s%s\x1b[37;1m", 9, 2, KIP_colours_a, "Cmd Line: ", KIP_colours_b, KIP_cmdline);
			
			//About
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 10, 13, KIP_colours_a, "About");
			
			//START THE DAMN GAME ALREADY!
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 12, 10, KIP_colours_c, "Start DSDoom");
			
			//Cursor stuff
			if(keys & KEY_DOWN && K_currentCursor < 5) //Move the cursor down unless its greater than...
			{
				K_currentCursor++;
				consoleClear();
			}
			if(keys & KEY_UP && K_currentCursor > 0) //Move the cursor up unless it's 0
			{
				K_currentCursor--;
				consoleClear();
			}
			if(K_currentCursor == 0) //Game Type cursor
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 6, 3, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 6, 29, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 0;
					K_currentMenu = 1; //Set the menu to Game Type
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
				}
			}
			
			if(K_currentCursor == 1) //Screen mode cursor
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 7, 4, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 7, 27, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 0;
					K_currentMenu = 2; //Set the menu to screenmode
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
				}
			}
			
			if(K_currentCursor == 2) //IWAD cursor
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 8, 5, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 8, 26, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 0;
					K_currentMenu = 3; //Set the menu to IWAD
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
				}
			}
			if(K_currentCursor == 3) //CmdLine cursor
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 9, 0, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 9, 31, "]"); //Why 32 when you can go 31
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 0;
					K_currentMenu = 5; //Set the menu to CmdLine
					scanKeys(); //Reset the keys!
					keys = keysDown();
					//keyboardStart(); //Show the keyboard
					consoleClear();
				}
			}
			
			if(K_currentCursor == 4) //About cursor
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 10, 11, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 10, 19, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 0;
					K_currentMenu = 6; //Set the menu to About
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
				}
			}
			
			if(K_currentCursor == 5) //START DA GAEM
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 12, 8, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 12, 23, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 0;
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
					break; //Stop the loop
				}
			}
			
			if(keys & KEY_START) //Start the game
			{
				K_currentCursor = 0;
				scanKeys(); //Reset the keys!
				keys = keysDown();
				consoleClear();
				break; //Stop the loop
			}
		}
		
		if(K_currentMenu == 1) //Game Type menu
		{
			//Text stuff
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 0, 12, KIP_colours_e, "Game Type");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 6, 11, KIP_colours_a, "Singleplayer");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 7, 11, KIP_colours_a, "Multiplayer");
			
			//Cursor stuff
			if(keys & KEY_DOWN && K_currentCursor < 1) //Move the cursor down unless its greater than...
			{
				K_currentCursor++;
				consoleClear();
			}
			if(keys & KEY_UP && K_currentCursor > 0) //Move the cursor up unless it's 0
			{
				K_currentCursor--;
				consoleClear();
			}
			if(K_currentCursor == 0)
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 6, 9, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 6, 24, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 0;
					K_currentMenu = 0; //Go back to the first menu
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
					netgame = false; //Set the Vars!
				}
			}
			
			if(K_currentCursor == 1)
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 7, 9, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 7, 23, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 0;
					K_currentMenu = 0; //Go back to the first menu
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
					netgame = true; //Set the Vars!
				}
			}
			
			if(keys & KEY_B) //Lets go back if B is pressed
			{
				K_currentCursor = 0;
				K_currentMenu = 0; //Go back to the first menu
				scanKeys(); //Reset the keys!
				keys = keysDown();
				consoleClear();
			}
		}
		
		if(K_currentMenu == 2) //Screen Mode menu
		{
			//Text stuff
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 0, 11, KIP_colours_e, "Screen Mode");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 6, 14, KIP_colours_a, "Dual");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 7, 13, KIP_colours_a, "Single");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 8, 13, KIP_colours_a, "Console");
			
			//Cursor stuff
			if(keys & KEY_DOWN && K_currentCursor < 2) //Move the cursor down unless its greater than...
			{
				K_currentCursor++;
				consoleClear();
			}
			if(keys & KEY_UP && K_currentCursor > 0) //Move the cursor up unless it's 0
			{
				K_currentCursor--;
				consoleClear();
			}
			if(K_currentCursor == 0)
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 6, 12, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 6, 19, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 1;
					K_currentMenu = 0; //Go back to the first menu
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
					KIP_screenmode = 0; //Set the Vars!
				}
			}
			
			if(K_currentCursor == 1)
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 7, 11, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 7, 20, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 1;
					K_currentMenu = 0; //Go back to the first menu
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
					KIP_screenmode = 1; //Set the Vars!
				}
			}
			
			if(K_currentCursor == 2)
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 8, 11, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 8, 21, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 1;
					K_currentMenu = 0; //Go back to the first menu
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
					KIP_screenmode = 2; //Set the Vars!
				}
			}
			
			if(keys & KEY_B) //Lets go back if B is pressed
			{
				K_currentCursor = 1;
				K_currentMenu = 0; //Go back to the first menu
				scanKeys(); //Reset the keys!
				keys = keysDown();
				consoleClear();
			}
		}
		
		if(K_currentMenu == 3) //IWAD menu
		{
			//Text stuff
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 0, 11, KIP_colours_e, "Select IWAD");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 6, 12, KIP_colours_a, "DOOM1.WAD");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 7, 12, KIP_colours_a, "DOOM.WAD");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 8, 12, KIP_colours_a, "DOOM2.WAD");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 9, 11, KIP_colours_a, "PLUTONIA.WAD");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 10, 13, KIP_colours_a, "TNT.WAD");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 11, 13, KIP_colours_a, "Custom");
			
			//Cursor stuff
			if(keys & KEY_DOWN && K_currentCursor < 5) //Move the cursor down unless its greater than...
			{
				K_currentCursor++;
				consoleClear();
			}
			if(keys & KEY_UP && K_currentCursor > 0) //Move the cursor up unless it's 0
			{
				K_currentCursor--;
				consoleClear();
			}
			if(K_currentCursor == 0) //Doom1.wad
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 6, 10, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 6, 22, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 2;
					K_currentMenu = 0; //Go back to the first menu
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
					KIP_iwadChoice = "DOOM1.WAD"; //Set the Vars!
				}
			}
			
			if(K_currentCursor == 1) //Doom.wad
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 7, 10, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 7, 21, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 2;
					K_currentMenu = 0; //Go back to the first menu
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
					KIP_iwadChoice = "DOOM.WAD"; //Set the Vars!
				}
			}
			
			if(K_currentCursor == 2) //Doom2.wad
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 8, 10, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 8, 22, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 2;
					K_currentMenu = 0; //Go back to the first menu
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
					KIP_iwadChoice = "DOOM2.WAD"; //Set the Vars!
				}
			}
			
			if(K_currentCursor == 3) //Plutonia.wad
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 9, 9, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 9, 24, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 2;
					K_currentMenu = 0; //Go back to the first menu
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
					KIP_iwadChoice = "PLUTONIA.WAD"; //Set the Vars!
				}
			}
			
			if(K_currentCursor == 4) //Tnt.wad
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 10, 11, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 10, 21, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 2;
					K_currentMenu = 0; //Go back to the first menu
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
					KIP_iwadChoice = "TNT.WAD"; //Set the Vars!
				}
			}
			
			if(K_currentCursor == 5) //Custom
			{
				//Draw the cursor
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 11, 11, "[");
				iprintf("\x1b[%d;%dH%s\x1b[37;1m", 11, 20, "]");
				//If you press A...
				if(keys & KEY_A)
				{
					K_currentCursor = 2;
					K_currentMenu = 4; //Go back to the next menu
					scanKeys(); //Reset the keys!
					keys = keysDown();
					consoleClear();
				}
			}
			
			if(keys & KEY_B) //Lets go back if B is pressed
			{
				K_currentCursor = 2;
				K_currentMenu = 0; //Go back to the first menu
				scanKeys(); //Reset the keys!
				keys = keysDown();
				consoleClear();
			}
		}
		
		if(K_currentMenu == 4) //Custom IWAD menu
		{
			//Text stuff
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 0, 8, KIP_colours_c, "WORK IN PROGESS!");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 6, 6, KIP_colours_4, "Press 'B' to go back");
			
			if(keys & KEY_B) //Lets go back if B is pressed
			{
				K_currentCursor = 5;
				K_currentMenu = 3; //Go back to the IWAD menu
				scanKeys(); //Reset the keys!
				keys = keysDown();
				consoleClear();
			}
		}
		
		if(K_currentMenu == 5) //CMDLine menu
		{
			//Text stuff
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 0, 8, KIP_colours_c, "WORK IN PROGESS!");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 6, 6, KIP_colours_4, "Press 'B' to go back");
			if(keys & KEY_B) //Lets go back if B is pressed
			{
				K_currentCursor = 3;
				K_currentMenu = 0; //Go back to the menu
				scanKeys(); //Reset the keys!
				keys = keysDown();
				//keyboardEnd();
				consoleClear();
			}
		}
		if(K_currentMenu == 6) //About menu
		{
			//Text stuff
			consoleClear();
			iprintf("%sThis is DSDOOM SVN Version: %s\n", KIP_colours_e, VER_DSDOOM);
			iprintf("Based of DSDOOM Build 1.2.1\n\n");
			iprintf("prBoom ported by:\nTheChuckster & WinterMute\n");
			iprintf("Updated by happy_bunny for latest tools/libs.\n");
			iprintf("some additions by JefKlak\n");
			iprintf("SVN by Kippykip and elhobbs\n\n");
			iprintf("\x1b[%d;%dH%s%s\x1b[37;1m", 12, 6, KIP_colours_c, "Press 'B' to go back");
			if(keys & KEY_B) //Lets go back if a button is pressed
			{
				K_currentCursor = 4;
				K_currentMenu = 0; //Go back to the menu
				scanKeys(); //Reset the keys!
				keys = keysDown();
				//keyboardEnd();
				consoleClear();
			}
		}
	}
	
	
	
  /* Version info */
  lprintf(LO_INFO,"\n");
  PrintVer();

  /* cph - Z_Close must be done after I_Quit, so we register it first. */
  //atexit(Z_Close);
  /*
     killough 1/98:

     This fixes some problems with exit handling
     during abnormal situations.

     The old code called I_Quit() to end program,
     while now I_Quit() is installed as an exit
     handler and exit() is called to exit, either
     normally or abnormally. Seg faults are caught
     and the error handler is used, to prevent
     being left in graphics mode or having very
     loud SFX noise because the sound card is
     left in an unstable state.
  */

  Z_Init();                  /* 1/18/98 killough: start up memory stuff first */

  atexit(I_Quit);
/*
#ifndef _DEBUG
  signal(SIGSEGV, I_SignalHandler);
#ifdef SIGPIPE
  signal(SIGPIPE, I_SignalHandler); // CPhipps - add SIGPIPE, as this is fatal
#endif
  signal(SIGTERM, I_SignalHandler);
  signal(SIGFPE,  I_SignalHandler);
  signal(SIGILL,  I_SignalHandler);
  signal(SIGINT,  I_SignalHandler);  // killough 3/6/98: allow CTRL-BRK during init
  signal(SIGABRT, I_SignalHandler);
#endif
*/
  /* cphipps - call to video specific startup code */
  I_PreInitGraphics();
  D_DoomMain();
  return 0;
}
