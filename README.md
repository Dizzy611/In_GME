In_GME
======

Winamp Plugin for Game-Music-Emu

Current Version: v0.01 Pre-Alpha

Currently in a pre-alpha state but usable!

Features not yet implemented:

 - Seeking:
   - Should actually be possible with current code, but I'm not willing
     to turn it on yet as I want to make sure its bug-free without being
     able to seek first, as I'm not sure what the delay in seek from GME
     will do to Winamp

 - Multi-Track:
   - That is, multiple tracks in one file, like NSF. Currently it'll 
     only play the first track from these files, I need to figure out 
     how to handle this, as I can't really see a way to do so in the general 
     winamp Input Plugin framework. Basically, I need to see how in_nsf does 
     this

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

