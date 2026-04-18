ifndef MSYSTEM
	CPREFIX = x86_64-w64-mingw32-
endif

WINDIVERTHEADERS = ../WinDivert-2.2.0-D/include
WINDIVERTLIBS = ../WinDivert-2.2.0-D/x64
MINGWLIB = /usr/x86_64-w64-mingw32/lib/

TARGET = hayalet.exe
LIBS = -L$(WINDIVERTLIBS) -lWinDivert -lws2_32 -liphlpapi -ldwmapi -lwininet -lcomctl32 -lwinhttp -lole32 -lshlwapi -lversion -ladvapi32 -luser32 -loleaut32 -luuid -lshell32 -l:libssp.a
CC = $(CPREFIX)gcc
CXX = $(CPREFIX)g++
CCWINDRES = windres
# If the above fails, you can fallback to 'windres' manually if needed
CXXFLAGS = -std=c++17 -DWEBVIEW_EDGE -DWIN32_LEAN_AND_MEAN -Iwebview2/build/native/include -O2

CFLAGS = -std=c99 -pie -fPIE -pipe -I$(WINDIVERTHEADERS) -L$(WINDIVERTLIBS) \
         -O2 -D_FORTIFY_SOURCE=2 -fstack-protector \
         -Wall -Wextra -Wpedantic -Wformat=2 -Wformat-overflow=2 -Wformat-truncation=2 \
         -Wformat-security -Wno-format-nonliteral -Wshadow -Wstrict-aliasing=1 \
         -Wnull-dereference -Warray-bounds=2 -Wimplicit-fallthrough=3 \
         -Wstringop-overflow=4 \
         -Wformat-signedness -Wstrict-overflow=2 -Wcast-align=strict \
         -Wfloat-equal -Wcast-align -Wsign-conversion \
         #-fstack-protector-strong
LDFLAGS = -static -static-libgcc -static-libstdc++ -fstack-protector -Wl,-O1,-pie,--dynamicbase,--nxcompat,--sort-common,--as-needed \
-Wl,--disable-auto-image-base -mwindows

ifdef BIT64
	LDFLAGS += -Wl,--high-entropy-va -Wl,--pic-executable,-e,WinMainCRTStartup
else
	CFLAGS += -m32
	LDFLAGS += -Wl,--pic-executable,-e,_WinMainCRTStartup -m32
endif

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c utils/*.c)) hayalet-rc.o webview_v2.o
HEADERS = $(wildcard *.h utils/*.h) version.h

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

hayalet-rc.o: hayalet-rc.rc resource.h
	$(CCWINDRES) hayalet-rc.rc hayalet-rc.o

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): version.h $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) $(LIBS) -s -o $@

version.h: ../VERSION
	@echo [SYSTEM] Syncing version.h from ../VERSION...
	@powershell -Command "$$v = (Get-Content ../VERSION).Trim(); \"#define HAYALET_VERSION `\"$$v`\"`n#define HAYALET_FULL_TITLE `\"HayaletDPI v$$v Platinum Edition`\"`n#define HAYALET_SUBTITLE `\"Developed by Gokhan Ozen`\"\" | Out-File -Encoding ascii version.h"

clean:
	-rm -f *.o utils/*.o
	-rm -f $(TARGET)
