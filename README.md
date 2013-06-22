In_GME
======

Winamp Plugin for Game-Music-Emu

Current Version: v0.02 Pre-Alpha

In_GME copyright (C) 2013 Dylan J. Morrison, released under the ISC license, details of license in MAIN_gme.c

Game_Music_Emu library copyright (C) 2003-2009 Shay Green, released under the 
LGPL, license available in libgme directory as license.txt, source code (unmodified from the original) available at https://code.google.com/p/game-music-emu/.

Sega Genesis YM2612 emulator copyright (C) 2002 Stephane Dallongeville, released under 
the LGPL.

Changelog:

 - Changes from v0.01 to v0.02
   - Multi-Track support implemented! No control currently available, but it will play
     your multi-track files track-by-track. Tested with Mega Man 2! It works great and 
     I'm so happy to even get this level of support done.

Features not yet implemented:

 - Seeking:
   - Should actually be possible with current code, but I'm not willing
     to turn it on yet as I want to make sure its bug-free without being
     able to seek first, as I'm not sure what the delay in seek from GME
     will do to Winamp

 - Controllable Multi-Track:
   - That is, multiple tracks in one file, like NSF. Currently it'll 
     play all the tracks from these files, one after another, while updating the title,
     but there's no way to go back and forward tracks. I either need to hook directly into
     Winamp's next/previous buttons to do this or provide my own in a seperate dialog
     that opens up when you play a multi-track file. The latter sounds cleaner and easier
     to implement, if not quite as convenient. It is also how the only open source NSF 
     player whose code I could understand, NotSo Fatso, does this.

 - Configuration:
   - Basically only to configure accuracy and stereo depth, and maybe 
     title format. Samplerate, bits per sample, and all that stuff are 
     pretty much set in stone by GME afaict (especially bits per sample).

 - Info:
   - Need to see what extra information besides what I expose in the 
     title is available in GME's track_info dealie.


Known Bugs:

 - Visualization:
   - Occasionally winamp's visualizer will give up. This only started 
     happening when I, in preparation for enabling seek, tied the 
     current position to gme_tell(), so this being out of sync by a 
     couple samples with winamp's output module is probably the culprit.

