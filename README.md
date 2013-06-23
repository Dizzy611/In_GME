In_GME
======

Installer now available for public testing! Please be aware this is still very much an early alpha. 
If that's okay with you, Click "In_GME_v0_06.exe" then "Raw" to download.

Winamp Plugin for Game-Music-Emu

Current Version: v0.06 Pre-Alpha

In_GME copyright (C) 2013 Dylan J. Morrison, released under the ISC license, details of
license in MAIN_gme.c

Game_Music_Emu library copyright (C) 2003-2009 Shay Green, released under the 
LGPL, license available in libgme directory as license.txt, source code (unmodified 
from the original) available at https://code.google.com/p/game-music-emu/.

Sega Genesis YM2612 emulator copyright (C) 2002 Stephane Dallongeville, released under 
the LGPL.

Changelog:
 - Changes from v0.05 to v0.06
   - Basic track information (title, length, number of tracks, etc) will now show for all in_gme
     tracks in the list, and will not disappear when you try to use the (currently non-existant)
     info dialog. I managed to get this working by creating a temporary emu instance and freeing it
     at the end of the info grab.
 - Changes from v0.04 to v0.05
   - Fixed visualizations by re-working current position/seeking to better fit with the way
     winamp expects them to work. Seeking should also be faster and cleaner now. Also, removed
     some useless debug messages and added some more useful ones, and some general code cleanup.
 - Changes from v0.03 to v0.04
   - Worked around a potential upstream bug regarding seeking: If you seek backwards during a
     fade-out, the fade-out is unset, causing the track to then loop indefinitely. I thus
     re-set the fade-out during every seek. I can see this potentially being intended behavior, 
     so I'm debating whether to report it upstream.
 - Changes from v0.02 to v0.03
   - Seeking implemented! It can take a few seconds for the audio to actually change, but
     as a bonus, seeking to 100% on a multi-track file functions as a skip-to-next, which
     is helpful while full controllable multi-track is still not here.
 - Changes from v0.01 to v0.02
   - Multi-Track support implemented! No control currently available, but it will play
     your multi-track files track-by-track. Tested with Mega Man 2! It works great and 
     I'm so happy to even get this level of support done.

Features not yet implemented:

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

 - None at the moment!