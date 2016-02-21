# makefile

WHERE_ZLIB?=./zlib-1.2.8-inflate
WHERE_ZOPFLI?=./zopfli/src/zopfli

# Ws2_32.dll; byte order functions
ifeq ($(OS),Windows_NT)
ADD_WINSOCK := -lws2_32
else
ADD_WINSOCK := 
endif

all: zlib zopfli
	mkdir -p bin/gcc
	g++ -o bin/gcc/HitotsuNoHikari -I$(WHERE_ZLIB) -I$(WHERE_ZOPFLI) -pie -fPIE $(wildcard *.o) src/HitotsuNoHikari.cpp $(ADD_WINSOCK)
	-rm *.o

zlib:
	gcc -c -O3 $(wildcard $(WHERE_ZLIB)/*.c)

zopfli:
	gcc -c -O3 $(wildcard $(WHERE_ZOPFLI)/*.c)

vscmd:
	@echo Locating cl.exe
	@where cl.exe
	@echo Locating link.exe
	@where link.exe
	@echo Locating rc.exe
	@where rc.exe
	-mkdir -p bin/vscmd
	cl -W3 -Zc:wchar_t -Ox -DWIN32 -D_CONSOLE -EHsc -MT -I$(WHERE_ZLIB) -I$(WHERE_ZOPFLI) -wd"4996" -c src/HitotsuNoHikari.cpp $(wildcard $(WHERE_ZLIB)/*.c) $(wildcard $(WHERE_ZOPFLI)/*.c)
	rc -v -l 0 Info.rc
	link -OUT:"bin\\vscmd\\HitotsuNoHikari.exe" -MANIFEST -NXCOMPAT -PDB:"bin\\vscmd\\HitotsuNoHikari.pdb" -DEBUG -RELEASE -SUBSYSTEM:CONSOLE *.obj Info.res ws2_32.lib
	-rm *.obj Info.res

clean:
	-rm *.o
	-rm *.obj Info.res

.PHONY: all zlib zopfli vscmd clean
