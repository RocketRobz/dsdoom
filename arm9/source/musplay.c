#ifdef ARM9
#include <nds.h> 
#include <fat.h> 
#endif

#include <stdio.h> 
#include <string.h>
#include <malloc.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <process.h>
#endif

#include <stdint.h>
typedef uintptr_t	Bitu;
typedef intptr_t	Bits;
typedef uint32_t	Bit32u;
typedef int32_t		Bit32s;
typedef uint16_t	Bit16u;
typedef int16_t		Bit16s;
typedef uint8_t		Bit8u;
typedef int8_t		Bit8s;

//#define DEBUG 1

#ifdef WIN32
typedef signed char s8;
typedef unsigned char byte;
typedef signed short s16;
typedef unsigned short u16;
typedef unsigned int u32;
#define iprintf printf
#endif


#ifdef ARM9
typedef vu32 DSTIME;
DSTIME ds_sound_start;
DSTIME ds_time()
{
	static DSTIME last = 0;
	static DSTIME time2 = 0;
	u16 time1 = TIMER3_DATA;
	if(time1 < last) {
		time2 ++;
	}
	last = time1;
	return (time2<<16) + time1;
}

void ds_set_timer(int rate) {
	if(rate == 0) {
		TIMER_CR(0) = 0;
		TIMER_CR(1) = 0;
		TIMER_CR(2) = 0;
	} else {
		TIMER_DATA(2) = 0x10000 - (0x1000000 / rate) * 2;
		TIMER_CR(2) = TIMER_ENABLE | TIMER_DIV_1;
		TIMER_DATA(3) = 0;
		TIMER_CR(3) = TIMER_ENABLE | TIMER_CASCADE | TIMER_DIV_1;
	}
}

DSTIME ds_sample_pos() {
	DSTIME v;

	v = (ds_time() - ds_sound_start);
	
	return v;
}
#endif

#define PU_STATIC 0

volatile int freeze_all = 0;

void *W_CacheLumpName(char *name,int unused) {
	int len;
	FILE *f = fopen(name,"rb");
	byte *buf;

	unused = 0;

	fseek(f,0,SEEK_END);
	len = ftell(f);
	fseek(f,0,SEEK_SET);

	buf = (byte *)malloc(len+1);
	fread( buf, 1, len, f );
	buf[len] = 0;

	return buf;
}

void W_ReleaseLumpName(char *name) {
}



byte adlib_write_reg(byte reg, byte data) {
#ifdef ARM9
	fifoSendValue32( FIFO_USER_01, ( reg << 8 ) | data ); 
#endif
	return 0;
}

/*
 * Write to an operator pair. To be used for register bases of 0x20, 0x40,
 * 0x60, 0x80 and 0xE0.
 */
void adlib_write_channel(byte regbase, byte channel, byte data1, byte data2)
{
    static byte adlib_op[] = {0, 1, 2, 8, 9, 10, 16, 17, 18};
	u16 reg = regbase+adlib_op[channel];

	adlib_write_reg(reg, data1);
	adlib_write_reg(reg+3, data2);
}

/*
 * Write to channel a single value. To be used for register bases of
 * 0xA0, 0xB0 and 0xC0.
 */
void adlib_write_value(byte regbase, byte channel, byte value)
{
    u16 chan;

	chan = channel;
    adlib_write_reg(regbase + chan, value);
}

/*
 * Write frequency/octave/keyon data to a channel
 */
void adlib_write_freq(byte channel, s16 freq, byte octave, byte keyon)
{
    adlib_write_value(0xA0, channel, (byte)freq);
    adlib_write_value(0xB0, channel, (byte)(freq>>8) | (octave << 2) | (keyon << 5));
}

/*
static s16 freqtable[] = {
	172, 183, 194, 205, 217, 230, 244, 258, 274, 290, 307, 326,
	345, 365, 387, 410, 435, 460, 488, 517, 547, 580, 615, 651,
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,
	690, 731, 774, 820, 869, 921, 975, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023};
static char octavetable[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1};

void mus_write_frequency(byte Adlibchannel, s16 note, int pitch, byte keyOn)
{
    s16 freq = freqtable[note];
    char octave = octavetable[note];

    if (pitch > 0) {
		int freq2;
		int octave2 = octavetable[note+1] - octave;

#ifdef DEBUG
		printf("DEBUG: pitch: N: %d  P: %d\n", note, pitch);
#endif

		if (octave2) {
			octave++;
			freq >>= 1;
		}
		freq2 = freqtable[note+1] - freq;
		freq += (freq2 * pitch) / 64;
    } else {
		if (pitch < 0) {
			int freq2;
			int octave2 = octave - octavetable[note-1];

#ifdef DEBUG
			printf("DEBUG: pitch: N: %d  P: %d\n", note, pitch);
#endif

			if (octave2) {
				octave--;
				freq <<= 1;
			}
			freq2 = freq - freqtable[note-1];
			freq -= (freq2 * -pitch) / 64;
		}
	}
    adlib_write_freq(Adlibchannel, freq, octave, keyOn);
}
*/


static u16 freqtable[] = {					 /* note # */
	345, 365, 387, 410, 435, 460, 488, 517, 547, 580, 615, 651,  /*  0 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 12 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 24 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 36 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 48 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 60 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 72 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 84 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651,  /* 96 */
	690, 731, 774, 820, 869, 921, 975, 517, 547, 580, 615, 651, /* 108 */
	690, 731, 774, 820, 869, 921, 975, 517};		    /* 120 */

static byte octavetable[] = {					 /* note # */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,			     /*  0 */
	0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,			     /* 12 */
	1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,			     /* 24 */
	2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,			     /* 36 */
	3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4,			     /* 48 */
	4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5,			     /* 60 */
	5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6,			     /* 72 */
	6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7,			     /* 84 */
	7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8,			     /* 96 */
	8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9,			    /* 108 */
	9, 9, 9, 9, 9, 9, 9,10};				    /* 120 */

//#define HIGHEST_NOTE 102
#define HIGHEST_NOTE 127

static u16 pitchtable[] = {				    /* pitch wheel */
	 29193U,29219U,29246U,29272U,29299U,29325U,29351U,29378U,  /* -128 */
	 29405U,29431U,29458U,29484U,29511U,29538U,29564U,29591U,  /* -120 */
	 29618U,29644U,29671U,29698U,29725U,29752U,29778U,29805U,  /* -112 */
	 29832U,29859U,29886U,29913U,29940U,29967U,29994U,30021U,  /* -104 */
	 30048U,30076U,30103U,30130U,30157U,30184U,30212U,30239U,  /*  -96 */
	 30266U,30293U,30321U,30348U,30376U,30403U,30430U,30458U,  /*  -88 */
	 30485U,30513U,30541U,30568U,30596U,30623U,30651U,30679U,  /*  -80 */
	 30706U,30734U,30762U,30790U,30817U,30845U,30873U,30901U,  /*  -72 */
	 30929U,30957U,30985U,31013U,31041U,31069U,31097U,31125U,  /*  -64 */
	 31153U,31181U,31209U,31237U,31266U,31294U,31322U,31350U,  /*  -56 */
	 31379U,31407U,31435U,31464U,31492U,31521U,31549U,31578U,  /*  -48 */
	 31606U,31635U,31663U,31692U,31720U,31749U,31778U,31806U,  /*  -40 */
	 31835U,31864U,31893U,31921U,31950U,31979U,32008U,32037U,  /*  -32 */
	 32066U,32095U,32124U,32153U,32182U,32211U,32240U,32269U,  /*  -24 */
	 32298U,32327U,32357U,32386U,32415U,32444U,32474U,32503U,  /*  -16 */
	 32532U,32562U,32591U,32620U,32650U,32679U,32709U,32738U,  /*   -8 */
	 32768U,32798U,32827U,32857U,32887U,32916U,32946U,32976U,  /*    0 */
	 33005U,33035U,33065U,33095U,33125U,33155U,33185U,33215U,  /*    8 */
	 33245U,33275U,33305U,33335U,33365U,33395U,33425U,33455U,  /*   16 */
	 33486U,33516U,33546U,33576U,33607U,33637U,33667U,33698U,  /*   24 */
	 33728U,33759U,33789U,33820U,33850U,33881U,33911U,33942U,  /*   32 */
	 33973U,34003U,34034U,34065U,34095U,34126U,34157U,34188U,  /*   40 */
	 34219U,34250U,34281U,34312U,34343U,34374U,34405U,34436U,  /*   48 */
	 34467U,34498U,34529U,34560U,34591U,34623U,34654U,34685U,  /*   56 */
	 34716U,34748U,34779U,34811U,34842U,34874U,34905U,34937U,  /*   64 */
	 34968U,35000U,35031U,35063U,35095U,35126U,35158U,35190U,  /*   72 */
	 35221U,35253U,35285U,35317U,35349U,35381U,35413U,35445U,  /*   80 */
	 35477U,35509U,35541U,35573U,35605U,35637U,35669U,35702U,  /*   88 */
	 35734U,35766U,35798U,35831U,35863U,35895U,35928U,35960U,  /*   96 */
	 35993U,36025U,36058U,36090U,36123U,36155U,36188U,36221U,  /*  104 */
	 36254U,36286U,36319U,36352U,36385U,36417U,36450U,36483U,  /*  112 */
	 36516U,36549U,36582U,36615U,36648U,36681U,36715U,36748U}; /*  120 */


static void mus_write_frequency(u32 slot, u32 note, int pitch, u32 keyOn)
{
    u32 freq = freqtable[note];
    u32 octave = octavetable[note];

    if (pitch) {
#ifdef DEBUG
		printf("DEBUG: pitch: N: %d  P: %d           \n", note, pitch);
#endif
		if (pitch > 127) {
			pitch = 127;
		} else if (pitch < -128) {
			pitch = -128;
		}
		freq = ((u32)freq * pitchtable[pitch + 128]) >> 15;
		if (freq >= 1024) {
			freq >>= 1;
			octave++;
		}
    }
    if (octave > 7) {
		octave = 7;
	}
    adlib_write_freq(slot, freq, octave, keyOn);
}

#if 0
/* OPL2 instrument */
typedef struct
{
	/*00*/	byte    trem_vibr_1;	/* OP 1: tremolo/vibrato/sustain/KSR/multi */
	/*01*/	byte	att_dec_1;	/* OP 1: attack rate/decay rate */
	/*02*/	byte	sust_rel_1;	/* OP 1: sustain level/release rate */
	/*03*/	byte	wave_1;		/* OP 1: waveform select */
	/*04*/	byte	scale_1;	/* OP 1: key scale level */
	/*05*/	byte	level_1;	/* OP 1: output level */
	/*06*/	byte	feedback;	/* feedback/AM-FM (both operators) */
	/*07*/	byte    trem_vibr_2;	/* OP 2: tremolo/vibrato/sustain/KSR/multi */
	/*08*/	byte	att_dec_2;	/* OP 2: attack rate/decay rate */
	/*09*/	byte	sust_rel_2;	/* OP 2: sustain level/release rate */
	/*0A*/	byte	wave_2;		/* OP 2: waveform select */
	/*0B*/	byte	scale_2;	/* OP 2: key scale level */
	/*0C*/	byte	level_2;	/* OP 2: output level */
	/*0D*/	byte	unused;
	/*0E*/	s16		basenote;	/* base note offset */
} PACKEDATTR OPL2instrument2_t;

#endif

static void mus_write_modulation(u32 slot, byte *instr, int state)
{
    if (state) {
		state = 0x40;	/* enable Frequency Vibrato */
	}
    adlib_write_channel(0x20, slot,
		(instr[6] & 1) ? (instr[7] | state) : instr[0],
		instr[7] | state);
}


/*
 * Adjust volume value (register 0x40)
 */
byte adlib_convert_volume(byte data, s16 volume)
{
    static byte volumetable[128] = {
	  0,   1,   3,   5,   6,   8,  10,  11,
	 13,  14,  16,  17,  19,  20,  22,  23,
	 25,  26,  27,  29,  30,  32,  33,  34,
	 36,  37,  39,  41,  43,  45,  47,  49,
	 50,  52,  54,  55,  57,  59,  60,  61,
	 63,  64,  66,  67,  68,  69,  71,  72,
	 73,  74,  75,  76,  77,  79,  80,  81,
	 82,  83,  84,  84,  85,  86,  87,  88,
	 89,  90,  91,  92,  92,  93,  94,  95,
	 96,  96,  97,  98,  99,  99, 100, 101,
	101, 102, 103, 103, 104, 105, 105, 106,
	107, 107, 108, 109, 109, 110, 110, 111,
	112, 112, 113, 113, 114, 114, 115, 115,
	116, 117, 117, 118, 118, 119, 119, 120,
	120, 121, 121, 122, 122, 123, 123, 123,
	124, 124, 125, 125, 126, 126, 127, 127};
    s16 n;

    if (volume > 127)
	volume = 127;
    n = 63 - (data & 63);
    n = (n*(int)volumetable[volume]) >> 7;
    n = 63 - n;
    return n | (data & 0xC0);
}

byte mus_pan_volume(s16 volume, int pan)
{
    if (pan >= 0)
	return volume;
    else
	return (volume * (pan+64)) / 64;
}

/*
 * Write volume data to a channel
 */
void mus_write_volume(byte channel, byte data[16], byte volume)
{
    adlib_write_channel(0x40, channel, 
		((data[6] & 1) ? (adlib_convert_volume(data[5], volume) | data[4]) : (data[5] | data[4])),
		adlib_convert_volume(data[12], volume) | data[11]);
}

/*
 * Write pan (balance) data to a channel
 */
void mus_write_pan(byte channel, byte data[16], int pan)
{
    byte bits;
    if (pan < -36) {
		bits = 0x10;		// left
	} else if (pan > 36) {
		bits = 0x20;		// right
	} else {
		bits = 0x30;		// both
	}

    adlib_write_value(0xC0, channel, data[6] | bits);
}

/*
 * Write an instrument to a channel
 *
 * Instrument layout:
 *
 *   Operator1  Operator2  Descr.
 *    data[0]    data[7]   reg. 0x20 - tremolo/vibrato/sustain/KSR/multi
 *    data[1]    data[8]   reg. 0x60 - attack rate/decay rate
 *    data[2]    data[9]   reg. 0x80 - sustain level/release rate
 *    data[3]    data[10]  reg. 0xE0 - waveform select
 *    data[4]    data[11]  reg. 0x40 - key scale level
 *    data[5]    data[12]  reg. 0x40 - output level
 *          data[6]        reg. 0xC0 - feedback/AM-FM (both operators)
 */
void mus_write_instrument(byte channel, byte data[16])
{
//    WriteChannel(0x80, channel, 0x0F, 0x0F);
    adlib_write_channel(0x40, channel, 0x3F, 0x3F);		// no volume
    adlib_write_channel(0x20, channel, data[0], data[7]);
    adlib_write_channel(0x60, channel, data[1], data[8]);
    adlib_write_channel(0x80, channel, data[2], data[9]);
    adlib_write_channel(0xE0, channel, data[3], data[10]);
    adlib_write_value  (0xC0, channel, data[6] | 0x30);
}

void adlib_init(Bit32u samplerate);
/*
 * Initialize hardware upon startup
 */
void adlib_init_hw()
{
    int i;
#ifdef ARM9
	// Wait a while so we know the ARM7 has started up the AdLib emulation.
	for (i=0; i < 10; i++ )
		swiWaitForVBlank();
#endif

	adlib_write_reg(0x01, 0x20);		// enable Waveform Select
	adlib_write_reg(0x08, 0x40);		// turn off CSW mode
	adlib_write_reg(0xBD, 0x00);		// set vibrato/tremolo depth to low, set melodic mode

	for(i = 0; i < 9; i++) {
		adlib_write_channel(0x40, i, 0x3F, 0x3F);	// turn off volume
		adlib_write_value(0xB0, i, 0);			// KEY-OFF
    }
}

#ifdef __GNUC__

#define PACKEDATTR __attribute__((packed))
 
#else
 
#define PACKEDATTR
 
#endif
 
// #define OPL_MIDI_DEBUG

#define MAXMIDLENGTH (96 * 1024)
#define GENMIDI_NUM_INSTRS  128

#define GENMIDI_HEADER          "#OPL_II#"
#define GENMIDI_FLAG_FIXED      0x0001         /* fixed pitch */
#define GENMIDI_FLAG_2VOICE     0x0004         /* double voice (OPL3) */

typedef struct
{
    byte tremolo;
    byte attack;
    byte sustain;
    byte waveform;
    byte scale;
    byte level;
} PACKEDATTR genmidi_op_t;

typedef struct
{
    genmidi_op_t modulator;
    byte feedback;
    genmidi_op_t carrier;
    byte unused;
    short base_note_offset;
} PACKEDATTR genmidi_voice_t;

typedef struct
{
    unsigned short flags;
    byte fine_tuning;
    byte fixed_note;

    genmidi_voice_t voices[2];
} PACKEDATTR genmidi_instr_t;

typedef struct {
	char			ID[4];		// identifier "MUS" 0x1A
	unsigned short	scoreLen;
	unsigned short	scoreStart;
	unsigned short	channels;
	unsigned short	dummy1;
	unsigned short  instrCnt;
	unsigned short	dummy2;
//	unsigned short	instruments[];
} PACKEDATTR mus_header_t;

// GENMIDI lump instrument data:

static genmidi_instr_t *main_instrs;
static genmidi_instr_t *percussion_instrs;

// Load instrument table from GENMIDI lump:

static int mus_load_instruments(void)
{
    byte *lump;

    lump = (byte *)W_CacheLumpName("GENMIDI", PU_STATIC);

    // Check header

    if (strncmp((char *) lump, GENMIDI_HEADER, strlen(GENMIDI_HEADER)) != 0)
    {
        W_ReleaseLumpName("GENMIDI");

        return 0;
    }

    main_instrs = (genmidi_instr_t *) (lump + strlen(GENMIDI_HEADER));
    percussion_instrs = main_instrs + GENMIDI_NUM_INSTRS;

    return 1;
}


byte* mus_load_music(char *name)
{
    byte *lump = (byte *)name; //Lets just directly load it from the 'name'
	//byte *lump = (byte *)W_CacheLumpName(name, PU_STATIC);
	mus_header_t *header = (mus_header_t *)lump;

	if(lump == 0) {
		iprintf("mus_load_music: failed\n");
		return 0;
	}

    if (header->ID[0] != 'M' ||
	header->ID[1] != 'U' ||
	header->ID[2] != 'S' ||
	header->ID[3] != 0x1A)
    {
		iprintf("mus_load_music: failed\n");
		return 0;
    }

    return lump + header->scoreStart;
}

void mus_init_music() {
	if(mus_load_instruments()) {
		iprintf("instruments loaded\n");
	} else {
		iprintf("instruments NOT loaded\n");
	}
}

#define PERCUSSION 15		// percussion channel
#define CHANNELS 16
#define OUTPUTCHANNELS 16
byte		channelFlags[CHANNELS];	// flags for channel
s16			channelInstr[CHANNELS];		// instrument #
byte		channelVolume[CHANNELS];	// volume
byte		channelLastVolume[CHANNELS];	// last volume
s8			channelPan[CHANNELS];	// pan, 0=normal
s8			channelPitch[CHANNELS];	// pitch wheel, 0=normal
byte		channelSustain[CHANNELS];	// sustain pedal value
byte		channelModulation[CHANNELS];	// modulation pot value
int			Adlibtime[OUTPUTCHANNELS];	// Adlib channel start time
s16			Adlibchannel[OUTPUTCHANNELS];	// Adlib channel & note #
byte		Adlibnote[OUTPUTCHANNELS];	// Adlib channel note
byte		*Adlibinstr[OUTPUTCHANNELS];	// Adlib channel instrument address
byte		Adlibvolume[OUTPUTCHANNELS];	// Adlib channel volume
byte		Adlibfinetune[OUTPUTCHANNELS];// Adlib 2nd channel pitch difference

#define CH_SECONDARY	0x01
#define CH_SUSTAIN		0x02
#define CH_VIBRATO		0x04		/* set if modulation >= MOD_MIN */
#define CH_FREE			0x80

#define MOD_MIN			40		/* vibrato threshold */

volatile int	MUStime;
volatile byte  *MUSdata;
volatile int	MUSticks;
volatile s16	playingAtOnce = 0;
volatile s16	playingPeak = 0;
volatile s16	playingChannels = 0;

volatile byte   *score;
int	singlevoice = 0;

int mus_release_channel(s16 i, s16 killed)
{
    s16 channel = (Adlibchannel[i] >> 8) & 0x7F;
#ifdef DEBUG
    printf("\nDEBUG: Release  Ch: %d  Adl: %d  %04X\n", channel, i, Adlibchannel[i]);
#endif
    playingChannels--;
    mus_write_frequency(i, Adlibnote[i], Adlibfinetune[i]+channelPitch[channel], 0);
    Adlibchannel[i] = 0xffff;
    if (killed) {
		adlib_write_channel(0x80, i, 0x0F, 0x0F);	// release rate - fastest
		adlib_write_channel(0x40, i, 0x3F, 0x3F);	// no volume
    }
    return i;
}

static int mus_release_sustain(int channel)
{
    int i;

    for(i = 0; i < 9; i++) {
		if (((Adlibchannel[i] & 0x7FFF) == channel) && (channelFlags[i] & CH_SUSTAIN))
		{
			mus_release_channel(i, 0);
		}
	}
    return 0;
}

int mus_find_free_channel(s16 flag)
{
    static int last = -1;
    int i;
    s16 latest = -1;
    int latesttime = MUStime;

    for(i = 0; i < 9; i++) {
		if (++last == 9)
			last = 0;
		if (Adlibchannel[last] == -1)
			return last;
    }

    if (flag & 1) {
		return -1;
	}

    for(i = 0; i < 9; i++) {
		if ((Adlibchannel[i] & 0x8000)) {
	#ifdef DEBUG
			printf("\nDEBUG: Kill 2nd %04X\n", Adlibchannel[i]);
	#endif
			mus_release_channel(i, -1);
			return i;
		} else {
			if (Adlibtime[i] < latesttime) {
				latesttime = Adlibtime[i];
				latest = i;
			}
		}
	}

	if ( !(flag & 2) && latest != -1) {
#ifdef DEBUG
		printf("DEBUG: Kill %04X !!!\n", Adlibchannel[latest]);
#endif
		mus_release_channel(latest, -1);
		return latest;
	}

#ifdef DEBUG
    printf("DEBUG: Full!!!\n");
#endif
    return -1;
}

int mus_occupy_channel(s16 i, s16 channel, byte note, int volume, byte *instr,s16 flag)
{
    playingChannels++;
    Adlibchannel[i] = (channel << 8) | note | (flag << 15);
    Adlibtime[i] = MUStime;
    if (volume == -1) {
		volume = channelLastVolume[channel];
	} else {
		channelLastVolume[channel] = volume;
	}
    volume = channelVolume[channel] * (Adlibvolume[i] = volume) / 127;
    if (instr[0] & 1) {
		note = instr[3];
	} else if (channel == PERCUSSION) {
		note = 60;			// C-5
	}
    if (flag && (instr[0] & 4)) {
		Adlibfinetune[i] = instr[2] - 0x80;
	} else {
		Adlibfinetune[i] = 0;
	}
    if (flag) {
		instr += 16+4;
		channelFlags[channel] = CH_SECONDARY;
	} else {
		instr += 4;
		channelFlags[channel] = CH_SECONDARY;
	}
    if (channelModulation[channel] >= MOD_MIN) {
		channelFlags[channel] |= CH_VIBRATO;
	}
    if ( (note += (*((s16 *)(instr+14)))) < 0) {
		while ((note += 12) < 0);
	} else if (note > HIGHEST_NOTE) {
		while ((note -= 12) > HIGHEST_NOTE);
	}
    Adlibnote[i] = note;


    mus_write_instrument(i, Adlibinstr[i] = instr);
    if (channelFlags[channel] & CH_VIBRATO) {
		mus_write_modulation(i, instr, 1);
	}
    mus_write_pan(i, instr, channelPan[channel]);
    mus_write_volume(i, instr, volume);
    //Adlibnote[i] = note += instr[14] + 12;
    mus_write_frequency(i, note, Adlibfinetune[i]+channelPitch[channel], 1);
    return i;
}

// code 1: play note
void mus_play_note(s16 channel, byte note, int volume)
{
    int i; // orignote = note;
    byte *instr;
	s16 instrnumber;

    if (channel == PERCUSSION) {
		if (note < 35 || note > 81) {
			return;			// wrong percussion number
		}
		instrnumber = (128-35) + note;
    } else {
		instrnumber = channelInstr[channel];
	}
    instr = (byte *)&main_instrs[instrnumber];//&instruments[instrnumber*INSTRSIZE];
#ifdef DEBUG
    printf("\rDEBUG: play: Ch: %d  N: %d  V: %d (%d)  I: %d (%s)  Fi: %d\r\n           ",
	channel, note, volume, channelVolume[channel], instrnumber, "UNK"/*instrumentName[instrnumber]*/,
	(instr[0] & 4) ? (instr[2] - 0x80) : 0);
#endif

    if ( (i = mus_find_free_channel((channel == PERCUSSION) ? 2 : 0)) != -1) {
		mus_occupy_channel(i, channel, note, volume, instr, 0);
		if (!singlevoice && instr[0] == 4) {
			if ( (i = mus_find_free_channel((channel == PERCUSSION) ? 3 : 1)) != -1) {
				mus_occupy_channel(i, channel, note, volume, instr, 1);
			}
		}
    }
}

// code 0: release note
void mus_release_note(byte channel, byte note)
{
    int i;
	int sustain = channelSustain[channel];

#ifdef DEBUG
    printf("DEBUG: release: Ch: %d  N: %d\n           ", channel, note);
#endif
    channel = (channel << 8) | note;
    for(i = 0; i < 9; i++) {
		if ((Adlibchannel[i] & 0x7FFF) == channel)
		{
			if (sustain < 0x40) {
				mus_release_channel(i, 0);
			} else {
				channelFlags[i] |= CH_SUSTAIN;
			}
	//	    return;
		}
	}
}

// code 2: change pitch wheel (bender)
void mus_pitch_wheel(byte channel, int pitch)
{
    int i;

#ifdef DEBUG
    printf("DEBUG: pitch: Ch: %d  P: %d\n", channel, pitch);
#endif
    channelPitch[channel] = pitch;
    for(i = 0; i < 9; i++) {
		if (((Adlibchannel[i] >> 8) & 0x7F) == channel) {
			Adlibtime[i] = MUStime;
			mus_write_frequency(i, Adlibnote[i], Adlibfinetune[i]+pitch, 1);
		}
	}
}

// code 4: change control
void mus_change_control(byte channel, byte controller, int value)
{
    int i;
#ifdef DEBUG
    printf("DEBUG: ctrl: Ch: %d  C: %d  V: %d\n", channel, controller, value);
#endif

    switch (controller) {
	case 0:	// change instrument
	    channelInstr[channel] = value;
	    break;
	case 2:	// change modulation
	    channelModulation[channel] = value;
	    for(i = 0; i < 9; i++) {
			if (((Adlibchannel[i] >> 8) & 0x7F) == channel)
			{
				byte flags = channelFlags[i];
				Adlibtime[i] = MUStime;
				if(value >= MOD_MIN) {
					channelFlags[i] |= CH_VIBRATO;
					if(channelFlags[i] != flags) {
						mus_write_modulation(i,Adlibinstr[i],1);
					} else {
						channelFlags[i] &= ~CH_VIBRATO;
						if(channelFlags[i] != flags) {
							mus_write_modulation(i,Adlibinstr[i],0);
						}
					}
				}
				mus_write_volume(i, Adlibinstr[i], value * Adlibvolume[i] / 127);
			}
		}
	    break;
	case 3:	// change volume
	    channelVolume[channel] = value;
	    for(i = 0; i < 9; i++) {
			if (((Adlibchannel[i] >> 8) & 0x7F) == channel)
			{
				Adlibtime[i] = MUStime;
				mus_write_volume(i, Adlibinstr[i], value * Adlibvolume[i] / 127);
			}
		}
	    break;
	case 4:	// change pan (balance)
	    channelPan[channel] = value -= 64;
	    for(i = 0; i < 9; i++) {
			if (((Adlibchannel[i] >> 8) & 0x7F) == channel) {
				Adlibtime[i] = MUStime;
				mus_write_pan(i, Adlibinstr[i], value);
			}
		}
	    break;
	case 8: //release sustain
	    channelSustain[channel] = value;
	    if (value < 0x40) {
			mus_release_sustain(channel);
		}
		break;
	default:
		iprintf("ctrl: %d\n",controller);
    }
}

void mus_stop_all()
{
	int channel = 0;
	while(channel < 8)
	{
		mus_release_channel(channel, -1);
		channel++;
		//printf("stop the musix plz");
	}
	//mus_release_note(channel, *data++);
}

byte *mus_play_tick(byte *data)
{
    for(;;) {
		byte command = (*data >> 4) & 7;
		byte channel = *data & 0x0F;
		byte last = *data & 0x80;
		data++;

		switch (command) {
		case 0:	// release note
			playingAtOnce--;
			mus_release_note(channel, *data++);
			break;
		case 1: {	// play note
			byte note = *data++;
			playingAtOnce++;
			if (playingAtOnce > playingPeak)
				playingPeak = playingAtOnce;
			if (note & 0x80)	// note with volume
				mus_play_note(channel, note & 0x7F, *data++);
			else
				mus_play_note(channel, note, -1);
			}
			break;
		case 2:	// pitch wheel
			mus_pitch_wheel(channel, *data++ - 0x80);
			break;
		case 3:	{// system event (valueless controller)
				byte ctrl = *data++;
				mus_change_control(channel, ctrl, 0);
				}
			break;
		case 4:	{// change control 
				byte ctrl = *data++;
				byte value = *data++;
				mus_change_control(channel, ctrl, value);
				}
			break;
		case 6:	// end
			mus_stop_all();
			return NULL;
		case 5:	// ???
		case 7:	// ???
			break;
		}
		if (last)
			break;
    }
    return data;
}

byte *mus_delay_ticks(byte *data, int *delaytime)
{
    int time = 0;

    do {
		time <<= 7;
		time += *data & 0x7F;
    } while (*data++ & 0x80);

    *delaytime = time;
    return data;
}

void mus_play_timer(void)
{
	int n;
    //BYTE keyflags = *(BYTE far *)MK_FP(0x0040, 0x0018);
	if(freeze_all ) return;

    playingPeak = playingAtOnce;
    if (!MUSdata) {
		return;
	}
    if (!MUSticks) {// || keyflags & 4) {
		MUSdata = mus_play_tick((byte *)MUSdata);
		if (MUSdata == NULL) {
			//if (loopForever)
			MUSdata = score;
			return;
		}
		MUSdata = mus_delay_ticks((byte *)MUSdata, (int *)&MUSticks);
		//MUSticks = 1;//(MUSticks * 75) / 10;
	//	MUSticks *= 8;
		MUStime += MUSticks;
    }
    MUSticks--;
//    MUStime++;
}

#ifdef WIN32
unsigned int __stdcall TimerThread(void *p_thread_data)
{
  HANDLE event_handle = CreateEvent(NULL, FALSE, FALSE, NULL);

  while(1)
  {
    switch(WaitForSingleObject(event_handle, 1)) // do something 1000 timers per second
    {
      case WAIT_TIMEOUT:
        mus_play_timer();
    }
  }

  return(0);
}

void mus_setup_timer() {
  unsigned int thread_id = 0;

	_beginthreadex(NULL, 0, TimerThread, NULL, 0, &thread_id);
	SetThreadPriority(thread_id,THREAD_PRIORITY_TIME_CRITICAL);
}

#endif

#ifdef ARM9
void mus_setup_timer() {
	timerStart( 2, ClockDivider_1024, TIMER_FREQ_1024( 140 ), mus_play_timer );
}
#endif

void mus_play_music(char *name)
{
	int lasttime;
	int mus_i;
	mus_stop_all();
    memset(Adlibchannel, 0xFF, sizeof(Adlibchannel));
    for (mus_i = 0; mus_i < CHANNELS; mus_i++) {
		channelVolume[mus_i] = 127; 	// default volume 127 (full volume)
		channelLastVolume[mus_i] = 100;
    }

    score = MUSdata = mus_load_music(name);
    MUSticks = 0;
    MUStime = 0;
    lasttime = 0xFFFFFFFF;
	
	mus_setup_timer();
	
    /*if (SetupTimer())
    {
	printf("FATAL ERROR: Cannot initialize 1024 Hz timer. Aborting.\n");
	return;
    }*/

    /*for(;;) {
		if (!MUSdata)		// no more music
			break;

#ifdef ARM9
		while((keysCurrent() & KEY_A)) {
			freeze_all = 1;
		}
		freeze_all = 0;
#endif

		if (lasttime != MUStime && freeze_all == 0)
		{
			int playtime = ((lasttime = MUStime)*1000)/1024;
			iprintf("Playing: %d:%d:%d  %d (%d)    \n", playtime / 60000,
			(playtime / 1000) % 60, playtime % 1000, playingAtOnce, playingChannels);
		}
		//mus_paint();
    }*/
	
    /*ShutdownTimer();
    cprintf("\r\n");

    //DeinitAdlib();*/
}

void mus_test() {
	mus_init_music();
	mus_play_music("D_E1M3.mus");
}

/*int main(int argc,char *argv[]) {
#ifdef ARM9
	int i;
	consoleDemoInit();  //setup the sub screen for printing
	i = fatInitDefault();
	iprintf("fat: %d\n",i);
	defaultExceptionHandler();
#endif
	adlib_init_hw();
	mus_test();
	do {
#ifdef WIN32
		Sleep(100);
#endif
#ifdef ARM9
		swiWaitForVBlank();
#endif
	} while(1);
	return 0;
}*/