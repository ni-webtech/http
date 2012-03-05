#
#   build.mk -- Build It Makefile to build Http Library for macosx on x86_64
#

PLATFORM  := macosx-x86_64-debug
CC        := /usr/bin/cc
CFLAGS    := -fPIC -Wall -g -Wshorten-64-to-32
DFLAGS    := -DPIC -DCPU=X86_64
IFLAGS    := -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc
LDFLAGS   := -Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -L/Users/mob/git/http/$(PLATFORM)/lib -g -ldl
LIBS      := -lpthread -lm

all: \
        $(PLATFORM)/lib/libmpr.dylib \
        $(PLATFORM)/bin/manager \
        $(PLATFORM)/bin/makerom \
        $(PLATFORM)/lib/libpcre.dylib \
        $(PLATFORM)/lib/libhttp.dylib \
        $(PLATFORM)/bin/http

.PHONY: prep

prep:
	@if [ ! -x $(PLATFORM)/inc ] ; then \
		mkdir -p $(PLATFORM)/inc $(PLATFORM)/obj $(PLATFORM)/lib $(PLATFORM)/bin ; \
		cp src/buildConfig.default $(PLATFORM)/inc\
	fi

clean:
	rm -rf $(PLATFORM)/lib/libmpr.dylib
	rm -rf $(PLATFORM)/lib/libmprssl.dylib
	rm -rf $(PLATFORM)/bin/manager
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

$(PLATFORM)/obj/mprLib.o: \
        src/deps/mpr/mprLib.c \
        $(PLATFORM)/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o $(PLATFORM)/obj/mprLib.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

$(PLATFORM)/lib/libmpr.dylib:  \
        $(PLATFORM)/obj/mprLib.o
	$(CC) -dynamiclib -o $(PLATFORM)/lib/libmpr.dylib -arch x86_64 -Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -L$(PLATFORM)/lib -g -ldl -install_name @rpath/libmpr.dylib $(PLATFORM)/obj/mprLib.o $(LIBS)

$(PLATFORM)/obj/manager.o: \
        src/deps/mpr/manager.c \
        $(PLATFORM)/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o $(PLATFORM)/obj/manager.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/deps/mpr/manager.c

$(PLATFORM)/bin/manager:  \
        $(PLATFORM)/lib/libmpr.dylib \
        $(PLATFORM)/obj/manager.o
	$(CC) -o $(PLATFORM)/bin/manager -arch x86_64 -Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -L$(PLATFORM)/lib -g -ldl -L$(PLATFORM)/lib $(PLATFORM)/obj/manager.o $(LIBS) -lmpr

$(PLATFORM)/obj/makerom.o: \
        src/deps/mpr/makerom.c \
        $(PLATFORM)/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o $(PLATFORM)/obj/makerom.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

$(PLATFORM)/bin/makerom:  \
        $(PLATFORM)/lib/libmpr.dylib \
        $(PLATFORM)/obj/makerom.o
	$(CC) -o $(PLATFORM)/bin/makerom -arch x86_64 -Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -L$(PLATFORM)/lib -g -ldl -L$(PLATFORM)/lib $(PLATFORM)/obj/makerom.o $(LIBS) -lmpr

$(PLATFORM)/obj/pcre.o: \
        src/deps/pcre/pcre.c \
        $(PLATFORM)/inc/bit.h \
        $(PLATFORM)/inc/buildConfig.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o $(PLATFORM)/obj/pcre.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

$(PLATFORM)/lib/libpcre.dylib:  \
        $(PLATFORM)/obj/pcre.o
	$(CC) -dynamiclib -o $(PLATFORM)/lib/libpcre.dylib -arch x86_64 -Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -L$(PLATFORM)/lib -g -ldl -install_name @rpath/libpcre.dylib $(PLATFORM)/obj/pcre.o $(LIBS)

$(PLATFORM)/obj/auth.o: \
        src/auth.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/auth.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/auth.c

$(PLATFORM)/obj/authCheck.o: \
        src/authCheck.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/authCheck.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/authCheck.c

$(PLATFORM)/obj/authFile.o: \
        src/authFile.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/authFile.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/authFile.c

$(PLATFORM)/obj/authPam.o: \
        src/authPam.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/authPam.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/authPam.c

$(PLATFORM)/obj/cache.o: \
        src/cache.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/cache.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/cache.c

$(PLATFORM)/obj/chunkFilter.o: \
        src/chunkFilter.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/chunkFilter.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/chunkFilter.c

$(PLATFORM)/obj/client.o: \
        src/client.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/client.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/client.c

$(PLATFORM)/obj/conn.o: \
        src/conn.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/conn.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/conn.c

$(PLATFORM)/obj/endpoint.o: \
        src/endpoint.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/endpoint.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/endpoint.c

$(PLATFORM)/obj/error.o: \
        src/error.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/error.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/error.c

$(PLATFORM)/obj/host.o: \
        src/host.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/host.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/host.c

$(PLATFORM)/obj/httpService.o: \
        src/httpService.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/httpService.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/httpService.c

$(PLATFORM)/obj/log.o: \
        src/log.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/log.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/log.c

$(PLATFORM)/obj/netConnector.o: \
        src/netConnector.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/netConnector.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/netConnector.c

$(PLATFORM)/obj/packet.o: \
        src/packet.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/packet.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/packet.c

$(PLATFORM)/obj/passHandler.o: \
        src/passHandler.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/passHandler.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/passHandler.c

$(PLATFORM)/obj/pipeline.o: \
        src/pipeline.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/pipeline.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/pipeline.c

$(PLATFORM)/obj/queue.o: \
        src/queue.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/queue.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/queue.c

$(PLATFORM)/obj/rangeFilter.o: \
        src/rangeFilter.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/rangeFilter.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/rangeFilter.c

$(PLATFORM)/obj/route.o: \
        src/route.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o $(PLATFORM)/obj/route.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/route.c

$(PLATFORM)/obj/rx.o: \
        src/rx.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/rx.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/rx.c

$(PLATFORM)/obj/sendConnector.o: \
        src/sendConnector.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/sendConnector.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/sendConnector.c

$(PLATFORM)/obj/stage.o: \
        src/stage.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/stage.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/stage.c

$(PLATFORM)/obj/trace.o: \
        src/trace.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/trace.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/trace.c

$(PLATFORM)/obj/tx.o: \
        src/tx.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/tx.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/tx.c

$(PLATFORM)/obj/uploadFilter.o: \
        src/uploadFilter.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/uploadFilter.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/uploadFilter.c

$(PLATFORM)/obj/uri.o: \
        src/uri.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/uri.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/uri.c

$(PLATFORM)/obj/var.o: \
        src/var.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/var.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/var.c

$(PLATFORM)/lib/libhttp.dylib:  \
        $(PLATFORM)/lib/libmpr.dylib \
        $(PLATFORM)/lib/libpcre.dylib \
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
	$(CC) -dynamiclib -o $(PLATFORM)/lib/libhttp.dylib -arch x86_64 -Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -L$(PLATFORM)/lib -g -ldl -install_name @rpath/libhttp.dylib $(PLATFORM)/obj/auth.o $(PLATFORM)/obj/authCheck.o $(PLATFORM)/obj/authFile.o $(PLATFORM)/obj/authPam.o $(PLATFORM)/obj/cache.o $(PLATFORM)/obj/chunkFilter.o $(PLATFORM)/obj/client.o $(PLATFORM)/obj/conn.o $(PLATFORM)/obj/endpoint.o $(PLATFORM)/obj/error.o $(PLATFORM)/obj/host.o $(PLATFORM)/obj/httpService.o $(PLATFORM)/obj/log.o $(PLATFORM)/obj/netConnector.o $(PLATFORM)/obj/packet.o $(PLATFORM)/obj/passHandler.o $(PLATFORM)/obj/pipeline.o $(PLATFORM)/obj/queue.o $(PLATFORM)/obj/rangeFilter.o $(PLATFORM)/obj/route.o $(PLATFORM)/obj/rx.o $(PLATFORM)/obj/sendConnector.o $(PLATFORM)/obj/stage.o $(PLATFORM)/obj/trace.o $(PLATFORM)/obj/tx.o $(PLATFORM)/obj/uploadFilter.o $(PLATFORM)/obj/uri.o $(PLATFORM)/obj/var.o $(LIBS) -lmpr -lpcre

$(PLATFORM)/obj/http.o: \
        src/utils/http.c \
        $(PLATFORM)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/http.o -arch x86_64 $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/utils/http.c

$(PLATFORM)/bin/http:  \
        $(PLATFORM)/lib/libhttp.dylib \
        $(PLATFORM)/obj/http.o
	$(CC) -o $(PLATFORM)/bin/http -arch x86_64 -Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -L$(PLATFORM)/lib -g -ldl -L$(PLATFORM)/lib $(PLATFORM)/obj/http.o $(LIBS) -lhttp -lmpr -lpcre

