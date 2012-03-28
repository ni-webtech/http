#
#   macosx-i686-debug.mk -- Build It Makefile to build Http Library for macosx on i686
#

PLATFORM       := macosx-i686-debug
CC             := cc
LD             := ld
CFLAGS         := -fPIC -Wall -g
DFLAGS         := -DPIC -DCPU=I686
IFLAGS         := -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc
LDFLAGS        := -Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -L$(PLATFORM)/lib -g -ldl
LIBS           := -lpthread -lm

all: prep \
        $(PLATFORM)/lib/libmpr.dylib \
        $(PLATFORM)/lib/libmprssl.dylib \
        $(PLATFORM)/bin/makerom \
        $(PLATFORM)/lib/libpcre.dylib \
        $(PLATFORM)/lib/libhttp.dylib \
        $(PLATFORM)/bin/http

.PHONY: prep

prep:
	@[ ! -x $(PLATFORM)/inc ] && mkdir -p $(PLATFORM)/inc $(PLATFORM)/obj $(PLATFORM)/lib $(PLATFORM)/bin ; true
	@[ ! -f $(PLATFORM)/inc/buildConfig.h ] && cp projects/buildConfig.$(PLATFORM) $(PLATFORM)/inc/buildConfig.h ; true

clean:
	rm -rf $(PLATFORM)/lib/libmpr.dylib
	rm -rf $(PLATFORM)/lib/libmprssl.dylib
	rm -rf $(PLATFORM)/bin/${settings.manager}
	rm -rf $(PLATFORM)/bin/makerom
	rm -rf $(PLATFORM)/lib/libpcre.dylib
	rm -rf $(PLATFORM)/lib/libhttp.dylib
	rm -rf $(PLATFORM)/bin/http
	rm -rf $(PLATFORM)/obj/mprLib.o
	rm -rf $(PLATFORM)/obj/mprSsl.o
	rm -rf $(PLATFORM)/obj/manager.o
	rm -rf $(PLATFORM)/obj/makerom.o
	rm -rf $(PLATFORM)/obj/pcre.o
	rm -rf $(PLATFORM)/obj/auth.o
	rm -rf $(PLATFORM)/obj/authCheck.o
	rm -rf $(PLATFORM)/obj/authFile.o
	rm -rf $(PLATFORM)/obj/authPam.o
	rm -rf $(PLATFORM)/obj/cache.o
	rm -rf $(PLATFORM)/obj/chunkFilter.o
	rm -rf $(PLATFORM)/obj/client.o
	rm -rf $(PLATFORM)/obj/conn.o
	rm -rf $(PLATFORM)/obj/endpoint.o
	rm -rf $(PLATFORM)/obj/error.o
	rm -rf $(PLATFORM)/obj/host.o
	rm -rf $(PLATFORM)/obj/httpService.o
	rm -rf $(PLATFORM)/obj/log.o
	rm -rf $(PLATFORM)/obj/netConnector.o
	rm -rf $(PLATFORM)/obj/packet.o
	rm -rf $(PLATFORM)/obj/passHandler.o
	rm -rf $(PLATFORM)/obj/pipeline.o
	rm -rf $(PLATFORM)/obj/queue.o
	rm -rf $(PLATFORM)/obj/rangeFilter.o
	rm -rf $(PLATFORM)/obj/route.o
	rm -rf $(PLATFORM)/obj/rx.o
	rm -rf $(PLATFORM)/obj/sendConnector.o
	rm -rf $(PLATFORM)/obj/stage.o
	rm -rf $(PLATFORM)/obj/trace.o
	rm -rf $(PLATFORM)/obj/tx.o
	rm -rf $(PLATFORM)/obj/uploadFilter.o
	rm -rf $(PLATFORM)/obj/uri.o
	rm -rf $(PLATFORM)/obj/var.o
	rm -rf $(PLATFORM)/obj/http.o

clobber: clean
	rm -fr ./$(PLATFORM)

$(PLATFORM)/inc/mpr.h: 
	rm -fr macosx-i686-debug/inc/mpr.h
	cp -r src/deps/mpr/mpr.h macosx-i686-debug/inc/mpr.h

$(PLATFORM)/inc/mprSsl.h: 
	rm -fr macosx-i686-debug/inc/mprSsl.h
	cp -r src/deps/mpr/mprSsl.h macosx-i686-debug/inc/mprSsl.h

$(PLATFORM)/obj/mprLib.o: \
        src/deps/mpr/mprLib.c \
        $(PLATFORM)/inc/buildConfig.h
	$(CC) -c -o $(PLATFORM)/obj/mprLib.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

$(PLATFORM)/lib/libmpr.dylib:  \
        $(PLATFORM)/inc/mpr.h \
        $(PLATFORM)/inc/mprSsl.h \
        $(PLATFORM)/obj/mprLib.o
	$(CC) -dynamiclib -o $(PLATFORM)/lib/libmpr.dylib -arch i686 $(LDFLAGS) -install_name @rpath/libmpr.dylib $(PLATFORM)/obj/mprLib.o $(LIBS)

$(PLATFORM)/obj/mprSsl.o: \
        src/deps/mpr/mprSsl.c \
        $(PLATFORM)/inc/buildConfig.h
	$(CC) -c -o $(PLATFORM)/obj/mprSsl.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc -I../../../../usr/include src/deps/mpr/mprSsl.c

$(PLATFORM)/lib/libmprssl.dylib:  \
        $(PLATFORM)/lib/libmpr.dylib \
        $(PLATFORM)/obj/mprSsl.o
	$(CC) -dynamiclib -o $(PLATFORM)/lib/libmprssl.dylib -arch i686 $(LDFLAGS) -L/usr/lib -install_name @rpath/libmprssl.dylib $(PLATFORM)/obj/mprSsl.o $(LIBS) -lmpr -lssl -lcrypto

$(PLATFORM)/obj/makerom.o: \
        src/deps/mpr/makerom.c \
        $(PLATFORM)/inc/buildConfig.h
	$(CC) -c -o $(PLATFORM)/obj/makerom.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

$(PLATFORM)/bin/makerom:  \
        $(PLATFORM)/lib/libmpr.dylib \
        $(PLATFORM)/obj/makerom.o
	$(CC) -o $(PLATFORM)/bin/makerom -arch i686 $(LDFLAGS) -L$(PLATFORM)/lib $(PLATFORM)/obj/makerom.o $(LIBS) -lmpr

$(PLATFORM)/obj/pcre.o: \
        src/deps/pcre/pcre.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o $(PLATFORM)/obj/pcre.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

$(PLATFORM)/lib/libpcre.dylib:  \
        $(PLATFORM)/obj/pcre.o
	$(CC) -dynamiclib -o $(PLATFORM)/lib/libpcre.dylib -arch i686 $(LDFLAGS) -install_name @rpath/libpcre.dylib $(PLATFORM)/obj/pcre.o $(LIBS)

$(PLATFORM)/inc/http.h: 
	rm -fr macosx-i686-debug/inc/http.h
	cp -r src/http.h macosx-i686-debug/inc/http.h

$(PLATFORM)/obj/auth.o: \
        src/auth.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/auth.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/auth.c

$(PLATFORM)/obj/authCheck.o: \
        src/authCheck.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/authCheck.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/authCheck.c

$(PLATFORM)/obj/authFile.o: \
        src/authFile.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/authFile.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/authFile.c

$(PLATFORM)/obj/authPam.o: \
        src/authPam.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/authPam.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/authPam.c

$(PLATFORM)/obj/cache.o: \
        src/cache.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/cache.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/cache.c

$(PLATFORM)/obj/chunkFilter.o: \
        src/chunkFilter.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/chunkFilter.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/chunkFilter.c

$(PLATFORM)/obj/client.o: \
        src/client.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/client.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/client.c

$(PLATFORM)/obj/conn.o: \
        src/conn.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/conn.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/conn.c

$(PLATFORM)/obj/endpoint.o: \
        src/endpoint.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/endpoint.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/endpoint.c

$(PLATFORM)/obj/error.o: \
        src/error.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/error.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/error.c

$(PLATFORM)/obj/host.o: \
        src/host.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/host.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/host.c

$(PLATFORM)/obj/httpService.o: \
        src/httpService.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/httpService.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/httpService.c

$(PLATFORM)/obj/log.o: \
        src/log.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/log.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/log.c

$(PLATFORM)/obj/netConnector.o: \
        src/netConnector.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/netConnector.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/netConnector.c

$(PLATFORM)/obj/packet.o: \
        src/packet.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/packet.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/packet.c

$(PLATFORM)/obj/passHandler.o: \
        src/passHandler.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/passHandler.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/passHandler.c

$(PLATFORM)/obj/pipeline.o: \
        src/pipeline.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/pipeline.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/pipeline.c

$(PLATFORM)/obj/queue.o: \
        src/queue.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/queue.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/queue.c

$(PLATFORM)/obj/rangeFilter.o: \
        src/rangeFilter.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/rangeFilter.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/rangeFilter.c

$(PLATFORM)/obj/route.o: \
        src/route.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o $(PLATFORM)/obj/route.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/route.c

$(PLATFORM)/obj/rx.o: \
        src/rx.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/rx.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/rx.c

$(PLATFORM)/obj/sendConnector.o: \
        src/sendConnector.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/sendConnector.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/sendConnector.c

$(PLATFORM)/obj/stage.o: \
        src/stage.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/stage.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/stage.c

$(PLATFORM)/obj/trace.o: \
        src/trace.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/trace.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/trace.c

$(PLATFORM)/obj/tx.o: \
        src/tx.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/tx.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/tx.c

$(PLATFORM)/obj/uploadFilter.o: \
        src/uploadFilter.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/uploadFilter.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/uploadFilter.c

$(PLATFORM)/obj/uri.o: \
        src/uri.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/uri.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/uri.c

$(PLATFORM)/obj/var.o: \
        src/var.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/var.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/var.c

$(PLATFORM)/lib/libhttp.dylib:  \
        $(PLATFORM)/lib/libmpr.dylib \
        $(PLATFORM)/lib/libpcre.dylib \
        $(PLATFORM)/lib/libmprssl.dylib \
        $(PLATFORM)/inc/http.h \
        $(PLATFORM)/obj/auth.o \
        $(PLATFORM)/obj/authCheck.o \
        $(PLATFORM)/obj/authFile.o \
        $(PLATFORM)/obj/authPam.o \
        $(PLATFORM)/obj/cache.o \
        $(PLATFORM)/obj/chunkFilter.o \
        $(PLATFORM)/obj/client.o \
        $(PLATFORM)/obj/conn.o \
        $(PLATFORM)/obj/endpoint.o \
        $(PLATFORM)/obj/error.o \
        $(PLATFORM)/obj/host.o \
        $(PLATFORM)/obj/httpService.o \
        $(PLATFORM)/obj/log.o \
        $(PLATFORM)/obj/netConnector.o \
        $(PLATFORM)/obj/packet.o \
        $(PLATFORM)/obj/passHandler.o \
        $(PLATFORM)/obj/pipeline.o \
        $(PLATFORM)/obj/queue.o \
        $(PLATFORM)/obj/rangeFilter.o \
        $(PLATFORM)/obj/route.o \
        $(PLATFORM)/obj/rx.o \
        $(PLATFORM)/obj/sendConnector.o \
        $(PLATFORM)/obj/stage.o \
        $(PLATFORM)/obj/trace.o \
        $(PLATFORM)/obj/tx.o \
        $(PLATFORM)/obj/uploadFilter.o \
        $(PLATFORM)/obj/uri.o \
        $(PLATFORM)/obj/var.o
	$(CC) -dynamiclib -o $(PLATFORM)/lib/libhttp.dylib -arch i686 $(LDFLAGS) -L/usr/lib -install_name @rpath/libhttp.dylib $(PLATFORM)/obj/auth.o $(PLATFORM)/obj/authCheck.o $(PLATFORM)/obj/authFile.o $(PLATFORM)/obj/authPam.o $(PLATFORM)/obj/cache.o $(PLATFORM)/obj/chunkFilter.o $(PLATFORM)/obj/client.o $(PLATFORM)/obj/conn.o $(PLATFORM)/obj/endpoint.o $(PLATFORM)/obj/error.o $(PLATFORM)/obj/host.o $(PLATFORM)/obj/httpService.o $(PLATFORM)/obj/log.o $(PLATFORM)/obj/netConnector.o $(PLATFORM)/obj/packet.o $(PLATFORM)/obj/passHandler.o $(PLATFORM)/obj/pipeline.o $(PLATFORM)/obj/queue.o $(PLATFORM)/obj/rangeFilter.o $(PLATFORM)/obj/route.o $(PLATFORM)/obj/rx.o $(PLATFORM)/obj/sendConnector.o $(PLATFORM)/obj/stage.o $(PLATFORM)/obj/trace.o $(PLATFORM)/obj/tx.o $(PLATFORM)/obj/uploadFilter.o $(PLATFORM)/obj/uri.o $(PLATFORM)/obj/var.o $(LIBS) -lmpr -lpcre -lmprssl -lssl -lcrypto

$(PLATFORM)/obj/http.o: \
        src/utils/http.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/http.o -arch i686 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/utils/http.c

$(PLATFORM)/bin/http:  \
        $(PLATFORM)/lib/libhttp.dylib \
        $(PLATFORM)/obj/http.o
	$(CC) -o $(PLATFORM)/bin/http -arch i686 $(LDFLAGS) -L/usr/lib -L$(PLATFORM)/lib $(PLATFORM)/obj/http.o $(LIBS) -lhttp -lmpr -lpcre -lmprssl -lssl -lcrypto

