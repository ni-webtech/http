#
#   build.mk -- Build It Makefile to build Http Library for macosx on i686
#

CC        := cc
CFLAGS    := -fPIC -Wall -g
DFLAGS    := -DPIC -DCPU=I686
IFLAGS    := -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc
LDFLAGS   := -Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -L/Users/mob/git/http/macosx-i686-debug/lib -g
LIBS      := -lpthread -lm

all: \
        macosx-i686-debug/lib/libmpr.dylib \
        macosx-i686-debug/bin/manager \
        macosx-i686-debug/bin/makerom \
        macosx-i686-debug/lib/libpcre.dylib \
        macosx-i686-debug/lib/libhttp.dylib \
        macosx-i686-debug/bin/http

clean:
	rm -f macosx-i686-debug/lib/libmpr.dylib
	rm -f macosx-i686-debug/lib/libmprssl.dylib
	rm -f macosx-i686-debug/bin/manager
	rm -f macosx-i686-debug/bin/makerom
	rm -f macosx-i686-debug/lib/libpcre.dylib
	rm -f macosx-i686-debug/lib/libhttp.dylib
	rm -f macosx-i686-debug/bin/http
	rm -f macosx-i686-debug/obj/mprLib.o
	rm -f macosx-i686-debug/obj/mprSsl.o
	rm -f macosx-i686-debug/obj/manager.o
	rm -f macosx-i686-debug/obj/makerom.o
	rm -f macosx-i686-debug/obj/pcre.o
	rm -f macosx-i686-debug/obj/auth.o
	rm -f macosx-i686-debug/obj/authCheck.o
	rm -f macosx-i686-debug/obj/authFile.o
	rm -f macosx-i686-debug/obj/authPam.o
	rm -f macosx-i686-debug/obj/cache.o
	rm -f macosx-i686-debug/obj/chunkFilter.o
	rm -f macosx-i686-debug/obj/client.o
	rm -f macosx-i686-debug/obj/conn.o
	rm -f macosx-i686-debug/obj/endpoint.o
	rm -f macosx-i686-debug/obj/error.o
	rm -f macosx-i686-debug/obj/host.o
	rm -f macosx-i686-debug/obj/httpService.o
	rm -f macosx-i686-debug/obj/log.o
	rm -f macosx-i686-debug/obj/netConnector.o
	rm -f macosx-i686-debug/obj/packet.o
	rm -f macosx-i686-debug/obj/passHandler.o
	rm -f macosx-i686-debug/obj/pipeline.o
	rm -f macosx-i686-debug/obj/queue.o
	rm -f macosx-i686-debug/obj/rangeFilter.o
	rm -f macosx-i686-debug/obj/route.o
	rm -f macosx-i686-debug/obj/rx.o
	rm -f macosx-i686-debug/obj/sendConnector.o
	rm -f macosx-i686-debug/obj/stage.o
	rm -f macosx-i686-debug/obj/trace.o
	rm -f macosx-i686-debug/obj/tx.o
	rm -f macosx-i686-debug/obj/uploadFilter.o
	rm -f macosx-i686-debug/obj/uri.o
	rm -f macosx-i686-debug/obj/var.o
	rm -f macosx-i686-debug/obj/http.o

macosx-i686-debug/obj/mprLib.o: \
        src/deps/mpr/mprLib.c \
        macosx-i686-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o macosx-i686-debug/obj/mprLib.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/deps/mpr/mprLib.c

macosx-i686-debug/lib/libmpr.dylib:  \
        macosx-i686-debug/obj/mprLib.o
	$(CC) -dynamiclib -o macosx-i686-debug/lib/libmpr.dylib -arch i686 $(LDFLAGS) -install_name @rpath/libmpr.dylib macosx-i686-debug/obj/mprLib.o $(LIBS)

macosx-i686-debug/obj/manager.o: \
        src/deps/mpr/manager.c \
        macosx-i686-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o macosx-i686-debug/obj/manager.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/deps/mpr/manager.c

macosx-i686-debug/bin/manager:  \
        macosx-i686-debug/lib/libmpr.dylib \
        macosx-i686-debug/obj/manager.o
	$(CC) -o macosx-i686-debug/bin/manager -arch i686 $(LDFLAGS) -Lmacosx-i686-debug/lib macosx-i686-debug/obj/manager.o $(LIBS) -lmpr

macosx-i686-debug/obj/makerom.o: \
        src/deps/mpr/makerom.c \
        macosx-i686-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o macosx-i686-debug/obj/makerom.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/deps/mpr/makerom.c

macosx-i686-debug/bin/makerom:  \
        macosx-i686-debug/lib/libmpr.dylib \
        macosx-i686-debug/obj/makerom.o
	$(CC) -o macosx-i686-debug/bin/makerom -arch i686 $(LDFLAGS) -Lmacosx-i686-debug/lib macosx-i686-debug/obj/makerom.o $(LIBS) -lmpr

macosx-i686-debug/obj/pcre.o: \
        src/deps/pcre/pcre.c \
        macosx-i686-debug/inc/bit.h \
        macosx-i686-debug/inc/buildConfig.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o macosx-i686-debug/obj/pcre.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/deps/pcre/pcre.c

macosx-i686-debug/lib/libpcre.dylib:  \
        macosx-i686-debug/obj/pcre.o
	$(CC) -dynamiclib -o macosx-i686-debug/lib/libpcre.dylib -arch i686 $(LDFLAGS) -install_name @rpath/libpcre.dylib macosx-i686-debug/obj/pcre.o $(LIBS)

macosx-i686-debug/obj/auth.o: \
        src/auth.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/auth.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/auth.c

macosx-i686-debug/obj/authCheck.o: \
        src/authCheck.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/authCheck.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/authCheck.c

macosx-i686-debug/obj/authFile.o: \
        src/authFile.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/authFile.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/authFile.c

macosx-i686-debug/obj/authPam.o: \
        src/authPam.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/authPam.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/authPam.c

macosx-i686-debug/obj/cache.o: \
        src/cache.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/cache.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/cache.c

macosx-i686-debug/obj/chunkFilter.o: \
        src/chunkFilter.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/chunkFilter.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/chunkFilter.c

macosx-i686-debug/obj/client.o: \
        src/client.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/client.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/client.c

macosx-i686-debug/obj/conn.o: \
        src/conn.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/conn.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/conn.c

macosx-i686-debug/obj/endpoint.o: \
        src/endpoint.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/endpoint.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/endpoint.c

macosx-i686-debug/obj/error.o: \
        src/error.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/error.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/error.c

macosx-i686-debug/obj/host.o: \
        src/host.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/host.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/host.c

macosx-i686-debug/obj/httpService.o: \
        src/httpService.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/httpService.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/httpService.c

macosx-i686-debug/obj/log.o: \
        src/log.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/log.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/log.c

macosx-i686-debug/obj/netConnector.o: \
        src/netConnector.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/netConnector.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/netConnector.c

macosx-i686-debug/obj/packet.o: \
        src/packet.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/packet.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/packet.c

macosx-i686-debug/obj/passHandler.o: \
        src/passHandler.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/passHandler.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/passHandler.c

macosx-i686-debug/obj/pipeline.o: \
        src/pipeline.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/pipeline.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/pipeline.c

macosx-i686-debug/obj/queue.o: \
        src/queue.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/queue.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/queue.c

macosx-i686-debug/obj/rangeFilter.o: \
        src/rangeFilter.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/rangeFilter.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/rangeFilter.c

macosx-i686-debug/obj/route.o: \
        src/route.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o macosx-i686-debug/obj/route.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/route.c

macosx-i686-debug/obj/rx.o: \
        src/rx.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/rx.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/rx.c

macosx-i686-debug/obj/sendConnector.o: \
        src/sendConnector.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/sendConnector.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/sendConnector.c

macosx-i686-debug/obj/stage.o: \
        src/stage.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/stage.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/stage.c

macosx-i686-debug/obj/trace.o: \
        src/trace.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/trace.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/trace.c

macosx-i686-debug/obj/tx.o: \
        src/tx.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/tx.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/tx.c

macosx-i686-debug/obj/uploadFilter.o: \
        src/uploadFilter.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/uploadFilter.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/uploadFilter.c

macosx-i686-debug/obj/uri.o: \
        src/uri.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/uri.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/uri.c

macosx-i686-debug/obj/var.o: \
        src/var.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/var.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/var.c

macosx-i686-debug/lib/libhttp.dylib:  \
        macosx-i686-debug/lib/libmpr.dylib \
        macosx-i686-debug/lib/libpcre.dylib \
        macosx-i686-debug/obj/auth.o \
        macosx-i686-debug/obj/authCheck.o \
        macosx-i686-debug/obj/authFile.o \
        macosx-i686-debug/obj/authPam.o \
        macosx-i686-debug/obj/cache.o \
        macosx-i686-debug/obj/chunkFilter.o \
        macosx-i686-debug/obj/client.o \
        macosx-i686-debug/obj/conn.o \
        macosx-i686-debug/obj/endpoint.o \
        macosx-i686-debug/obj/error.o \
        macosx-i686-debug/obj/host.o \
        macosx-i686-debug/obj/httpService.o \
        macosx-i686-debug/obj/log.o \
        macosx-i686-debug/obj/netConnector.o \
        macosx-i686-debug/obj/packet.o \
        macosx-i686-debug/obj/passHandler.o \
        macosx-i686-debug/obj/pipeline.o \
        macosx-i686-debug/obj/queue.o \
        macosx-i686-debug/obj/rangeFilter.o \
        macosx-i686-debug/obj/route.o \
        macosx-i686-debug/obj/rx.o \
        macosx-i686-debug/obj/sendConnector.o \
        macosx-i686-debug/obj/stage.o \
        macosx-i686-debug/obj/trace.o \
        macosx-i686-debug/obj/tx.o \
        macosx-i686-debug/obj/uploadFilter.o \
        macosx-i686-debug/obj/uri.o \
        macosx-i686-debug/obj/var.o
	$(CC) -dynamiclib -o macosx-i686-debug/lib/libhttp.dylib -arch i686 $(LDFLAGS) -install_name @rpath/libhttp.dylib macosx-i686-debug/obj/auth.o macosx-i686-debug/obj/authCheck.o macosx-i686-debug/obj/authFile.o macosx-i686-debug/obj/authPam.o macosx-i686-debug/obj/cache.o macosx-i686-debug/obj/chunkFilter.o macosx-i686-debug/obj/client.o macosx-i686-debug/obj/conn.o macosx-i686-debug/obj/endpoint.o macosx-i686-debug/obj/error.o macosx-i686-debug/obj/host.o macosx-i686-debug/obj/httpService.o macosx-i686-debug/obj/log.o macosx-i686-debug/obj/netConnector.o macosx-i686-debug/obj/packet.o macosx-i686-debug/obj/passHandler.o macosx-i686-debug/obj/pipeline.o macosx-i686-debug/obj/queue.o macosx-i686-debug/obj/rangeFilter.o macosx-i686-debug/obj/route.o macosx-i686-debug/obj/rx.o macosx-i686-debug/obj/sendConnector.o macosx-i686-debug/obj/stage.o macosx-i686-debug/obj/trace.o macosx-i686-debug/obj/tx.o macosx-i686-debug/obj/uploadFilter.o macosx-i686-debug/obj/uri.o macosx-i686-debug/obj/var.o $(LIBS) -lmpr -lpcre

macosx-i686-debug/obj/http.o: \
        src/utils/http.c \
        macosx-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o macosx-i686-debug/obj/http.o -arch i686 $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/utils/http.c

macosx-i686-debug/bin/http:  \
        macosx-i686-debug/lib/libhttp.dylib \
        macosx-i686-debug/obj/http.o
	$(CC) -o macosx-i686-debug/bin/http -arch i686 $(LDFLAGS) -Lmacosx-i686-debug/lib macosx-i686-debug/obj/http.o $(LIBS) -lhttp -lmpr -lpcre

