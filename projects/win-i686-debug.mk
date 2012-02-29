#
#   build.mk -- Build It Makefile to build Http Library for win on i686
#

CC        := cl
CFLAGS    := -nologo -GR- -W3 -Zi -Od -MDd
DFLAGS    := -D_REENTRANT -D_MT
IFLAGS    := -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc
LDFLAGS   := -nologo -nodefaultlib -incremental:no -libpath:/Users/mob/git/http/win-i686-debug/bin -debug -machine:x86
LIBS      := ws2_32.lib advapi32.lib user32.lib kernel32.lib oldnames.lib msvcrt.lib

export PATH := %VS%/Bin:%VS%/VC/Bin:%VS%/Common7/IDE:%VS%/Common7/Tools:%VS%/SDK/v3.5/bin:%VS%/VC/VCPackages
export INCLUDE := %VS%/INCLUDE:%VS%/VC/INCLUDE
export LIB := %VS%/lib:%VS%/VC/lib
all: \
        win-i686-debug/bin/libmpr.dll \
        win-i686-debug/bin/manager.exe \
        win-i686-debug/bin/makerom.exe \
        win-i686-debug/bin/libpcre.dll \
        win-i686-debug/bin/libhttp.dll \
        win-i686-debug/bin/http.exe

clean:
	rm -f win-i686-debug/bin/libmpr.dll
	rm -f win-i686-debug/bin/libmprssl.dll
	rm -f win-i686-debug/bin/manager.exe
	rm -f win-i686-debug/bin/makerom.exe
	rm -f win-i686-debug/bin/libpcre.dll
	rm -f win-i686-debug/bin/libhttp.dll
	rm -f win-i686-debug/bin/http.exe
	rm -f win-i686-debug/obj/mprLib.obj
	rm -f win-i686-debug/obj/mprSsl.obj
	rm -f win-i686-debug/obj/manager.obj
	rm -f win-i686-debug/obj/makerom.obj
	rm -f win-i686-debug/obj/pcre.obj
	rm -f win-i686-debug/obj/auth.obj
	rm -f win-i686-debug/obj/authCheck.obj
	rm -f win-i686-debug/obj/authFile.obj
	rm -f win-i686-debug/obj/authPam.obj
	rm -f win-i686-debug/obj/cache.obj
	rm -f win-i686-debug/obj/chunkFilter.obj
	rm -f win-i686-debug/obj/$(CC)ient.obj
	rm -f win-i686-debug/obj/conn.obj
	rm -f win-i686-debug/obj/endpoint.obj
	rm -f win-i686-debug/obj/error.obj
	rm -f win-i686-debug/obj/host.obj
	rm -f win-i686-debug/obj/httpService.obj
	rm -f win-i686-debug/obj/log.obj
	rm -f win-i686-debug/obj/netConnector.obj
	rm -f win-i686-debug/obj/packet.obj
	rm -f win-i686-debug/obj/passHandler.obj
	rm -f win-i686-debug/obj/pipeline.obj
	rm -f win-i686-debug/obj/queue.obj
	rm -f win-i686-debug/obj/rangeFilter.obj
	rm -f win-i686-debug/obj/route.obj
	rm -f win-i686-debug/obj/rx.obj
	rm -f win-i686-debug/obj/sendConnector.obj
	rm -f win-i686-debug/obj/stage.obj
	rm -f win-i686-debug/obj/trace.obj
	rm -f win-i686-debug/obj/tx.obj
	rm -f win-i686-debug/obj/uploadFilter.obj
	rm -f win-i686-debug/obj/uri.obj
	rm -f win-i686-debug/obj/var.obj
	rm -f win-i686-debug/obj/http.obj

win-i686-debug/obj/mprLib.obj: \
        src/deps/mpr/mprLib.c \
        win-i686-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	"$(CC)" -c -Fowin-i686-debug/obj/mprLib.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/deps/mpr/mprLib.c

win-i686-debug/bin/libmpr.dll:  \
        win-i686-debug/obj/mprLib.obj
	"link" -dll -out:win-i686-debug/bin/libmpr.dll -entry:_DllMainCRTStartup@12 -def:win-i686-debug/bin/libmpr.def $(LDFLAGS) win-i686-debug/obj/mprLib.obj $(LIBS)

win-i686-debug/obj/manager.obj: \
        src/deps/mpr/manager.c \
        win-i686-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	"$(CC)" -c -Fowin-i686-debug/obj/manager.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/deps/mpr/manager.c

win-i686-debug/bin/manager.exe:  \
        win-i686-debug/bin/libmpr.dll \
        win-i686-debug/obj/manager.obj
	"link" -out:win-i686-debug/bin/manager.exe -entry:WinMainCRTStartup -subsystem:Windows $(LDFLAGS) win-i686-debug/obj/manager.obj $(LIBS) mpr.lib shell32.lib

win-i686-debug/obj/makerom.obj: \
        src/deps/mpr/makerom.c \
        win-i686-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	"$(CC)" -c -Fowin-i686-debug/obj/makerom.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/deps/mpr/makerom.c

win-i686-debug/bin/makerom.exe:  \
        win-i686-debug/bin/libmpr.dll \
        win-i686-debug/obj/makerom.obj
	"link" -out:win-i686-debug/bin/makerom.exe -entry:mainCRTStartup -subsystem:console $(LDFLAGS) win-i686-debug/obj/makerom.obj $(LIBS) mpr.lib

win-i686-debug/obj/pcre.obj: \
        src/deps/pcre/pcre.c \
        win-i686-debug/inc/bit.h \
        win-i686-debug/inc/buildConfig.h \
        src/deps/pcre/pcre.h
	"$(CC)" -c -Fowin-i686-debug/obj/pcre.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/deps/pcre/pcre.c

win-i686-debug/bin/libpcre.dll:  \
        win-i686-debug/obj/pcre.obj
	"link" -dll -out:win-i686-debug/bin/libpcre.dll -entry:_DllMainCRTStartup@12 -def:win-i686-debug/bin/libpcre.def $(LDFLAGS) win-i686-debug/obj/pcre.obj $(LIBS)

win-i686-debug/obj/auth.obj: \
        src/auth.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/auth.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/auth.c

win-i686-debug/obj/authCheck.obj: \
        src/authCheck.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/authCheck.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/authCheck.c

win-i686-debug/obj/authFile.obj: \
        src/authFile.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/authFile.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/authFile.c

win-i686-debug/obj/authPam.obj: \
        src/authPam.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/authPam.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/authPam.c

win-i686-debug/obj/cache.obj: \
        src/cache.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/cache.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/cache.c

win-i686-debug/obj/chunkFilter.obj: \
        src/chunkFilter.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/chunkFilter.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/chunkFilter.c

win-i686-debug/obj/client.obj: \
        src/client.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/client.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/client.c

win-i686-debug/obj/conn.obj: \
        src/conn.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/conn.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/conn.c

win-i686-debug/obj/endpoint.obj: \
        src/endpoint.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/endpoint.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/endpoint.c

win-i686-debug/obj/error.obj: \
        src/error.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/error.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/error.c

win-i686-debug/obj/host.obj: \
        src/host.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/host.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/host.c

win-i686-debug/obj/httpService.obj: \
        src/httpService.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/httpService.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/httpService.c

win-i686-debug/obj/log.obj: \
        src/log.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/log.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/log.c

win-i686-debug/obj/netConnector.obj: \
        src/netConnector.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/netConnector.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/netConnector.c

win-i686-debug/obj/packet.obj: \
        src/packet.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/packet.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/packet.c

win-i686-debug/obj/passHandler.obj: \
        src/passHandler.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/passHandler.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/passHandler.c

win-i686-debug/obj/pipeline.obj: \
        src/pipeline.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/pipeline.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/pipeline.c

win-i686-debug/obj/queue.obj: \
        src/queue.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/queue.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/queue.c

win-i686-debug/obj/rangeFilter.obj: \
        src/rangeFilter.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/rangeFilter.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/rangeFilter.c

win-i686-debug/obj/route.obj: \
        src/route.c \
        win-i686-debug/inc/bit.h \
        src/http.h \
        src/deps/pcre/pcre.h
	"$(CC)" -c -Fowin-i686-debug/obj/route.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/route.c

win-i686-debug/obj/rx.obj: \
        src/rx.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/rx.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/rx.c

win-i686-debug/obj/sendConnector.obj: \
        src/sendConnector.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/sendConnector.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/sendConnector.c

win-i686-debug/obj/stage.obj: \
        src/stage.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/stage.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/stage.c

win-i686-debug/obj/trace.obj: \
        src/trace.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/trace.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/trace.c

win-i686-debug/obj/tx.obj: \
        src/tx.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/tx.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/tx.c

win-i686-debug/obj/uploadFilter.obj: \
        src/uploadFilter.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/uploadFilter.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/uploadFilter.c

win-i686-debug/obj/uri.obj: \
        src/uri.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/uri.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/uri.c

win-i686-debug/obj/var.obj: \
        src/var.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/var.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/var.c

win-i686-debug/bin/libhttp.dll:  \
        win-i686-debug/bin/libmpr.dll \
        win-i686-debug/bin/libpcre.dll \
        win-i686-debug/obj/auth.obj \
        win-i686-debug/obj/authCheck.obj \
        win-i686-debug/obj/authFile.obj \
        win-i686-debug/obj/authPam.obj \
        win-i686-debug/obj/cache.obj \
        win-i686-debug/obj/chunkFilter.obj \
        win-i686-debug/obj/client.obj \
        win-i686-debug/obj/conn.obj \
        win-i686-debug/obj/endpoint.obj \
        win-i686-debug/obj/error.obj \
        win-i686-debug/obj/host.obj \
        win-i686-debug/obj/httpService.obj \
        win-i686-debug/obj/log.obj \
        win-i686-debug/obj/netConnector.obj \
        win-i686-debug/obj/packet.obj \
        win-i686-debug/obj/passHandler.obj \
        win-i686-debug/obj/pipeline.obj \
        win-i686-debug/obj/queue.obj \
        win-i686-debug/obj/rangeFilter.obj \
        win-i686-debug/obj/route.obj \
        win-i686-debug/obj/rx.obj \
        win-i686-debug/obj/sendConnector.obj \
        win-i686-debug/obj/stage.obj \
        win-i686-debug/obj/trace.obj \
        win-i686-debug/obj/tx.obj \
        win-i686-debug/obj/uploadFilter.obj \
        win-i686-debug/obj/uri.obj \
        win-i686-debug/obj/var.obj
	"link" -dll -out:win-i686-debug/bin/libhttp.dll -entry:_DllMainCRTStartup@12 -def:win-i686-debug/bin/libhttp.def $(LDFLAGS) win-i686-debug/obj/auth.obj win-i686-debug/obj/authCheck.obj win-i686-debug/obj/authFile.obj win-i686-debug/obj/authPam.obj win-i686-debug/obj/cache.obj win-i686-debug/obj/chunkFilter.obj win-i686-debug/obj/$(CC)ient.obj win-i686-debug/obj/conn.obj win-i686-debug/obj/endpoint.obj win-i686-debug/obj/error.obj win-i686-debug/obj/host.obj win-i686-debug/obj/httpService.obj win-i686-debug/obj/log.obj win-i686-debug/obj/netConnector.obj win-i686-debug/obj/packet.obj win-i686-debug/obj/passHandler.obj win-i686-debug/obj/pipeline.obj win-i686-debug/obj/queue.obj win-i686-debug/obj/rangeFilter.obj win-i686-debug/obj/route.obj win-i686-debug/obj/rx.obj win-i686-debug/obj/sendConnector.obj win-i686-debug/obj/stage.obj win-i686-debug/obj/trace.obj win-i686-debug/obj/tx.obj win-i686-debug/obj/uploadFilter.obj win-i686-debug/obj/uri.obj win-i686-debug/obj/var.obj $(LIBS) mpr.lib pcre.lib

win-i686-debug/obj/http.obj: \
        src/utils/http.c \
        win-i686-debug/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fowin-i686-debug/obj/http.obj -Fdwin-i686-debug/obj $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/utils/http.c

win-i686-debug/bin/http.exe:  \
        win-i686-debug/bin/libhttp.dll \
        win-i686-debug/obj/http.obj
	"link" -out:win-i686-debug/bin/http.exe -entry:mainCRTStartup -subsystem:console $(LDFLAGS) win-i686-debug/obj/http.obj $(LIBS) http.lib mpr.lib pcre.lib

