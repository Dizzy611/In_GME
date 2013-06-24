rm -f in_gme.dll *.o ./resources/*.o
gcc -I./libgme/include/gme/ -I./resources/ -I/c/Program\ Files\ \(x86\)/Winamp\ SDK/Winamp/ -Wp,-w -c MAIN_gme.c
windres -i ./resources/resource.rc -o ./resources/resource.o
g++ -mdll -static -static-libgcc -static-libstdc++ -L./libgme/lib/ MAIN_gme.o ./resources/resource.o -lgme -o in_gme.dll
rm -f *.o ./resources/*.o
