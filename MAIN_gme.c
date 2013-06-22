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
	"Game_Music_Emu 0.6.0 Winamp Plugin "
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

void init() { 
	emu = NULL;
	accurate = TRUE;
}

void quit() { 
	emu = NULL;
}

int isourfile(const char *fn) { 
	return 0; 
} 

int play(const char *fn) 
{ 
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
	
	// And now we don't need the handle anymore.
	CloseHandle(input_file);		
	
	// TODO: Make sure this GME loading code is correct.
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

	// -1 and -1 are to specify buffer and prebuffer lengths.
	// -1 means to use the default, which all input plug-ins should
	// really do.
	maxlatency = mod.outMod->Open(SAMPLERATE,NCH,BPS, -1,-1); 

	// maxlatency is the maxium latency between a outMod->Write() call and
	// when you hear those samples. In ms. Used primarily by the visualization
	// system.

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

	// launch decode thread
	killDecodeThread=0;
	thread_handle = (HANDLE) 
		CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) DecodeThread,NULL,0,&thread_id);
	
	return 0; 
}

// standard pause implementation
void pause() { paused=1; mod.outMod->Pause(1); }
void unpause() { paused=0; mod.outMod->Pause(0); }
int ispaused() { return paused; }


// stop playing.
void stop() { 
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

	// close output system
	mod.outMod->Close();

	// deinitialize visualization
	mod.SAVSADeInit();
	

	// CHANGEME! Write your own file closing code here
	if (input_file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(input_file);
		input_file=INVALID_HANDLE_VALUE;
	}

}


// returns length of playing track
int getlength() {
	return track_length;
}


// returns current output position, in ms.
// you could just use return mod.outMod->GetOutputTime(),
// but the dsp plug-ins that do tempo changing tend to make
// that wrong.
int getoutputtime() { 
	return decode_pos_ms+
		(mod.outMod->GetOutputTime()-mod.outMod->GetWrittenTime()); 
}


// called when the user releases the seek scroll bar.
// usually we use it to set seek_needed to the seek
// point (seek_needed is -1 when no seek is needed)
// and the decode thread checks seek_needed.
void setoutputtime(int time_in_ms) { 
	return; // We can't handle seeking right now
}


// standard volume/pan functions
void setvolume(int volume) { mod.outMod->SetVolume(volume); }

void setpan(int pan) { mod.outMod->SetPan(pan); }

// this gets called when the use hits Alt+3 to get the file info.
// if you need more info, ask me :)

int infoDlg(const char *fn, HWND hwnd)
{
	// CHANGEME! Write your own info dialog code here
	return 0;
}


// this is an odd function. it is used to get the title and/or
// length of a track.
// if filename is either NULL or of length 0, it means you should
// return the info of lastfn. Otherwise, return the information
// for the file in filename.
// if title is NULL, no title is copied into it.
// if length_in_ms is NULL, no length is copied into it.
void getfileinfo(const char *filename, char *title, int *length_in_ms)
{
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
			*length_in_ms=-1000; // the default is unknown file length (-1000), and we can't return the length of a non-opened file yet.
		}
		if (title) // get non path portion of filename
		{
			const char *p=filename+strlen(filename);
			while (*p != '\\' && p >= filename) p--;
			strcpy(title,++p);
		}
	}
}


void eq_set(int on, char data[10], int preamp) 
{ 
	// most plug-ins can't even do an EQ anyhow.. I'm working on writing
	// a generic PCM EQ, but it looks like it'll be a little too CPU 
	// consuming to be useful :)
	// if you _CAN_ do EQ with your format, each data byte is 0-63 (+20db <-> -20db)
	// and preamp is the same. 
}


// render 576 samples into buf. 
// this function is only used by DecodeThread. 

// note that if you adjust the size of sample_buffer, for say, 1024
// sample blocks, it will still work, but some of the visualization 
// might not look as good as it could. Stick with 576 sample blocks
// if you can, and have an additional auxiliary (overflow) buffer if 
// necessary.. 
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
	int done=0; // set to TRUE if decoding has finished
	while (!killDecodeThread) 
	{
		if (done) // done was set to TRUE during decoding, signaling eof
		{
			mod.outMod->CanWrite();		// some output drivers need CanWrite
									    // to be called on a regular basis.

			if (!mod.outMod->IsPlaying()) 
			{
				// we're done playing, so tell Winamp and quit the thread.
				PostMessage(mod.hMainWindow,WM_WA_MPEG_EOF,0,0);
				return 0;	// quit thread
			}
			Sleep(10);		// give a little CPU time back to the system.
		}
		else if (mod.outMod->CanWrite() >= ((576*NCH*(BPS/8))*(mod.dsp_isactive()?2:1)))
			// CanWrite() returns the number of bytes you can write, so we check that
			// to the block size. the reason we multiply the block size by two if 
			// mod.dsp_isactive() is that DSP plug-ins can change it by up to a 
			// factor of two (for tempo adjustment).
		{	
			int l=576*NCH*(BPS/8);			       // block length in bytes
			static char sample_buffer[576*NCH*(BPS/8)*2]; 
												   // sample buffer. twice as 
												   // big as the blocksize

			l=get_576_samples(sample_buffer);	   // retrieve samples
			if (!l)			// no samples means we're at eof
			{
				done=1;
				gme_delete(emu);
				emu=NULL;
			}
			else	// we got samples!
			{
				// give the samples to the vis subsystems
				mod.SAAddPCMData((char *)sample_buffer,NCH,BPS,decode_pos_ms);	
				mod.VSAAddPCMData((char *)sample_buffer,NCH,BPS,decode_pos_ms);
				// adjust decode position variable
				decode_pos_ms+=(576*1000)/SAMPLERATE;	

				// if we have a DSP plug-in, then call it on our samples
				if (mod.dsp_isactive()) 
					l=mod.dsp_dosamples(
						(short *)sample_buffer,l/NCH/(BPS/8),BPS,NCH,SAMPLERATE
					  ) // dsp_dosamples
					  *(NCH*(BPS/8));

				// write the pcm data to the output system
				mod.outMod->Write(sample_buffer,l);
			}
		}
		else Sleep(20); 
		// if we can't write data, wait a little bit. Otherwise, continue 
		// through the loop writing more data (without sleeping)
	}
	return 0;
}



__declspec( dllexport ) In_Module * winampGetInModule2()
{
	return &mod;
}
