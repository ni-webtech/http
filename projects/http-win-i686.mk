#
#   win-i686-debug.mk -- Build It Makefile to build Http Library for win on i686
#

VS             := $(VSINSTALLDIR)
VS             ?= \Users\mob\git\http\$(VS)
SDK            := $(WindowsSDKDir)
SDK            ?= $(SDK)

export         SDK VS
export PATH    := $(SDK)/Bin:$(VS)/VC/Bin:$(VS)/Common7/IDE:$(VS)/Common7/Tools:$(VS)/SDK/v3.5/bin:$(VS)/VC/VCPackages;$(PATH)
export INCLUDE := $(INCLUDE);$(SDK)/INCLUDE:$(VS)/VC/INCLUDE
export LIB     := $(LIB);$(SDK)/lib:$(VS)/VC/lib

OS       := win
CONFIG   := $(OS)-i686-debug
CC       := cl.exe
LD       := link.exe
CFLAGS   := -nologo -GR- -W3 -Zi -Od -MDd
DFLAGS   := -D_REENTRANT -D_MT
IFLAGS   := -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc
LDFLAGS  := '-nologo' '-nodefaultlib' '-incremental:no' '-debug' '-machine:x86'
LIBPATHS := -libpath:$(CONFIG)/bin
LIBS     := ws2_32.lib advapi32.lib user32.lib kernel32.lib oldnames.lib msvcrt.lib shell32.lib

all: prep \
        $(CONFIG)/bin/libmpr.dll \
        $(CONFIG)/bin/libmprssl.dll \
        $(CONFIG)/bin/makerom.exe \
        $(CONFIG)/bin/libpcre.dll \
        $(CONFIG)/bin/libhttp.dll \
        $(CONFIG)/bin/http.exe

.PHONY: prep

prep:
	@[ ! -x $(CONFIG)/inc ] && mkdir -p $(CONFIG)/inc $(CONFIG)/obj $(CONFIG)/lib $(CONFIG)/bin ; true
	@[ ! -f $(CONFIG)/inc/bit.h ] && cp projects/http-$(OS)-bit.h $(CONFIG)/inc/bit.h ; true
	@if ! diff $(CONFIG)/inc/bit.h projects/http-$(OS)-bit.h >/dev/null ; then\
		echo cp projects/http-$(OS)-bit.h $(CONFIG)/inc/bit.h  ; \
		cp projects/http-$(OS)-bit.h $(CONFIG)/inc/bit.h  ; \
	fi; true

clean:
	rm -rf $(CONFIG)/bin/libmpr.dll
	rm -rf $(CONFIG)/bin/libmprssl.dll
	rm -rf $(CONFIG)/bin/${settings.manager}.exe
	rm -rf $(CONFIG)/bin/makerom.exe
	rm -rf $(CONFIG)/bin/libpcre.dll
	rm -rf $(CONFIG)/bin/libhttp.dll
	rm -rf $(CONFIG)/bin/http.exe
	rm -rf $(CONFIG)/obj/mprLib.obj
	rm -rf $(CONFIG)/obj/mprSsl.obj
	rm -rf $(CONFIG)/obj/manager.obj
	rm -rf $(CONFIG)/obj/makerom.obj
	rm -rf $(CONFIG)/obj/pcre.obj
	rm -rf $(CONFIG)/obj/auth.obj
	rm -rf $(CONFIG)/obj/authCheck.obj
	rm -rf $(CONFIG)/obj/authFile.obj
	rm -rf $(CONFIG)/obj/authPam.obj
	rm -rf $(CONFIG)/obj/cache.obj
	rm -rf $(CONFIG)/obj/chunkFilter.obj
	rm -rf $(CONFIG)/obj/client.obj
	rm -rf $(CONFIG)/obj/conn.obj
	rm -rf $(CONFIG)/obj/endpoint.obj
	rm -rf $(CONFIG)/obj/error.obj
	rm -rf $(CONFIG)/obj/host.obj
	rm -rf $(CONFIG)/obj/httpService.obj
	rm -rf $(CONFIG)/obj/log.obj
	rm -rf $(CONFIG)/obj/netConnector.obj
	rm -rf $(CONFIG)/obj/packet.obj
	rm -rf $(CONFIG)/obj/passHandler.obj
	rm -rf $(CONFIG)/obj/pipeline.obj
	rm -rf $(CONFIG)/obj/queue.obj
	rm -rf $(CONFIG)/obj/rangeFilter.obj
	rm -rf $(CONFIG)/obj/route.obj
	rm -rf $(CONFIG)/obj/rx.obj
	rm -rf $(CONFIG)/obj/sendConnector.obj
	rm -rf $(CONFIG)/obj/stage.obj
	rm -rf $(CONFIG)/obj/trace.obj
	rm -rf $(CONFIG)/obj/tx.obj
	rm -rf $(CONFIG)/obj/uploadFilter.obj
	rm -rf $(CONFIG)/obj/uri.obj
	rm -rf $(CONFIG)/obj/var.obj
	rm -rf $(CONFIG)/obj/http.obj

clobber: clean
	rm -fr ./$(CONFIG)

$(CONFIG)/inc/mpr.h: 
	rm -fr $(CONFIG)/inc/mpr.h
	cp -r src/deps/mpr/mpr.h $(CONFIG)/inc/mpr.h

$(CONFIG)/inc/mprSsl.h: 
	rm -fr $(CONFIG)/inc/mprSsl.h
	cp -r src/deps/mpr/mprSsl.h $(CONFIG)/inc/mprSsl.h

$(CONFIG)/obj/mprLib.obj: \
        src/deps/mpr/mprLib.c \
        $(CONFIG)/inc/bit.h
	"$(CC)" -c -Fo$(CONFIG)/obj/mprLib.obj -Fd$(CONFIG)/obj/mprLib.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

$(CONFIG)/bin/libmpr.dll:  \
        $(CONFIG)/inc/mpr.h \
        $(CONFIG)/inc/mprSsl.h \
        $(CONFIG)/obj/mprLib.obj
	"$(LD)" -dll -out:$(CONFIG)/bin/libmpr.dll -entry:_DllMainCRTStartup@12 -def:$(CONFIG)/bin/libmpr.def $(LIBPATHS) $(CONFIG)/obj/mprLib.obj $(LIBS)

$(CONFIG)/obj/mprSsl.obj: \
        src/deps/mpr/mprSsl.c \
        $(CONFIG)/inc/bit.h
	"$(CC)" -c -Fo$(CONFIG)/obj/mprSsl.obj -Fd$(CONFIG)/obj/mprSsl.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprSsl.c

$(CONFIG)/bin/libmprssl.dll:  \
        $(CONFIG)/bin/libmpr.dll \
        $(CONFIG)/obj/mprSsl.obj
	"$(LD)" -dll -out:$(CONFIG)/bin/libmprssl.dll -entry:_DllMainCRTStartup@12 -def:$(CONFIG)/bin/libmprssl.def $(LIBPATHS) $(CONFIG)/obj/mprSsl.obj $(LIBS) libmpr.lib

$(CONFIG)/obj/makerom.obj: \
        src/deps/mpr/makerom.c \
        $(CONFIG)/inc/bit.h
	"$(CC)" -c -Fo$(CONFIG)/obj/makerom.obj -Fd$(CONFIG)/obj/makerom.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

$(CONFIG)/bin/makerom.exe:  \
        $(CONFIG)/bin/libmpr.dll \
        $(CONFIG)/obj/makerom.obj
	"$(LD)" -out:$(CONFIG)/bin/makerom.exe -entry:mainCRTStartup -subsystem:console $(LIBPATHS) $(CONFIG)/obj/makerom.obj $(LIBS) libmpr.lib

$(CONFIG)/obj/pcre.obj: \
        src/deps/pcre/pcre.c \
        $(CONFIG)/inc/bit.h \
        src/deps/pcre/pcre.h
	"$(CC)" -c -Fo$(CONFIG)/obj/pcre.obj -Fd$(CONFIG)/obj/pcre.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

$(CONFIG)/bin/libpcre.dll:  \
        $(CONFIG)/obj/pcre.obj
	"$(LD)" -dll -out:$(CONFIG)/bin/libpcre.dll -entry:_DllMainCRTStartup@12 -def:$(CONFIG)/bin/libpcre.def $(LIBPATHS) $(CONFIG)/obj/pcre.obj $(LIBS)

$(CONFIG)/inc/http.h: 
	rm -fr $(CONFIG)/inc/http.h
	cp -r src/http.h $(CONFIG)/inc/http.h

$(CONFIG)/obj/auth.obj: \
        src/auth.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/auth.obj -Fd$(CONFIG)/obj/auth.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/auth.c

$(CONFIG)/obj/authCheck.obj: \
        src/authCheck.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/authCheck.obj -Fd$(CONFIG)/obj/authCheck.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/authCheck.c

$(CONFIG)/obj/authFile.obj: \
        src/authFile.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/authFile.obj -Fd$(CONFIG)/obj/authFile.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/authFile.c

$(CONFIG)/obj/authPam.obj: \
        src/authPam.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/authPam.obj -Fd$(CONFIG)/obj/authPam.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/authPam.c

$(CONFIG)/obj/cache.obj: \
        src/cache.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/cache.obj -Fd$(CONFIG)/obj/cache.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/cache.c

$(CONFIG)/obj/chunkFilter.obj: \
        src/chunkFilter.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/chunkFilter.obj -Fd$(CONFIG)/obj/chunkFilter.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/chunkFilter.c

$(CONFIG)/obj/client.obj: \
        src/client.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/client.obj -Fd$(CONFIG)/obj/client.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/client.c

$(CONFIG)/obj/conn.obj: \
        src/conn.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/conn.obj -Fd$(CONFIG)/obj/conn.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/conn.c

$(CONFIG)/obj/endpoint.obj: \
        src/endpoint.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/endpoint.obj -Fd$(CONFIG)/obj/endpoint.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/endpoint.c

$(CONFIG)/obj/error.obj: \
        src/error.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/error.obj -Fd$(CONFIG)/obj/error.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/error.c

$(CONFIG)/obj/host.obj: \
        src/host.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/host.obj -Fd$(CONFIG)/obj/host.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/host.c

$(CONFIG)/obj/httpService.obj: \
        src/httpService.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/httpService.obj -Fd$(CONFIG)/obj/httpService.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/httpService.c

$(CONFIG)/obj/log.obj: \
        src/log.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/log.obj -Fd$(CONFIG)/obj/log.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/log.c

$(CONFIG)/obj/netConnector.obj: \
        src/netConnector.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/netConnector.obj -Fd$(CONFIG)/obj/netConnector.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/netConnector.c

$(CONFIG)/obj/packet.obj: \
        src/packet.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/packet.obj -Fd$(CONFIG)/obj/packet.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/packet.c

$(CONFIG)/obj/passHandler.obj: \
        src/passHandler.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/passHandler.obj -Fd$(CONFIG)/obj/passHandler.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/passHandler.c

$(CONFIG)/obj/pipeline.obj: \
        src/pipeline.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/pipeline.obj -Fd$(CONFIG)/obj/pipeline.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/pipeline.c

$(CONFIG)/obj/queue.obj: \
        src/queue.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/queue.obj -Fd$(CONFIG)/obj/queue.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/queue.c

$(CONFIG)/obj/rangeFilter.obj: \
        src/rangeFilter.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/rangeFilter.obj -Fd$(CONFIG)/obj/rangeFilter.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/rangeFilter.c

$(CONFIG)/obj/route.obj: \
        src/route.c \
        $(CONFIG)/inc/bit.h \
        src/http.h \
        src/deps/pcre/pcre.h
	"$(CC)" -c -Fo$(CONFIG)/obj/route.obj -Fd$(CONFIG)/obj/route.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/route.c

$(CONFIG)/obj/rx.obj: \
        src/rx.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/rx.obj -Fd$(CONFIG)/obj/rx.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/rx.c

$(CONFIG)/obj/sendConnector.obj: \
        src/sendConnector.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/sendConnector.obj -Fd$(CONFIG)/obj/sendConnector.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/sendConnector.c

$(CONFIG)/obj/stage.obj: \
        src/stage.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/stage.obj -Fd$(CONFIG)/obj/stage.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/stage.c

$(CONFIG)/obj/trace.obj: \
        src/trace.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/trace.obj -Fd$(CONFIG)/obj/trace.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/trace.c

$(CONFIG)/obj/tx.obj: \
        src/tx.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/tx.obj -Fd$(CONFIG)/obj/tx.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/tx.c

$(CONFIG)/obj/uploadFilter.obj: \
        src/uploadFilter.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/uploadFilter.obj -Fd$(CONFIG)/obj/uploadFilter.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/uploadFilter.c

$(CONFIG)/obj/uri.obj: \
        src/uri.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/uri.obj -Fd$(CONFIG)/obj/uri.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/uri.c

$(CONFIG)/obj/var.obj: \
        src/var.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/var.obj -Fd$(CONFIG)/obj/var.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/var.c

$(CONFIG)/bin/libhttp.dll:  \
        $(CONFIG)/bin/libmpr.dll \
        $(CONFIG)/bin/libpcre.dll \
        $(CONFIG)/bin/libmprssl.dll \
        $(CONFIG)/inc/http.h \
        $(CONFIG)/obj/auth.obj \
        $(CONFIG)/obj/authCheck.obj \
        $(CONFIG)/obj/authFile.obj \
        $(CONFIG)/obj/authPam.obj \
        $(CONFIG)/obj/cache.obj \
        $(CONFIG)/obj/chunkFilter.obj \
        $(CONFIG)/obj/client.obj \
        $(CONFIG)/obj/conn.obj \
        $(CONFIG)/obj/endpoint.obj \
        $(CONFIG)/obj/error.obj \
        $(CONFIG)/obj/host.obj \
        $(CONFIG)/obj/httpService.obj \
        $(CONFIG)/obj/log.obj \
        $(CONFIG)/obj/netConnector.obj \
        $(CONFIG)/obj/packet.obj \
        $(CONFIG)/obj/passHandler.obj \
        $(CONFIG)/obj/pipeline.obj \
        $(CONFIG)/obj/queue.obj \
        $(CONFIG)/obj/rangeFilter.obj \
        $(CONFIG)/obj/route.obj \
        $(CONFIG)/obj/rx.obj \
        $(CONFIG)/obj/sendConnector.obj \
        $(CONFIG)/obj/stage.obj \
        $(CONFIG)/obj/trace.obj \
        $(CONFIG)/obj/tx.obj \
        $(CONFIG)/obj/uploadFilter.obj \
        $(CONFIG)/obj/uri.obj \
        $(CONFIG)/obj/var.obj
	"$(LD)" -dll -out:$(CONFIG)/bin/libhttp.dll -entry:_DllMainCRTStartup@12 -def:$(CONFIG)/bin/libhttp.def $(LIBPATHS) $(CONFIG)/obj/auth.obj $(CONFIG)/obj/authCheck.obj $(CONFIG)/obj/authFile.obj $(CONFIG)/obj/authPam.obj $(CONFIG)/obj/cache.obj $(CONFIG)/obj/chunkFilter.obj $(CONFIG)/obj/client.obj $(CONFIG)/obj/conn.obj $(CONFIG)/obj/endpoint.obj $(CONFIG)/obj/error.obj $(CONFIG)/obj/host.obj $(CONFIG)/obj/httpService.obj $(CONFIG)/obj/log.obj $(CONFIG)/obj/netConnector.obj $(CONFIG)/obj/packet.obj $(CONFIG)/obj/passHandler.obj $(CONFIG)/obj/pipeline.obj $(CONFIG)/obj/queue.obj $(CONFIG)/obj/rangeFilter.obj $(CONFIG)/obj/route.obj $(CONFIG)/obj/rx.obj $(CONFIG)/obj/sendConnector.obj $(CONFIG)/obj/stage.obj $(CONFIG)/obj/trace.obj $(CONFIG)/obj/tx.obj $(CONFIG)/obj/uploadFilter.obj $(CONFIG)/obj/uri.obj $(CONFIG)/obj/var.obj $(LIBS) libmpr.lib libpcre.lib libmprssl.lib

$(CONFIG)/obj/http.obj: \
        src/utils/http.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	"$(CC)" -c -Fo$(CONFIG)/obj/http.obj -Fd$(CONFIG)/obj/http.pdb $(CFLAGS) -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/utils/http.c

$(CONFIG)/bin/http.exe:  \
        $(CONFIG)/bin/libhttp.dll \
        $(CONFIG)/obj/http.obj
	"$(LD)" -out:$(CONFIG)/bin/http.exe -entry:mainCRTStartup -subsystem:console $(LIBPATHS) $(CONFIG)/obj/http.obj $(LIBS) libhttp.lib libmpr.lib libpcre.lib libmprssl.lib

