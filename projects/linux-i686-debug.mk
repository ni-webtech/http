#
#   build.mk -- Build It Makefile to build Http Library for linux on i686
#

CC        := cc
CFLAGS    := -fPIC -g -mcpu=i686
DFLAGS    := -DPIC
IFLAGS    := -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc
LDFLAGS   := -L/Users/mob/git/http/linux-i686-debug/lib -g
LIBS      := -lpthread -lm

all: \
        linux-i686-debug/lib/libmpr.so \
        linux-i686-debug/bin/manager \
        linux-i686-debug/bin/makerom \
        linux-i686-debug/lib/libpcre.so \
        linux-i686-debug/lib/libhttp.so \
        linux-i686-debug/bin/http

clean:
	rm -f linux-i686-debug/lib/libmpr.so
	rm -f linux-i686-debug/lib/libmprssl.so
	rm -f linux-i686-debug/bin/manager
	rm -f linux-i686-debug/bin/makerom
	rm -f linux-i686-debug/lib/libpcre.so
	rm -f linux-i686-debug/lib/libhttp.so
	rm -f linux-i686-debug/bin/http
	rm -f linux-i686-debug/obj/mprLib.o
	rm -f linux-i686-debug/obj/mprSsl.o
	rm -f linux-i686-debug/obj/manager.o
	rm -f linux-i686-debug/obj/makerom.o
	rm -f linux-i686-debug/obj/pcre.o
	rm -f linux-i686-debug/obj/auth.o
	rm -f linux-i686-debug/obj/authCheck.o
	rm -f linux-i686-debug/obj/authFile.o
	rm -f linux-i686-debug/obj/authPam.o
	rm -f linux-i686-debug/obj/cache.o
	rm -f linux-i686-debug/obj/chunkFilter.o
	rm -f linux-i686-debug/obj/client.o
	rm -f linux-i686-debug/obj/conn.o
	rm -f linux-i686-debug/obj/endpoint.o
	rm -f linux-i686-debug/obj/error.o
	rm -f linux-i686-debug/obj/host.o
	rm -f linux-i686-debug/obj/httpService.o
	rm -f linux-i686-debug/obj/log.o
	rm -f linux-i686-debug/obj/netConnector.o
	rm -f linux-i686-debug/obj/packet.o
	rm -f linux-i686-debug/obj/passHandler.o
	rm -f linux-i686-debug/obj/pipeline.o
	rm -f linux-i686-debug/obj/queue.o
	rm -f linux-i686-debug/obj/rangeFilter.o
	rm -f linux-i686-debug/obj/route.o
	rm -f linux-i686-debug/obj/rx.o
	rm -f linux-i686-debug/obj/sendConnector.o
	rm -f linux-i686-debug/obj/stage.o
	rm -f linux-i686-debug/obj/trace.o
	rm -f linux-i686-debug/obj/tx.o
	rm -f linux-i686-debug/obj/uploadFilter.o
	rm -f linux-i686-debug/obj/uri.o
	rm -f linux-i686-debug/obj/var.o
	rm -f linux-i686-debug/obj/http.o

linux-i686-debug/obj/mprLib.o: \
        src/deps/mpr/mprLib.c \
        linux-i686-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o linux-i686-debug/obj/mprLib.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/deps/mpr/mprLib.c

linux-i686-debug/lib/libmpr.so:  \
        linux-i686-debug/obj/mprLib.o
	$(CC) -shared -o linux-i686-debug/lib/libmpr.so $(LDFLAGS) linux-i686-debug/obj/mprLib.o $(LIBS)

linux-i686-debug/obj/manager.o: \
        src/deps/mpr/manager.c \
        linux-i686-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o linux-i686-debug/obj/manager.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/deps/mpr/manager.c

linux-i686-debug/bin/manager:  \
        linux-i686-debug/lib/libmpr.so \
        linux-i686-debug/obj/manager.o
	$(CC) -o linux-i686-debug/bin/manager $(LDFLAGS) -Llinux-i686-debug/lib linux-i686-debug/obj/manager.o $(LIBS) -lmpr -Llinux-i686-debug/lib -g

linux-i686-debug/obj/makerom.o: \
        src/deps/mpr/makerom.c \
        linux-i686-debug/inc/bit.h \
        src/deps/mpr/mpr.h
	$(CC) -c -o linux-i686-debug/obj/makerom.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/deps/mpr/makerom.c

linux-i686-debug/bin/makerom:  \
        linux-i686-debug/lib/libmpr.so \
        linux-i686-debug/obj/makerom.o
	$(CC) -o linux-i686-debug/bin/makerom $(LDFLAGS) -Llinux-i686-debug/lib linux-i686-debug/obj/makerom.o $(LIBS) -lmpr -Llinux-i686-debug/lib -g

linux-i686-debug/obj/pcre.o: \
        src/deps/pcre/pcre.c \
        linux-i686-debug/inc/bit.h \
        linux-i686-debug/inc/buildConfig.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o linux-i686-debug/obj/pcre.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/deps/pcre/pcre.c

linux-i686-debug/lib/libpcre.so:  \
        linux-i686-debug/obj/pcre.o
	$(CC) -shared -o linux-i686-debug/lib/libpcre.so $(LDFLAGS) linux-i686-debug/obj/pcre.o $(LIBS)

linux-i686-debug/obj/auth.o: \
        src/auth.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/auth.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/auth.c

linux-i686-debug/obj/authCheck.o: \
        src/authCheck.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/authCheck.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/authCheck.c

linux-i686-debug/obj/authFile.o: \
        src/authFile.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/authFile.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/authFile.c

linux-i686-debug/obj/authPam.o: \
        src/authPam.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/authPam.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/authPam.c

linux-i686-debug/obj/cache.o: \
        src/cache.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/cache.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/cache.c

linux-i686-debug/obj/chunkFilter.o: \
        src/chunkFilter.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/chunkFilter.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/chunkFilter.c

linux-i686-debug/obj/client.o: \
        src/client.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/client.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/client.c

linux-i686-debug/obj/conn.o: \
        src/conn.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/conn.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/conn.c

linux-i686-debug/obj/endpoint.o: \
        src/endpoint.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/endpoint.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/endpoint.c

linux-i686-debug/obj/error.o: \
        src/error.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/error.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/error.c

linux-i686-debug/obj/host.o: \
        src/host.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/host.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/host.c

linux-i686-debug/obj/httpService.o: \
        src/httpService.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/httpService.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/httpService.c

linux-i686-debug/obj/log.o: \
        src/log.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/log.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/log.c

linux-i686-debug/obj/netConnector.o: \
        src/netConnector.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/netConnector.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/netConnector.c

linux-i686-debug/obj/packet.o: \
        src/packet.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/packet.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/packet.c

linux-i686-debug/obj/passHandler.o: \
        src/passHandler.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/passHandler.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/passHandler.c

linux-i686-debug/obj/pipeline.o: \
        src/pipeline.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/pipeline.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/pipeline.c

linux-i686-debug/obj/queue.o: \
        src/queue.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/queue.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/queue.c

linux-i686-debug/obj/rangeFilter.o: \
        src/rangeFilter.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/rangeFilter.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/rangeFilter.c

linux-i686-debug/obj/route.o: \
        src/route.c \
        linux-i686-debug/inc/bit.h \
        src/http.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o linux-i686-debug/obj/route.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/route.c

linux-i686-debug/obj/rx.o: \
        src/rx.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/rx.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/rx.c

linux-i686-debug/obj/sendConnector.o: \
        src/sendConnector.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/sendConnector.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/sendConnector.c

linux-i686-debug/obj/stage.o: \
        src/stage.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/stage.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/stage.c

linux-i686-debug/obj/trace.o: \
        src/trace.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/trace.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/trace.c

linux-i686-debug/obj/tx.o: \
        src/tx.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/tx.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/tx.c

linux-i686-debug/obj/uploadFilter.o: \
        src/uploadFilter.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/uploadFilter.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/uploadFilter.c

linux-i686-debug/obj/uri.o: \
        src/uri.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/uri.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/uri.c

linux-i686-debug/obj/var.o: \
        src/var.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/var.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/var.c

linux-i686-debug/lib/libhttp.so:  \
        linux-i686-debug/lib/libmpr.so \
        linux-i686-debug/lib/libpcre.so \
        linux-i686-debug/obj/auth.o \
        linux-i686-debug/obj/authCheck.o \
        linux-i686-debug/obj/authFile.o \
        linux-i686-debug/obj/authPam.o \
        linux-i686-debug/obj/cache.o \
        linux-i686-debug/obj/chunkFilter.o \
        linux-i686-debug/obj/client.o \
        linux-i686-debug/obj/conn.o \
        linux-i686-debug/obj/endpoint.o \
        linux-i686-debug/obj/error.o \
        linux-i686-debug/obj/host.o \
        linux-i686-debug/obj/httpService.o \
        linux-i686-debug/obj/log.o \
        linux-i686-debug/obj/netConnector.o \
        linux-i686-debug/obj/packet.o \
        linux-i686-debug/obj/passHandler.o \
        linux-i686-debug/obj/pipeline.o \
        linux-i686-debug/obj/queue.o \
        linux-i686-debug/obj/rangeFilter.o \
        linux-i686-debug/obj/route.o \
        linux-i686-debug/obj/rx.o \
        linux-i686-debug/obj/sendConnector.o \
        linux-i686-debug/obj/stage.o \
        linux-i686-debug/obj/trace.o \
        linux-i686-debug/obj/tx.o \
        linux-i686-debug/obj/uploadFilter.o \
        linux-i686-debug/obj/uri.o \
        linux-i686-debug/obj/var.o
	$(CC) -shared -o linux-i686-debug/lib/libhttp.so $(LDFLAGS) linux-i686-debug/obj/auth.o linux-i686-debug/obj/authCheck.o linux-i686-debug/obj/authFile.o linux-i686-debug/obj/authPam.o linux-i686-debug/obj/cache.o linux-i686-debug/obj/chunkFilter.o linux-i686-debug/obj/client.o linux-i686-debug/obj/conn.o linux-i686-debug/obj/endpoint.o linux-i686-debug/obj/error.o linux-i686-debug/obj/host.o linux-i686-debug/obj/httpService.o linux-i686-debug/obj/log.o linux-i686-debug/obj/netConnector.o linux-i686-debug/obj/packet.o linux-i686-debug/obj/passHandler.o linux-i686-debug/obj/pipeline.o linux-i686-debug/obj/queue.o linux-i686-debug/obj/rangeFilter.o linux-i686-debug/obj/route.o linux-i686-debug/obj/rx.o linux-i686-debug/obj/sendConnector.o linux-i686-debug/obj/stage.o linux-i686-debug/obj/trace.o linux-i686-debug/obj/tx.o linux-i686-debug/obj/uploadFilter.o linux-i686-debug/obj/uri.o linux-i686-debug/obj/var.o $(LIBS) -lmpr -lpcre

linux-i686-debug/obj/http.o: \
        src/utils/http.c \
        linux-i686-debug/inc/bit.h \
        src/http.h
	$(CC) -c -o linux-i686-debug/obj/http.o $(CFLAGS) $(DFLAGS) -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/utils/http.c

linux-i686-debug/bin/http:  \
        linux-i686-debug/lib/libhttp.so \
        linux-i686-debug/obj/http.o
	$(CC) -o linux-i686-debug/bin/http $(LDFLAGS) -Llinux-i686-debug/lib linux-i686-debug/obj/http.o $(LIBS) -lhttp -lmpr -lpcre -Llinux-i686-debug/lib -g

