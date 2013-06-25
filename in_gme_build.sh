rm -f in_gme.dll *.o ./resources/*.o
gcc -I./zlib/include/ -c ./vgz/minigzip.c -o ./vgz/minigzip.o
gcc -I./libgme/include/gme/ -I./resources/ -I./vgz/ -I/c/Program\ Files\ \(x86\)/Winamp\ SDK/Winamp/ -c MAIN_gme.c
windres -i ./resources/resource.rc -o ./resources/resource.o
g++ -mdll -static -static-libgcc -static-libstdc++ -L./zlib/lib/ -L./libgme/lib/ ./vgz/minigzip.o MAIN_gme.o ./resources/resource.o -lgme -lz -o in_gme.dll
rm -f *.o ./resources/*.o
