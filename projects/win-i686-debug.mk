#
#   win-i686-debug.mk -- Build It Makefile to build Http Library for win on i686
#

VS             := $(VSINSTALLDIR)
VS             ?= $(VS)
SDK            := $(WindowsSDKDir)
SDK            ?= $(SDK)

export         SDK VS
export PATH    := $(SDK)/Bin:$(VS)/VC/Bin:$(VS)/Common7/IDE:$(VS)/Common7/Tools:$(VS)/SDK/v3.5/bin:$(VS)/VC/VCPackages;$(PATH)
export INCLUDE := $(INCLUDE);$(SDK)/INCLUDE:$(VS)/VC/INCLUDE
export LIB     := $(LIB);$(SDK)/lib:$(VS)/VC/lib

PLATFORM       := win-i686-debug
CC             := cl.exe
LD             := link.exe
CFLAGS         := -nologo -GR- -W3 -Zi -Od -MDd
DFLAGS         := -D_REENTRANT -D_MT
IFLAGS         := -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc
LDFLAGS        := '-nologo' '-nodefaultlib' '-incremental:no' '-debug' '-machine:x86'
LIBPATHS       := -libpath:$(PLATFORM)/bin
LIBS           := ws2_32.lib advapi32.lib user32.lib kernel32.lib oldnames.lib msvcrt.lib shell32.lib

all: prep \
        $(PLATFORM)/bin/libmpr.dll \
        $(PLATFORM)/bin/makerom.exe \
        $(PLATFORM)/bin/libpcre.dll \
        $(PLATFORM)/bin/libhttp.dll \
        $(PLATFORM)/bin/http.exe

.PHONY: prep

prep:
	@[ ! -x $(PLATFORM)/inc ] && mkdir -p $(PLATFORM)/inc $(PLATFORM)/obj $(PLATFORM)/lib $(PLATFORM)/bin ; true
	@[ ! -f $(PLATFORM)/inc/buildConfig.h ] && cp projects/buildConfig.$(PLATFORM) $(PLATFORM)/inc/buildConfig.h ; true
	@if ! diff $(PLATFORM)/inc/buildConfig.h projects/buildConfig.$(PLATFORM) >/dev/null ; then\
		echo cp projects/buildConfig.$(PLATFORM) $(PLATFORM)/inc/buildConfig.h  ; \
		cp projects/buildConfig.$(PLATFORM) $(PLATFORM)/inc/buildConfig.h  ; \
	fi; true

clean:
	rm -rf $(PLATFORM)/bin/libmpr.dll
	rm -rf $(PLATFORM)/bin/libmprssl.dll
	rm -rf $(PLATFORM)/bin/${settings.manager}
	rm -rf $(PLATFORM)/bin/makerom.exe
	rm -rf $(PLATFORM)/bin/libpcre.dll
	rm -rf $(PLATFORM)/bin/libhttp.dll
	rm -rf $(PLATFORM)/bin/http.exe
	rm -rf $(PLATFORM)/obj/mprLib.obj
	rm -rf $(PLATFORM)/obj/mprSsl.obj
	rm -rf $(PLATFORM)/obj/manager.obj
	rm -rf $(PLATFORM)/obj/makerom.obj
	rm -rf $(PLATFORM)/obj/pcre.obj
	rm -rf $(PLATFORM)/obj/auth.obj
	rm -rf $(PLATFORM)/obj/authCheck.obj
	rm -rf $(PLATFORM)/obj/authFile.obj
	rm -rf $(PLATFORM)/obj/authPam.obj
	rm -rf $(PLATFORM)/obj/cache.obj
	rm -rf $(PLATFORM)/obj/chunkFilter.obj
	rm -rf $(PLATFORM)/obj/client.obj
	rm -rf $(PLATFORM)/obj/conn.obj
	rm -rf $(PLATFORM)/obj/endpoint.obj
	rm -rf $(PLATFORM)/obj/error.obj
	rm -rf $(PLATFORM)/obj/host.obj
	rm -rf $(PLATFORM)/obj/httpService.obj
	rm -rf $(PLATFORM)/obj/log.obj
	rm -rf $(PLATFORM)/obj/netConnector.obj
	rm -rf $(PLATFORM)/obj/packet.obj
	rm -rf $(PLATFORM)/obj/passHandler.obj
	rm -rf $(PLATFORM)/obj/pipeline.obj
	rm -rf $(PLATFORM)/obj/queue.obj
	rm -rf $(PLATFORM)/obj/rangeFilter.obj
	rm -rf $(PLATFORM)/obj/route.obj
	rm -rf $(PLATFORM)/obj/rx.obj
	rm -rf $(PLATFORM)/obj/sendConnector.obj
	rm -rf $(PLATFORM)/obj/stage.obj
	rm -rf $(PLATFORM)/obj/trace.obj
	rm -rf $(PLATFORM)/obj/tx.obj
	rm -rf $(PLATFORM)/obj/uploadFilter.obj
	rm -rf $(PLATFORM)/obj/uri.obj
	rm -rf $(PLATFORM)/obj/var.obj
	rm -rf $(PLATFORM)/obj/http.obj

clobber: clean
	rm -fr ./$(PLATFORM)

$(PLATFORM)/inc/mpr.h: 
	rm -fr win-i686-debug/inc/mpr.h
	cp -r src/deps/mpr/mpr.h win-i686-debug/inc/mpr.h

$(PLATFORM)/inc/mprSsl.h: 
	rm -fr win-i686-debug/inc/mprSsl.h
	cp -r src/deps/mpr/mprSsl.h win-i686-debug/inc/mprSsl.h

$(PLATFORM)/obj/mprLib.obj: \
        src/deps/mpr/mprLib.c \
        $(PLATFORM)/inc/buildConfig.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/mprLib.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

$(PLATFORM)/bin/libmpr.dll:  \
        $(PLATFORM)/inc/mpr.h \
        $(PLATFORM)/inc/mprSsl.h \
        $(PLATFORM)/obj/mprLib.obj
	"$(LD)" -dll -out:$(PLATFORM)/bin/libmpr.dll -entry:_DllMainCRTStartup@12 -def:$(PLATFORM)/bin/libmpr.def $(LDFLAGS) $(LIBPATHS) $(PLATFORM)/obj/mprLib.obj $(LIBS)

$(PLATFORM)/obj/makerom.obj: \
        src/deps/mpr/makerom.c \
        $(PLATFORM)/inc/buildConfig.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/makerom.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

$(PLATFORM)/bin/makerom.exe:  \
        $(PLATFORM)/bin/libmpr.dll \
        $(PLATFORM)/obj/makerom.obj
	"$(LD)" -out:$(PLATFORM)/bin/makerom.exe -entry:mainCRTStartup -subsystem:console $(LDFLAGS) $(LIBPATHS) $(PLATFORM)/obj/makerom.obj $(LIBS) libmpr.lib

$(PLATFORM)/obj/pcre.obj: \
        src/deps/pcre/pcre.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/deps/pcre/pcre.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/pcre.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

$(PLATFORM)/bin/libpcre.dll:  \
        $(PLATFORM)/obj/pcre.obj
	"$(LD)" -dll -out:$(PLATFORM)/bin/libpcre.dll -entry:_DllMainCRTStartup@12 -def:$(PLATFORM)/bin/libpcre.def $(LDFLAGS) $(LIBPATHS) $(PLATFORM)/obj/pcre.obj $(LIBS)

$(PLATFORM)/inc/http.h: 
	rm -fr win-i686-debug/inc/http.h
	cp -r src/http.h win-i686-debug/inc/http.h

$(PLATFORM)/obj/auth.obj: \
        src/auth.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/auth.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/auth.c

$(PLATFORM)/obj/authCheck.obj: \
        src/authCheck.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/authCheck.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/authCheck.c

$(PLATFORM)/obj/authFile.obj: \
        src/authFile.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/authFile.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/authFile.c

$(PLATFORM)/obj/authPam.obj: \
        src/authPam.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/authPam.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/authPam.c

$(PLATFORM)/obj/cache.obj: \
        src/cache.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/cache.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/cache.c

$(PLATFORM)/obj/chunkFilter.obj: \
        src/chunkFilter.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/chunkFilter.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/chunkFilter.c

$(PLATFORM)/obj/client.obj: \
        src/client.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/client.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/client.c

$(PLATFORM)/obj/conn.obj: \
        src/conn.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/conn.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/conn.c

$(PLATFORM)/obj/endpoint.obj: \
        src/endpoint.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/endpoint.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/endpoint.c

$(PLATFORM)/obj/error.obj: \
        src/error.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/error.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/error.c

$(PLATFORM)/obj/host.obj: \
        src/host.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/host.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/host.c

$(PLATFORM)/obj/httpService.obj: \
        src/httpService.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/httpService.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/httpService.c

$(PLATFORM)/obj/log.obj: \
        src/log.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/log.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/log.c

$(PLATFORM)/obj/netConnector.obj: \
        src/netConnector.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/netConnector.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/netConnector.c

$(PLATFORM)/obj/packet.obj: \
        src/packet.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/packet.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/packet.c

$(PLATFORM)/obj/passHandler.obj: \
        src/passHandler.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/passHandler.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/passHandler.c

$(PLATFORM)/obj/pipeline.obj: \
        src/pipeline.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/pipeline.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/pipeline.c

$(PLATFORM)/obj/queue.obj: \
        src/queue.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/queue.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/queue.c

$(PLATFORM)/obj/rangeFilter.obj: \
        src/rangeFilter.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/rangeFilter.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/rangeFilter.c

$(PLATFORM)/obj/route.obj: \
        src/route.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h \
        src/deps/pcre/pcre.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/route.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/route.c

$(PLATFORM)/obj/rx.obj: \
        src/rx.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/rx.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/rx.c

$(PLATFORM)/obj/sendConnector.obj: \
        src/sendConnector.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/sendConnector.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/sendConnector.c

$(PLATFORM)/obj/stage.obj: \
        src/stage.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/stage.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/stage.c

$(PLATFORM)/obj/trace.obj: \
        src/trace.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/trace.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/trace.c

$(PLATFORM)/obj/tx.obj: \
        src/tx.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/tx.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/tx.c

$(PLATFORM)/obj/uploadFilter.obj: \
        src/uploadFilter.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/uploadFilter.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/uploadFilter.c

$(PLATFORM)/obj/uri.obj: \
        src/uri.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/uri.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/uri.c

$(PLATFORM)/obj/var.obj: \
        src/var.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/var.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/var.c

$(PLATFORM)/bin/libhttp.dll:  \
        $(PLATFORM)/bin/libmpr.dll \
        $(PLATFORM)/bin/libpcre.dll \
        $(PLATFORM)/inc/http.h \
        $(PLATFORM)/obj/auth.obj \
        $(PLATFORM)/obj/authCheck.obj \
        $(PLATFORM)/obj/authFile.obj \
        $(PLATFORM)/obj/authPam.obj \
        $(PLATFORM)/obj/cache.obj \
        $(PLATFORM)/obj/chunkFilter.obj \
        $(PLATFORM)/obj/client.obj \
        $(PLATFORM)/obj/conn.obj \
        $(PLATFORM)/obj/endpoint.obj \
        $(PLATFORM)/obj/error.obj \
        $(PLATFORM)/obj/host.obj \
        $(PLATFORM)/obj/httpService.obj \
        $(PLATFORM)/obj/log.obj \
        $(PLATFORM)/obj/netConnector.obj \
        $(PLATFORM)/obj/packet.obj \
        $(PLATFORM)/obj/passHandler.obj \
        $(PLATFORM)/obj/pipeline.obj \
        $(PLATFORM)/obj/queue.obj \
        $(PLATFORM)/obj/rangeFilter.obj \
        $(PLATFORM)/obj/route.obj \
        $(PLATFORM)/obj/rx.obj \
        $(PLATFORM)/obj/sendConnector.obj \
        $(PLATFORM)/obj/stage.obj \
        $(PLATFORM)/obj/trace.obj \
        $(PLATFORM)/obj/tx.obj \
        $(PLATFORM)/obj/uploadFilter.obj \
        $(PLATFORM)/obj/uri.obj \
        $(PLATFORM)/obj/var.obj
	"$(LD)" -dll -out:$(PLATFORM)/bin/libhttp.dll -entry:_DllMainCRTStartup@12 -def:$(PLATFORM)/bin/libhttp.def $(LDFLAGS) $(LIBPATHS) $(PLATFORM)/obj/auth.obj $(PLATFORM)/obj/authCheck.obj $(PLATFORM)/obj/authFile.obj $(PLATFORM)/obj/authPam.obj $(PLATFORM)/obj/cache.obj $(PLATFORM)/obj/chunkFilter.obj $(PLATFORM)/obj/client.obj $(PLATFORM)/obj/conn.obj $(PLATFORM)/obj/endpoint.obj $(PLATFORM)/obj/error.obj $(PLATFORM)/obj/host.obj $(PLATFORM)/obj/httpService.obj $(PLATFORM)/obj/log.obj $(PLATFORM)/obj/netConnector.obj $(PLATFORM)/obj/packet.obj $(PLATFORM)/obj/passHandler.obj $(PLATFORM)/obj/pipeline.obj $(PLATFORM)/obj/queue.obj $(PLATFORM)/obj/rangeFilter.obj $(PLATFORM)/obj/route.obj $(PLATFORM)/obj/rx.obj $(PLATFORM)/obj/sendConnector.obj $(PLATFORM)/obj/stage.obj $(PLATFORM)/obj/trace.obj $(PLATFORM)/obj/tx.obj $(PLATFORM)/obj/uploadFilter.obj $(PLATFORM)/obj/uri.obj $(PLATFORM)/obj/var.obj $(LIBS) libmpr.lib libpcre.lib

$(PLATFORM)/obj/http.obj: \
        src/utils/http.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	"$(CC)" -c -Fo$(PLATFORM)/obj/http.obj -Fd$(PLATFORM)/obj $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/utils/http.c

$(PLATFORM)/bin/http.exe:  \
        $(PLATFORM)/bin/libhttp.dll \
        $(PLATFORM)/obj/http.obj
	"$(LD)" -out:$(PLATFORM)/bin/http.exe -entry:mainCRTStartup -subsystem:console $(LDFLAGS) $(LIBPATHS) $(PLATFORM)/obj/http.obj $(LIBS) libhttp.lib libmpr.lib libpcre.lib

