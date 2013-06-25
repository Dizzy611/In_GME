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
#include <string.h>

#include "in2.h"
#include "wa_ipc.h"
#include "resource.h"
#include "gme.h"
#include "minigzip.h"

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
volatile int seek_needed;
int track;                        // Track within file.
int track_count;                  // Number of tracks within file
long track_length;			  // Length of current track in ms
gme_info_t* track_info;           // GME info on track.

HANDLE input_file=INVALID_HANDLE_VALUE; // input file handle

volatile int killDecodeThread=0;			// the kill switch for the decode thread
HANDLE thread_handle=INVALID_HANDLE_VALUE;	// the handle to the decode thread

Music_Emu* emu;
BOOL accurate;
BOOL decompressing;
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
	"Game_Music_Emu Winamp Plugin v0.10.3 DEBUG"
#else
	"Game_Music_Emu Winamp Plugin v0.10.3"
#endif
	,
	0,	// hMainWindow (filled in by winamp)
	0,  // hDllInstance (filled in by winamp)
	"SPC\0SNES SPC700 Sound File (*.SPC)\0VGM\0Sega Genesis/MD/SMS/BBC Micro VGM File (*.VGM)\0VGZ\0Zipped VGM (*.VGZ)\0AY\0ZX Spectrum/Amstrad CPC Sound File (*.AY)\0GBS\0Game Boy Sound File (*.GBS)\0HES\0TG-16/PC-Engine Sound File (*.HES)\0KSS\0MSX Sound File (*.KSS)\0NSF\0NES Sound File (*.NSF)\0SAP\0Atari Sound File (*.SAP)\0GYM\0Sega Genesis Sound File (*.GYM)\0NSFE\0Extended NSF (*.NSFE)"
	// this is a double-null limited list. "EXT\0Description\0EXT\0Description\0" etc.
	,
	1,	// is_seekable
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


INT_PTR CALLBACK AboutDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_COMMAND:
    {
      switch (LOWORD(wParam))
      {
        case IDOK:
        case IDCANCEL:
        {
          EndDialog(hwndDlg, (INT_PTR) LOWORD(wParam));
          return (INT_PTR) TRUE;
        }
      }
      break;
    }

    case WM_INITDIALOG:
      return (INT_PTR) TRUE;
  }

  return (INT_PTR) FALSE;
}

void config(HWND hwndParent)
{
	MessageBox(hwndParent,
		"No configuration yet! Defaults to 44100 rate, 16-bit, Stereo, using Accurate Emulation where available.",
		"Configuration",MB_OK);
	// if we had a configuration box we'd want to write it here (using DialogBox, etc)
}
void about(HWND hwndParent)
{
	DialogBox(mod.hDllInstance, MAKEINTRESOURCE(IDD_ABOUTDIALOG), hwndParent, &AboutDialogProc);
	return;
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
	fp = fopen(filename, "a");
	debugmessage("LOG START");
#endif
	emu = NULL;
	accurate = TRUE;
}

void debugmessage(char *message) {
#ifdef DEBUG
	fprintf(fp, "%s\n", message);
	fflush(fp);
#endif
	return;
}

void quit() { 
	emu = NULL;
#ifdef DEBUG
	debugmessage("END LOG");
	fclose(fp);
#endif
	return;
}

int isourfile(const char *fn) { 
	debugmessage("DUMMIED FUNCTION: isourfile()");
	return 0; 
} 

int play(const char *fn) 
{ 
	debugmessage("FUNCTION: play()");
	int maxlatency;
	int thread_id;
	decompressing=0;
	// Grab a filehandle just to get the filesize. There's gotta be a better way to do this...
	input_file = CreateFile(fn,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,
		OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if (input_file == INVALID_HANDLE_VALUE)
	{
		return 1;
	}

	file_length=GetFileSize(input_file,NULL);
	
	CloseHandle(input_file);		
	
	// Check if its a vgz
	char *ext;
	ext = strrchr(fn, '.');
	ext += 1;
	if ( strcmp(ext, "vgz") == 0 ) {
		debugmessage("Found a VGZ file!");
		char dest[MAX_PATH];
		ExpandEnvironmentStrings("%temp%\\temp.vgm", dest, MAX_PATH);
		simpleDecompress(fn, dest);
		strcpy(fn, dest);
		decompressing=1;
	}
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
	track_length = track_length + 8000; // Fade takes 8 seconds to process, increasing track_length by 8000 makes winamp line up.

	
	paused=0;
	decode_pos_ms=0;
	seek_needed=-1;

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

void track_change() {
	pause();
	gme_start_track( emu, track );
	gme_free_info( track_info );
	track_info = NULL;
	gme_track_info( emu, &track_info, track );
	track_length = track_info->length;
	if ( track_length <= 0 ) {
		track_length = track_info->intro_length + track_info->loop_length * 2;
	}	
	if ( track_length <= 0 )
		track_length = (long) (2.5 * 60 * 1000);
	gme_set_fade(emu, track_length);
	track_length = track_length + 8000; // see play()
	decode_pos_ms = 0;
	seek_needed = -1;
	PostMessage(mod.hMainWindow,WM_WA_IPC,0,IPC_UPDTITLE);
	Sleep(10);
	unpause();
}

int next_track() {
	track++;
	if ( track >= track_count ) {
			track--;
			return 0;
	} else {
		track_change();
		return 1;
	}
}

int prev_track() {
	track--;
	if ( track < 0 ) {
		track++;
		return 0;
	} else {
		track_change();
		return 1;
	}
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
	if (decompressing) {
		char tempfile[MAX_PATH];
		ExpandEnvironmentStrings("%temp%\\temp.vgm", tempfile, MAX_PATH);
		remove(tempfile);
	}
	emu=NULL;
	debugmessage("END FUNCTION: stop()");
	return;
}


int getlength() {
	return track_length;
}

	
int getoutputtime() { 
	return decode_pos_ms+
		(mod.outMod->GetOutputTime()-mod.outMod->GetWrittenTime()); 
}

void setoutputtime(int time_in_ms) {
	seek_needed=time_in_ms;
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
				if (track_count > 1) {
					sprintf(title, "%d/%d %s", track+1, track_count, track_info->song);
				} else {
					sprintf(title, "%s", track_info->song);
				}
			} else {
				if (track_count > 1) {
					sprintf(title, "%s: %d/%d %s", game, track+1, track_count, track_info->song);
				} else {
					sprintf(title, "%s - %s", game, track_info->song);
				}
			}
		}
	}
	else // some other file
	{
		Music_Emu* temp_emu; // create a temporary emu instance
		gme_info_t* temp_track_info; // create a temporary track_info instance
		long temp_track_length;
		temp_emu=NULL;
		temp_track_info=NULL; // if we screw up we want this to show as a null pointer dereference so that we know exactly where and how
		char *ext;
		BOOL temp_decompressing=0;
		ext = strrchr(filename, '.');
		ext += 1;
		if ( strcmp(ext, "vgz") == 0 ) {
			debugmessage("Found a VGZ file!");
			char dest[MAX_PATH];
			ExpandEnvironmentStrings("%temp%\\temp-info.vgm", dest, MAX_PATH);
			simpleDecompress(filename, dest);
			strcpy(filename, dest);
			temp_decompressing=1;
		}
		gme_open_file(filename,&temp_emu,SAMPLERATE);
		gme_track_info(temp_emu,&temp_track_info,0);
		temp_track_length = temp_track_info->length;
		if ( temp_track_length <= 0 ) {
			temp_track_length = temp_track_info->intro_length + temp_track_info->loop_length * 2;
		}	
		if ( temp_track_length <= 0 )
			temp_track_length = (long) (2.5 * 60 * 1000);
		temp_track_length = temp_track_length + 8000;
		if (length_in_ms)
		{
			*length_in_ms=temp_track_length; 
		}
		if (title) 
		{
			const char* game = temp_track_info->game;
			if ( !*game ) {
				if (gme_track_count(temp_emu) > 1) {
					sprintf(title, "%d/%d %s", 1, gme_track_count(temp_emu), temp_track_info->song);
				} else {
					sprintf(title, "%s", temp_track_info->song);
				}
			} else {
				if (gme_track_count(temp_emu) > 1) {
					sprintf(title, "%s: %d/%d %s", game, 1, gme_track_count(temp_emu), temp_track_info->song);
				} else {
					sprintf(title, "%s - %s", game, temp_track_info->song);
				}
			}
		}
		gme_free_info(temp_track_info);
		gme_delete(temp_emu);
		if ( temp_decompressing == 1 ) {
			remove(filename);
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
		debugmessage("Track ends.");
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
	return buf_position;
}

DWORD WINAPI DecodeThread(LPVOID b)
{
	int done=0;
	while (!killDecodeThread) 
	{
		if (seek_needed != -1) {
			done=0;
			decode_pos_ms = seek_needed;
			seek_needed=-1;
			mod.outMod->Flush(decode_pos_ms);
			pause();
			gme_seek(emu, decode_pos_ms);
			// Fix for possible upstream bug where, after seeking during a fadeout, the fadeout is unset and the track loops indefinitely.
			// We just re-set the fade after any seek. May be an upstream bug, but may be intended behavior.
			gme_set_fade(emu, track_length-8000);
			decode_pos_ms = gme_tell(emu);
			unpause();
		}
		if (done)
		{
			mod.outMod->CanWrite();
			if (!mod.outMod->IsPlaying()) 
			{
				if (track+1 == track_count) {
					PostMessage(mod.hMainWindow,WM_WA_MPEG_EOF,0,0);
					return 0;
				} else { 
					next_track();
					done=0;
				}
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
