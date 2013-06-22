rm in_gme.dll *.o
gcc -I./libgme/include/gme/ -I/c/Program\ Files\ \(x86\)/Winamp\ SDK/Winamp/ -c MAIN_gme.c
g++ -mdll -static -static-libgcc -static-libstdc++ -L./libgme/lib/ MAIN_gme.o -lgme -o in_gme.dll

