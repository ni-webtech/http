#
#   build.mk -- Build It Makefile to build Http Library for solaris on i686
#

CC        := cc
CFLAGS    := -fPIC -g -mcpu=i686
DFLAGS    := -DPIC
IFLAGS    := -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc
LDFLAGS   := -L/Users/mob/git/http/solaris-i686-debug/lib -g
LIBS      := -lpthread -lm

all: \
        solaris-i686-debug/lib/libmpr.so \
        solaris-i686-debug/bin/manager \
        solaris-i686-debug/bin/makerom \
        solaris-i686-debug/lib/libpcre.so \
        solaris-i686-debug/lib/libhttp.so \
        solaris-i686-debug/bin/http

clean:
	rm -f solaris-i686-debug/lib/libmpr.so
	rm -f solaris-i686-debug/lib/libmprssl.so
	rm -f solaris-i686-debug/bin/manager
	rm -f solaris-i686-debug/bin/makerom
	rm -f solaris-i686-debug/lib/libpcre.so
	rm -f solaris-i686-debug/lib/libhttp.so
	rm -f solaris-i686-debug/bin/http
	rm -f solaris-i686-debug/obj/mprLib.o
	rm -f solaris-i686-debug/obj/mprSsl.o
	rm -f solaris-i686-debug/obj/manager.o
	rm -f solaris-i686-debug/obj/makerom.o
	rm -f solaris-i686-debug/obj/pcre.o
	rm -f solaris-i686-debug/obj/auth.o
	rm -f solaris-i686-debug/obj/authCheck.o
	rm -f solaris-i686-debug/obj/authFile.o
	rm -f solaris-i686-debug/obj/authPam.o
	rm -f solaris-i686-debug/obj/cache.o
	rm -f solaris-i686-debug/obj/chunkFilter.o
	rm -f solaris-i686-debug/obj/client.o
	rm -f solaris-i686-debug/obj/conn.o
	rm -f solaris-i686-debug/obj/endpoint.o
	rm -f solaris-i686-debug/obj/error.o
	rm -f solaris-i686-debug/obj/host.o
	rm -f solaris-i686-debug/obj/httpService.o
	rm -f solaris-i686-debug/obj/log.o
	rm -f solaris-i686-debug/obj/netConnector.o
	rm -f solaris-i686-debug/obj/packet.o
	rm -f solaris-i686-debug/obj/passHandler.o
	rm -f solaris-i686-debug/obj/pipeline.o
	rm -f solaris-i686-debug/obj/queue.o
	rm -f solaris-i686-debug/obj/rangeFilter.o
	rm -f solaris-i686-debug/obj/route.o
	rm -f solaris-i686-debug/obj/rx.o
	rm -f solaris-i686-debug/obj/sendConnector.o
	rm -f solaris-i686-debug/obj/stage.o
	rm -f solaris-i686-debug/obj/trace.o
	rm -f solaris-i686-debug/obj/tx.o
	rm -f solaris-i686-debug/obj/uploadFilter.o
	rm -f solaris-i686-debug/obj/uri.o
	rm -f solaris-i686-debug/obj/var.o
	rm -f solaris-i686-debug/obj/http.o

solaris-i686-debug/obj/mprLib.o: \
        src/deps/mpr/mprLib.c \
        solaris-i686-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o solaris-i686-debug/obj/mprLib.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/deps/mpr/mprLib.c

solaris-i686-debug/lib/libmpr.so:  \
        solaris-i686-debug/obj/mprLib.o
	$(CC) -shared -o solaris-i686-debug/lib/libmpr.so $(LDFLAGS) solaris-i686-debug/obj/mprLib.o $(LIBS)

solaris-i686-debug/obj/manager.o: \
        src/deps/mpr/manager.c \
        solaris-i686-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o solaris-i686-debug/obj/manager.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/deps/mpr/manager.c

solaris-i686-debug/bin/manager:  \
        solaris-i686-debug/lib/libmpr.so \
        solaris-i686-debug/obj/manager.o
	$(CC) -o solaris-i686-debug/bin/manager $(LDFLAGS) -Lsolaris-i686-debug/lib solaris-i686-debug/obj/manager.o $(LIBS) -lmpr -Lsolaris-i686-debug/lib -g

solaris-i686-debug/obj/makerom.o: \
        src/deps/mpr/makerom.c \
        solaris-i686-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o solaris-i686-debug/obj/makerom.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/deps/mpr/makerom.c

solaris-i686-debug/bin/makerom:  \
        solaris-i686-debug/lib/libmpr.so \
        solaris-i686-debug/obj/makerom.o
	$(CC) -o solaris-i686-debug/bin/makerom $(LDFLAGS) -Lsolaris-i686-debug/lib solaris-i686-debug/obj/makerom.o $(LIBS) -lmpr -Lsolaris-i686-debug/lib -g

solaris-i686-debug/obj/pcre.o: \
        src/deps/pcre/pcre.c \
        solaris-i686-debug/inc/bit.h \
        solaris-i686-debug/inc/buildConfig.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o solaris-i686-debug/obj/pcre.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/deps/pcre/pcre.c

solaris-i686-debug/lib/libpcre.so:  \
        solaris-i686-debug/obj/pcre.o
	$(CC) -shared -o solaris-i686-debug/lib/libpcre.so $(LDFLAGS) solaris-i686-debug/obj/pcre.o $(LIBS)

solaris-i686-debug/obj/auth.o: \
        src/auth.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/auth.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/auth.c

solaris-i686-debug/obj/authCheck.o: \
        src/authCheck.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/authCheck.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/authCheck.c

solaris-i686-debug/obj/authFile.o: \
        src/authFile.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/authFile.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/authFile.c

solaris-i686-debug/obj/authPam.o: \
        src/authPam.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/authPam.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/authPam.c

solaris-i686-debug/obj/cache.o: \
        src/cache.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/cache.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/cache.c

solaris-i686-debug/obj/chunkFilter.o: \
        src/chunkFilter.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/chunkFilter.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/chunkFilter.c

solaris-i686-debug/obj/client.o: \
        src/client.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/client.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/client.c

solaris-i686-debug/obj/conn.o: \
        src/conn.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/conn.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/conn.c

solaris-i686-debug/obj/endpoint.o: \
        src/endpoint.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/endpoint.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/endpoint.c

solaris-i686-debug/obj/error.o: \
        src/error.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/error.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/error.c

solaris-i686-debug/obj/host.o: \
        src/host.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/host.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/host.c

solaris-i686-debug/obj/httpService.o: \
        src/httpService.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/httpService.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/httpService.c

solaris-i686-debug/obj/log.o: \
        src/log.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/log.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/log.c

solaris-i686-debug/obj/netConnector.o: \
        src/netConnector.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/netConnector.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/netConnector.c

solaris-i686-debug/obj/packet.o: \
        src/packet.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/packet.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/packet.c

solaris-i686-debug/obj/passHandler.o: \
        src/passHandler.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/passHandler.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/passHandler.c

solaris-i686-debug/obj/pipeline.o: \
        src/pipeline.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/pipeline.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/pipeline.c

solaris-i686-debug/obj/queue.o: \
        src/queue.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/queue.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/queue.c

solaris-i686-debug/obj/rangeFilter.o: \
        src/rangeFilter.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/rangeFilter.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/rangeFilter.c

solaris-i686-debug/obj/route.o: \
        src/route.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o solaris-i686-debug/obj/route.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/route.c

solaris-i686-debug/obj/rx.o: \
        src/rx.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/rx.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/rx.c

solaris-i686-debug/obj/sendConnector.o: \
        src/sendConnector.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/sendConnector.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/sendConnector.c

solaris-i686-debug/obj/stage.o: \
        src/stage.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/stage.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/stage.c

solaris-i686-debug/obj/trace.o: \
        src/trace.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/trace.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/trace.c

solaris-i686-debug/obj/tx.o: \
        src/tx.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/tx.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/tx.c

solaris-i686-debug/obj/uploadFilter.o: \
        src/uploadFilter.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/uploadFilter.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/uploadFilter.c

solaris-i686-debug/obj/uri.o: \
        src/uri.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/uri.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/uri.c

solaris-i686-debug/obj/var.o: \
        src/var.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/var.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/var.c

solaris-i686-debug/lib/libhttp.so:  \
        solaris-i686-debug/lib/libmpr.so \
        solaris-i686-debug/lib/libpcre.so \
        solaris-i686-debug/obj/auth.o \
        solaris-i686-debug/obj/authCheck.o \
        solaris-i686-debug/obj/authFile.o \
        solaris-i686-debug/obj/authPam.o \
        solaris-i686-debug/obj/cache.o \
        solaris-i686-debug/obj/chunkFilter.o \
        solaris-i686-debug/obj/client.o \
        solaris-i686-debug/obj/conn.o \
        solaris-i686-debug/obj/endpoint.o \
        solaris-i686-debug/obj/error.o \
        solaris-i686-debug/obj/host.o \
        solaris-i686-debug/obj/httpService.o \
        solaris-i686-debug/obj/log.o \
        solaris-i686-debug/obj/netConnector.o \
        solaris-i686-debug/obj/packet.o \
        solaris-i686-debug/obj/passHandler.o \
        solaris-i686-debug/obj/pipeline.o \
        solaris-i686-debug/obj/queue.o \
        solaris-i686-debug/obj/rangeFilter.o \
        solaris-i686-debug/obj/route.o \
        solaris-i686-debug/obj/rx.o \
        solaris-i686-debug/obj/sendConnector.o \
        solaris-i686-debug/obj/stage.o \
        solaris-i686-debug/obj/trace.o \
        solaris-i686-debug/obj/tx.o \
        solaris-i686-debug/obj/uploadFilter.o \
        solaris-i686-debug/obj/uri.o \
        solaris-i686-debug/obj/var.o
	$(CC) -shared -o solaris-i686-debug/lib/libhttp.so $(LDFLAGS) solaris-i686-debug/obj/auth.o solaris-i686-debug/obj/authCheck.o solaris-i686-debug/obj/authFile.o solaris-i686-debug/obj/authPam.o solaris-i686-debug/obj/cache.o solaris-i686-debug/obj/chunkFilter.o solaris-i686-debug/obj/client.o solaris-i686-debug/obj/conn.o solaris-i686-debug/obj/endpoint.o solaris-i686-debug/obj/error.o solaris-i686-debug/obj/host.o solaris-i686-debug/obj/httpService.o solaris-i686-debug/obj/log.o solaris-i686-debug/obj/netConnector.o solaris-i686-debug/obj/packet.o solaris-i686-debug/obj/passHandler.o solaris-i686-debug/obj/pipeline.o solaris-i686-debug/obj/queue.o solaris-i686-debug/obj/rangeFilter.o solaris-i686-debug/obj/route.o solaris-i686-debug/obj/rx.o solaris-i686-debug/obj/sendConnector.o solaris-i686-debug/obj/stage.o solaris-i686-debug/obj/trace.o solaris-i686-debug/obj/tx.o solaris-i686-debug/obj/uploadFilter.o solaris-i686-debug/obj/uri.o solaris-i686-debug/obj/var.o $(LIBS) -lmpr -lpcre

solaris-i686-debug/obj/http.o: \
        src/utils/http.c \
        solaris-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o solaris-i686-debug/obj/http.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/utils/http.c

solaris-i686-debug/bin/http:  \
        solaris-i686-debug/lib/libhttp.so \
        solaris-i686-debug/obj/http.o
	$(CC) -o solaris-i686-debug/bin/http $(LDFLAGS) -Lsolaris-i686-debug/lib solaris-i686-debug/obj/http.o $(LIBS) -lhttp -lmpr -lpcre -Lsolaris-i686-debug/lib -g

