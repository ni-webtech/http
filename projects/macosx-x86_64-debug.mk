#
#   build.mk -- Build It Makefile to build Http Library for macosx on x86_64
#

CC        := /usr/bin/cc
CFLAGS    := -fPIC -Wall -g -Wshorten-64-to-32
DFLAGS    := -DPIC -DCPU=X86_64
IFLAGS    := -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc
LDFLAGS   := -Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -L/Users/mob/git/http/macosx-x86_64-debug/lib -g -ldl
LIBS      := -lpthread -lm

all: \
        macosx-x86_64-debug/lib/libmpr.dylib \
        macosx-x86_64-debug/bin/manager \
        macosx-x86_64-debug/bin/makerom \
        macosx-x86_64-debug/lib/libpcre.dylib \
        macosx-x86_64-debug/lib/libhttp.dylib \
        macosx-x86_64-debug/bin/http

clean:
	rm -f macosx-x86_64-debug/lib/libmpr.dylib
	rm -f macosx-x86_64-debug/lib/libmprssl.dylib
	rm -f macosx-x86_64-debug/bin/manager
	rm -f macosx-x86_64-debug/bin/makerom
	rm -f macosx-x86_64-debug/lib/libpcre.dylib
	rm -f macosx-x86_64-debug/lib/libhttp.dylib
	rm -f macosx-x86_64-debug/bin/http
	rm -f macosx-x86_64-debug/obj/mprLib.o
	rm -f macosx-x86_64-debug/obj/mprSsl.o
	rm -f macosx-x86_64-debug/obj/manager.o
	rm -f macosx-x86_64-debug/obj/makerom.o
	rm -f macosx-x86_64-debug/obj/pcre.o
	rm -f macosx-x86_64-debug/obj/auth.o
	rm -f macosx-x86_64-debug/obj/authCheck.o
	rm -f macosx-x86_64-debug/obj/authFile.o
	rm -f macosx-x86_64-debug/obj/authPam.o
	rm -f macosx-x86_64-debug/obj/cache.o
	rm -f macosx-x86_64-debug/obj/chunkFilter.o
	rm -f macosx-x86_64-debug/obj/client.o
	rm -f macosx-x86_64-debug/obj/conn.o
	rm -f macosx-x86_64-debug/obj/endpoint.o
	rm -f macosx-x86_64-debug/obj/error.o
	rm -f macosx-x86_64-debug/obj/host.o
	rm -f macosx-x86_64-debug/obj/httpService.o
	rm -f macosx-x86_64-debug/obj/log.o
	rm -f macosx-x86_64-debug/obj/netConnector.o
	rm -f macosx-x86_64-debug/obj/packet.o
	rm -f macosx-x86_64-debug/obj/passHandler.o
	rm -f macosx-x86_64-debug/obj/pipeline.o
	rm -f macosx-x86_64-debug/obj/queue.o
	rm -f macosx-x86_64-debug/obj/rangeFilter.o
	rm -f macosx-x86_64-debug/obj/route.o
	rm -f macosx-x86_64-debug/obj/rx.o
	rm -f macosx-x86_64-debug/obj/sendConnector.o
	rm -f macosx-x86_64-debug/obj/stage.o
	rm -f macosx-x86_64-debug/obj/trace.o
	rm -f macosx-x86_64-debug/obj/tx.o
	rm -f macosx-x86_64-debug/obj/uploadFilter.o
	rm -f macosx-x86_64-debug/obj/uri.o
	rm -f macosx-x86_64-debug/obj/var.o
	rm -f macosx-x86_64-debug/obj/http.o

macosx-x86_64-debug/obj/mprLib.o: \
        src/deps/mpr/mprLib.c \
        macosx-x86_64-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o macosx-x86_64-debug/obj/mprLib.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/deps/mpr/mprLib.c

macosx-x86_64-debug/lib/libmpr.dylib:  \
        macosx-x86_64-debug/obj/mprLib.o
	$(CC) -dynamiclib -o macosx-x86_64-debug/lib/libmpr.dylib -arch x86_64 $(LDFLAGS) -install_name @rpath/libmpr.dylib macosx-x86_64-debug/obj/mprLib.o $(LIBS)

macosx-x86_64-debug/obj/manager.o: \
        src/deps/mpr/manager.c \
        macosx-x86_64-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o macosx-x86_64-debug/obj/manager.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/deps/mpr/manager.c

macosx-x86_64-debug/bin/manager:  \
        macosx-x86_64-debug/lib/libmpr.dylib \
        macosx-x86_64-debug/obj/manager.o
	$(CC) -o macosx-x86_64-debug/bin/manager -arch x86_64 $(LDFLAGS) -Lmacosx-x86_64-debug/lib macosx-x86_64-debug/obj/manager.o $(LIBS) -lmpr

macosx-x86_64-debug/obj/makerom.o: \
        src/deps/mpr/makerom.c \
        macosx-x86_64-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o macosx-x86_64-debug/obj/makerom.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/deps/mpr/makerom.c

macosx-x86_64-debug/bin/makerom:  \
        macosx-x86_64-debug/lib/libmpr.dylib \
        macosx-x86_64-debug/obj/makerom.o
	$(CC) -o macosx-x86_64-debug/bin/makerom -arch x86_64 $(LDFLAGS) -Lmacosx-x86_64-debug/lib macosx-x86_64-debug/obj/makerom.o $(LIBS) -lmpr

macosx-x86_64-debug/obj/pcre.o: \
        src/deps/pcre/pcre.c \
        macosx-x86_64-debug/inc/bit.h \
        macosx-x86_64-debug/inc/buildConfig.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o macosx-x86_64-debug/obj/pcre.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/deps/pcre/pcre.c

macosx-x86_64-debug/lib/libpcre.dylib:  \
        macosx-x86_64-debug/obj/pcre.o
	$(CC) -dynamiclib -o macosx-x86_64-debug/lib/libpcre.dylib -arch x86_64 $(LDFLAGS) -install_name @rpath/libpcre.dylib macosx-x86_64-debug/obj/pcre.o $(LIBS)

macosx-x86_64-debug/obj/auth.o: \
        src/auth.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/auth.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/auth.c

macosx-x86_64-debug/obj/authCheck.o: \
        src/authCheck.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/authCheck.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/authCheck.c

macosx-x86_64-debug/obj/authFile.o: \
        src/authFile.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/authFile.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/authFile.c

macosx-x86_64-debug/obj/authPam.o: \
        src/authPam.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/authPam.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/authPam.c

macosx-x86_64-debug/obj/cache.o: \
        src/cache.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/cache.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/cache.c

macosx-x86_64-debug/obj/chunkFilter.o: \
        src/chunkFilter.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/chunkFilter.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/chunkFilter.c

macosx-x86_64-debug/obj/client.o: \
        src/client.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/client.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/client.c

macosx-x86_64-debug/obj/conn.o: \
        src/conn.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/conn.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/conn.c

macosx-x86_64-debug/obj/endpoint.o: \
        src/endpoint.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/endpoint.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/endpoint.c

macosx-x86_64-debug/obj/error.o: \
        src/error.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/error.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/error.c

macosx-x86_64-debug/obj/host.o: \
        src/host.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/host.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/host.c

macosx-x86_64-debug/obj/httpService.o: \
        src/httpService.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/httpService.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/httpService.c

macosx-x86_64-debug/obj/log.o: \
        src/log.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/log.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/log.c

macosx-x86_64-debug/obj/netConnector.o: \
        src/netConnector.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/netConnector.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/netConnector.c

macosx-x86_64-debug/obj/packet.o: \
        src/packet.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/packet.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/packet.c

macosx-x86_64-debug/obj/passHandler.o: \
        src/passHandler.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/passHandler.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/passHandler.c

macosx-x86_64-debug/obj/pipeline.o: \
        src/pipeline.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/pipeline.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/pipeline.c

macosx-x86_64-debug/obj/queue.o: \
        src/queue.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/queue.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/queue.c

macosx-x86_64-debug/obj/rangeFilter.o: \
        src/rangeFilter.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/rangeFilter.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/rangeFilter.c

macosx-x86_64-debug/obj/route.o: \
        src/route.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o macosx-x86_64-debug/obj/route.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/route.c

macosx-x86_64-debug/obj/rx.o: \
        src/rx.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/rx.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/rx.c

macosx-x86_64-debug/obj/sendConnector.o: \
        src/sendConnector.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/sendConnector.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/sendConnector.c

macosx-x86_64-debug/obj/stage.o: \
        src/stage.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/stage.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/stage.c

macosx-x86_64-debug/obj/trace.o: \
        src/trace.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/trace.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/trace.c

macosx-x86_64-debug/obj/tx.o: \
        src/tx.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/tx.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/tx.c

macosx-x86_64-debug/obj/uploadFilter.o: \
        src/uploadFilter.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/uploadFilter.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/uploadFilter.c

macosx-x86_64-debug/obj/uri.o: \
        src/uri.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/uri.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/uri.c

macosx-x86_64-debug/obj/var.o: \
        src/var.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/var.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/var.c

macosx-x86_64-debug/lib/libhttp.dylib:  \
        macosx-x86_64-debug/lib/libmpr.dylib \
        macosx-x86_64-debug/lib/libpcre.dylib \
        macosx-x86_64-debug/obj/auth.o \
        macosx-x86_64-debug/obj/authCheck.o \
        macosx-x86_64-debug/obj/authFile.o \
        macosx-x86_64-debug/obj/authPam.o \
        macosx-x86_64-debug/obj/cache.o \
        macosx-x86_64-debug/obj/chunkFilter.o \
        macosx-x86_64-debug/obj/client.o \
        macosx-x86_64-debug/obj/conn.o \
        macosx-x86_64-debug/obj/endpoint.o \
        macosx-x86_64-debug/obj/error.o \
        macosx-x86_64-debug/obj/host.o \
        macosx-x86_64-debug/obj/httpService.o \
        macosx-x86_64-debug/obj/log.o \
        macosx-x86_64-debug/obj/netConnector.o \
        macosx-x86_64-debug/obj/packet.o \
        macosx-x86_64-debug/obj/passHandler.o \
        macosx-x86_64-debug/obj/pipeline.o \
        macosx-x86_64-debug/obj/queue.o \
        macosx-x86_64-debug/obj/rangeFilter.o \
        macosx-x86_64-debug/obj/route.o \
        macosx-x86_64-debug/obj/rx.o \
        macosx-x86_64-debug/obj/sendConnector.o \
        macosx-x86_64-debug/obj/stage.o \
        macosx-x86_64-debug/obj/trace.o \
        macosx-x86_64-debug/obj/tx.o \
        macosx-x86_64-debug/obj/uploadFilter.o \
        macosx-x86_64-debug/obj/uri.o \
        macosx-x86_64-debug/obj/var.o
	$(CC) -dynamiclib -o macosx-x86_64-debug/lib/libhttp.dylib -arch x86_64 $(LDFLAGS) -install_name @rpath/libhttp.dylib macosx-x86_64-debug/obj/auth.o macosx-x86_64-debug/obj/authCheck.o macosx-x86_64-debug/obj/authFile.o macosx-x86_64-debug/obj/authPam.o macosx-x86_64-debug/obj/cache.o macosx-x86_64-debug/obj/chunkFilter.o macosx-x86_64-debug/obj/client.o macosx-x86_64-debug/obj/conn.o macosx-x86_64-debug/obj/endpoint.o macosx-x86_64-debug/obj/error.o macosx-x86_64-debug/obj/host.o macosx-x86_64-debug/obj/httpService.o macosx-x86_64-debug/obj/log.o macosx-x86_64-debug/obj/netConnector.o macosx-x86_64-debug/obj/packet.o macosx-x86_64-debug/obj/passHandler.o macosx-x86_64-debug/obj/pipeline.o macosx-x86_64-debug/obj/queue.o macosx-x86_64-debug/obj/rangeFilter.o macosx-x86_64-debug/obj/route.o macosx-x86_64-debug/obj/rx.o macosx-x86_64-debug/obj/sendConnector.o macosx-x86_64-debug/obj/stage.o macosx-x86_64-debug/obj/trace.o macosx-x86_64-debug/obj/tx.o macosx-x86_64-debug/obj/uploadFilter.o macosx-x86_64-debug/obj/uri.o macosx-x86_64-debug/obj/var.o $(LIBS) -lmpr -lpcre

macosx-x86_64-debug/obj/http.o: \
        src/utils/http.c \
        macosx-x86_64-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-x86_64-debug/obj/http.o -arch x86_64 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/utils/http.c

macosx-x86_64-debug/bin/http:  \
        macosx-x86_64-debug/lib/libhttp.dylib \
        macosx-x86_64-debug/obj/http.o
	$(CC) -o macosx-x86_64-debug/bin/http -arch x86_64 $(LDFLAGS) -Lmacosx-x86_64-debug/lib macosx-x86_64-debug/obj/http.o $(LIBS) -lhttp -lmpr -lpcre

