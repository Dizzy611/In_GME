rm in_gme_debug.dll *.o
gcc -DDEBUG -I./libgme/include/gme/ -I./resources/ -I/c/Program\ Files\ \(x86\)/Winamp\ SDK/Winamp/ -g -Wp,-w -c MAIN_gme.c
g++ -mdll -static -static-libgcc -static-libstdc++ -L./libgme/lib/ MAIN_gme.o -lgme -g -o in_gme_debug.dll

