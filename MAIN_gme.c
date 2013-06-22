/*
** Winamp Game_Music_Emu Plugin, Pre-Alpha Version
** Based on example Winamp Raw Plugin and example Music Player from the GME library.
** This is VERY early code and I expect it will crash or just plain not compile instead 
** of actually doing what it needs to do.
**
** Copyright (C) 2013, Dylan J Morrison <insidious@gmail.com>
** 
** Permission to use, copy, modify, and/or distribute this software for any purpose with or
** without fee is hereby granted, provided that the above copyright notice and this permission
** notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS 
** SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL 
** THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
** WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, 
** NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
** OF THIS SOFTWARE.
*/

#include <windows.h>
#include <stdio.h>
#include <time.h>

#include "in2.h"

#include "gme.h"

// avoid CRT. Evil. Big. Bloated. Only uncomment this code if you are using 
// 'ignore default libraries' in VC++. Keeps DLL size way down.
// /*
BOOL WINAPI _DllMainCRTStartup(HANDLE hInst, ULONG ul_reason_for_call, LPVOID lpReserved)
{
	return TRUE;
}
// */

// post this to the main window at end of file (after playback as stopped)
#define WM_WA_MPEG_EOF WM_USER+2


// raw configuration.
#define NCH 2
#define SAMPLERATE 44100
#define BPS 16

char lastfn[MAX_PATH];	// currently playing file (used for getting info on the current file)

int file_length;		// file length, in bytes
int decode_pos_ms;		// current decoding position, in milliseconds. 
						// Used for correcting DSP plug-in pitch changes
int paused;				// are we paused?
int track;                        // Track within file.
int track_count;                  // Number of tracks within file
long track_length;			  // Length of current track in ms
gme_info_t* track_info;           // GME info on track.

HANDLE input_file=INVALID_HANDLE_VALUE; // input file handle

volatile int killDecodeThread=0;			// the kill switch for the decode thread
HANDLE thread_handle=INVALID_HANDLE_VALUE;	// the handle to the decode thread

Music_Emu* emu;
BOOL accurate;
DWORD WINAPI DecodeThread(LPVOID b); // the decode thread procedure

// function definitions

void config(HWND hwndParent);
void about(HWND hwndParent);
void init();
void quit();
int isourfile(const char *fn);
int play(const char *fn);
void pause();
void unpause();
int ispaused();
void stop();
int getlength();
int getoutputtime();
void setoutputtime(int time_in_ms);
void setvolume(int volume);
void setpan(int pan);
int infoDlg(const char *fn, HWND hwnd);
void getfileinfo(const char *filename, char *title, int *length_in_ms);
void eq_set(int on, char data[10], int preamp);
int get_576_samples(char *buf);
DWORD WINAPI DecodeThread(LPVOID b);
__declspec( dllexport ) In_Module * winampGetInModule2();



// module definition.

In_Module mod = 
{
	IN_VER,	// defined in IN2.H
#ifdef DEBUG
	"Game_Music_Emu 0.6.0 Winamp Plugin v0.1 DEBUG "
#else
	"Game_Music_Emu 0.6.0 Winamp Plugin v0.1 "
#endif
	// winamp runs on both alpha systems and x86 ones. :)
#ifdef __alpha
	"(AXP)"
#else
	"(x86)"
#endif
	,
	0,	// hMainWindow (filled in by winamp)
	0,  // hDllInstance (filled in by winamp)
	"SPC\0SNES SPC700 Sound File (*.SPC)\0VGM\0Sega Genesis/MD/SMS/BBC Micro VGM File (*.VGM)\0AY\0ZX Spectrum/Amstrad CPC Sound File (*.AY)\0GBS\0Game Boy Sound File (*.GBS)\0HES\0TG-16/PC-Engine Sound File (*.HES)\0KSS\0MSX Sound File (*.KSS)\0NSF\0NES Sound File (*.NSF)\0SAP\0Atari Sound File (*.SAP)\0GYM\0Sega Genesis Sound File (*.GYM)\0"
	// this is a double-null limited list. "EXT\0Description\0EXT\0Description\0" etc.
	,
	0,	// is_seekable
	1,	// uses output plug-in system
	config,
	about,
	init,
	quit,
	getfileinfo,
	infoDlg,
	isourfile,
	play,
	pause,
	unpause,
	ispaused,
	stop,
	
	getlength,
	getoutputtime,
	setoutputtime,

	setvolume,
	setpan,

	0,0,0,0,0,0,0,0,0, // visualization calls filled in by winamp

	0,0, // dsp calls filled in by winamp

	eq_set,

	NULL,		// setinfo call filled in by winamp

	0 // out_mod filled in by winamp

};


void config(HWND hwndParent)
{
	MessageBox(hwndParent,
		"No configuration yet! Defaults to 44100 rate, 16-bit, Stereo, using Accurate Emulation where available.",
		"Configuration",MB_OK);
	// if we had a configuration box we'd want to write it here (using DialogBox, etc)
}
void about(HWND hwndParent)
{
	MessageBox(hwndParent,"Game_Music_Emu 0.6.0 Winamp Plugin, Plugin by Dylan Morrison, Library by Blargg",
		"About Game_Music_Emu Plugin",MB_OK);
}

#ifdef DEBUG
FILE * fp;
#endif

void init() { 
#ifdef DEBUG
	time_t now = time(0);
	char out[9];
	strftime(out, 9, "%Y%m%d", localtime(&now));
	char filename[22];
	sprintf(filename, "in_gme-debug-%s", out);
	fp = fopen(filename, "w+");
#endif
	emu = NULL;
	accurate = TRUE;
}

void debugmessage(char *message) {
#ifdef DEBUG
	fprintf(fp, "%s", message);
#endif
	return;
}

void quit() { 
	debugmessage("FUNCTION: quit()");
	emu = NULL;
#ifdef DEBUG
	debugmessage("END FUNCTION: quit()");
	debugmessage("END LOG");
	fclose(fp);
#endif
	return;
}

int isourfile(const char *fn) { 
	debugmessage("FUNCTION: isourfile()");
	debugmessage("END FUNCTION: isourfile()");
	return 0; 
} 

int play(const char *fn) 
{ 
	debugmessage("FUNCTION: play()");
	int maxlatency;
	int thread_id;

	// Grab a filehandle just to get the filesize. There's gotta be a better way to do this...
	input_file = CreateFile(fn,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,
		OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if (input_file == INVALID_HANDLE_VALUE)
	{
		return 1;
	}

	file_length=GetFileSize(input_file,NULL);
	
	CloseHandle(input_file);		
	
	gme_err_t temperr = gme_open_file(fn,&emu,SAMPLERATE);
	if (temperr != 0) // error opening file
	{
		return 1;
	}
	gme_enable_accuracy( emu, accurate );
	track = 0;
	track_count = gme_track_count( emu );
	gme_start_track ( emu, track );
	gme_free_info ( track_info	);
	track_info = NULL;
	gme_track_info( emu, &track_info, track );
	track_length = track_info->length;
	if ( track_length <= 0 ) {
		track_length = track_info->intro_length + track_info->loop_length * 2;
	}
	if ( track_length <= 0 )
		track_length = (long) (2.5 * 60 * 1000);
	gme_set_fade(emu, track_length);

	
	paused=0;
	decode_pos_ms=0;

	strcpy(lastfn,fn);

	maxlatency = mod.outMod->Open(SAMPLERATE,NCH,BPS, -1,-1); 

	if (maxlatency < 0) // error opening device
	{
		CloseHandle(input_file);
		input_file=INVALID_HANDLE_VALUE;
		return 1;
	}
	mod.SetInfo((SAMPLERATE*BPS*NCH)/1000,SAMPLERATE/1000,NCH,1);
	mod.SAVSAInit(maxlatency,SAMPLERATE);
	mod.VSASetInfo(SAMPLERATE,NCH);
	mod.outMod->SetVolume(-666); 

	killDecodeThread=0;
	thread_handle = (HANDLE) 
		CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) DecodeThread,NULL,0,&thread_id);
	debugmessage("END FUNCTION: play()");
	return 0; 
}

void pause() { paused=1; mod.outMod->Pause(1); }
void unpause() { paused=0; mod.outMod->Pause(0); }
int ispaused() { return paused; }


void stop() { 
	debugmessage("FUNCTION: stop()");
	if (thread_handle != INVALID_HANDLE_VALUE)
	{
		killDecodeThread=1;
		if (WaitForSingleObject(thread_handle,10000) == WAIT_TIMEOUT)
		{
			MessageBox(mod.hMainWindow,"error asking thread to die!\n",
				"error killing decode thread",0);
			TerminateThread(thread_handle,0);
		}
		CloseHandle(thread_handle);
		thread_handle = INVALID_HANDLE_VALUE;
	}

	mod.outMod->Close();

	mod.SAVSADeInit();
	

	gme_delete(emu);
	emu=NULL;
	debugmessage("END FUNCTION: stop()");
	return;
}


int getlength() {
	debugmessage("FUNCTION: getlength()");
	debugmessage("END FUNCTION: getlength()");
	return track_length;
}


int getoutputtime() { 
	debugmessage("FUNCTION: getoutputtime()");
	debugmessage("END FUNCTION: getoutputtime()");
	return gme_tell(emu); 
}

void setoutputtime(int time_in_ms) { 
	debugmessage("FUNCTION: setoutputtime()");
	gme_seek(emu, time_in_ms);
	debugmessage("END FUNCTION: setoutputtime()");
	return;
}


// standard volume/pan functions
void setvolume(int volume) { mod.outMod->SetVolume(volume); }

void setpan(int pan) { mod.outMod->SetPan(pan); }


int infoDlg(const char *fn, HWND hwnd)
{
	// CHANGEME! Write your own info dialog code here
	return 0;
}


void getfileinfo(const char *filename, char *title, int *length_in_ms)
{
	debugmessage("FUNCTION: getfileinfo()");
	if (!filename || !*filename)  // currently playing file
	{
		if (length_in_ms) *length_in_ms=getlength();
		if (title) 
		{
			const char* game = track_info->game;
			if ( !*game ) {
				sprintf(title, "%d/%d %s", track+1, track_count, track_info->song);
			} else {
				sprintf(title, "%s: %d/%d %s", game, track+1, track_count, track_info->song);
			}
		}
	}
	else // some other file
	{
		if (length_in_ms)
		{
			*length_in_ms=-1000; // the default is unknown file length (-1000), and we can't return the length of a non-opened file yet
		}
		if (title) // get non path portion of filename
		{
			const char *p=filename+strlen(filename);
			while (*p != '\\' && p >= filename) p--;
			strcpy(title,++p);
		}
	}
	debugmessage("END FUNCTION: getfileinfo()");
}


void eq_set(int on, char data[10], int preamp) 
{ 	
	// TODO: See if GME's equalization and Winamp's are compatible. 
        //       For now, do nothing.
	return;
}

int get_576_samples(char *buf)
{
	if (gme_track_ended( emu )) {
		return 0; // Song is over.
	}
	short temp_buf[1152];
	int buf_position = 0;
	gme_play( emu, 1152, temp_buf );
	int i;
	for (i=0; i < 1152; i++) {
		buf[buf_position] = temp_buf[i] & 0xff;
		buf_position++;
		buf[buf_position] = (temp_buf[i] >> 8) & 0xff;
		buf_position++;
	}
	char str[10];
	sprintf(str, "%d", buf_position);
	return buf_position;
}

DWORD WINAPI DecodeThread(LPVOID b)
{
	int done=0;
	while (!killDecodeThread) 
	{
		if (done)
		{
			mod.outMod->CanWrite();
			if (!mod.outMod->IsPlaying()) 
			{
				PostMessage(mod.hMainWindow,WM_WA_MPEG_EOF,0,0);
				return 0;
			}
			Sleep(10);
		}
		else if (mod.outMod->CanWrite() >= ((576*NCH*(BPS/8))*(mod.dsp_isactive()?2:1)))
		{	
			int l=576*NCH*(BPS/8);
			static char sample_buffer[576*NCH*(BPS/8)*2]; 
			l=get_576_samples(sample_buffer);
			if (!l)
			{
				done=1;
			}
			else
			{
				mod.SAAddPCMData((char *)sample_buffer,NCH,BPS,decode_pos_ms);	
				mod.VSAAddPCMData((char *)sample_buffer,NCH,BPS,decode_pos_ms);
				decode_pos_ms+=(576*1000)/SAMPLERATE;	

				if (mod.dsp_isactive()) 
					l=mod.dsp_dosamples(
						(short *)sample_buffer,l/NCH/(BPS/8),BPS,NCH,SAMPLERATE
					  )
					  *(NCH*(BPS/8));

				mod.outMod->Write(sample_buffer,l);
			}
		}
		else Sleep(20); 
	}
	return 0;
}



__declspec( dllexport ) In_Module * winampGetInModule2()
{
	return &mod;
}
