#
#   linux-i686-debug.mk -- Build It Makefile to build Http Library for linux on i686
#

PLATFORM       := linux-i686-debug
CC             := cc
LD             := ld
CFLAGS         := -Wall -fPIC -g -Wno-unused-result -mtune=i686
DFLAGS         := -D_REENTRANT -DCPU=i686 -DPIC
IFLAGS         := -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc
LDFLAGS        := -Wl,--enable-new-dtags '-Wl,-rpath,$$ORIGIN/' '-Wl,-rpath,$$ORIGIN/../lib' -L$(PLATFORM)/lib -g -ldl
LIBS           := -lpthread -lm

all: prep \
        $(PLATFORM)/lib/libmpr.so \
        $(PLATFORM)/bin/makerom \
        $(PLATFORM)/lib/libpcre.so \
        $(PLATFORM)/lib/libhttp.so \
        $(PLATFORM)/bin/http

.PHONY: prep

prep:
	@[ ! -x $(PLATFORM)/inc ] && mkdir -p $(PLATFORM)/inc $(PLATFORM)/obj $(PLATFORM)/lib $(PLATFORM)/bin ; true
	@[ ! -f $(PLATFORM)/inc/buildConfig.h ] && cp projects/buildConfig.$(PLATFORM) $(PLATFORM)/inc/buildConfig.h ; true

clean:
	rm -rf $(PLATFORM)/lib/libmpr.so
	rm -rf $(PLATFORM)/lib/libmprssl.so
	rm -rf $(PLATFORM)/bin/${settings.manager}
	rm -rf $(PLATFORM)/bin/makerom
	rm -rf $(PLATFORM)/lib/libpcre.so
	rm -rf $(PLATFORM)/lib/libhttp.so
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
	rm -fr linux-i686-debug/inc/mpr.h
	cp -r src/deps/mpr/mpr.h linux-i686-debug/inc/mpr.h

$(PLATFORM)/inc/mprSsl.h: 
	rm -fr linux-i686-debug/inc/mprSsl.h
	cp -r src/deps/mpr/mprSsl.h linux-i686-debug/inc/mprSsl.h

$(PLATFORM)/obj/mprLib.o: \
        src/deps/mpr/mprLib.c \
        $(PLATFORM)/inc/buildConfig.h
	$(CC) -c -o $(PLATFORM)/obj/mprLib.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

$(PLATFORM)/lib/libmpr.so:  \
        $(PLATFORM)/inc/mpr.h \
        $(PLATFORM)/inc/mprSsl.h \
        $(PLATFORM)/obj/mprLib.o
	$(CC) -shared -o $(PLATFORM)/lib/libmpr.so $(LDFLAGS) $(PLATFORM)/obj/mprLib.o $(LIBS)

$(PLATFORM)/obj/makerom.o: \
        src/deps/mpr/makerom.c \
        $(PLATFORM)/inc/buildConfig.h
	$(CC) -c -o $(PLATFORM)/obj/makerom.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

$(PLATFORM)/bin/makerom:  \
        $(PLATFORM)/lib/libmpr.so \
        $(PLATFORM)/obj/makerom.o
	$(CC) -o $(PLATFORM)/bin/makerom $(LDFLAGS) -L$(PLATFORM)/lib $(PLATFORM)/obj/makerom.o $(LIBS) -lmpr $(LDFLAGS)

$(PLATFORM)/obj/pcre.o: \
        src/deps/pcre/pcre.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o $(PLATFORM)/obj/pcre.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

$(PLATFORM)/lib/libpcre.so:  \
        $(PLATFORM)/obj/pcre.o
	$(CC) -shared -o $(PLATFORM)/lib/libpcre.so $(LDFLAGS) $(PLATFORM)/obj/pcre.o $(LIBS)

$(PLATFORM)/inc/http.h: 
	rm -fr linux-i686-debug/inc/http.h
	cp -r src/http.h linux-i686-debug/inc/http.h

$(PLATFORM)/obj/auth.o: \
        src/auth.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/auth.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/auth.c

$(PLATFORM)/obj/authCheck.o: \
        src/authCheck.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/authCheck.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/authCheck.c

$(PLATFORM)/obj/authFile.o: \
        src/authFile.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/authFile.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/authFile.c

$(PLATFORM)/obj/authPam.o: \
        src/authPam.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/authPam.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/authPam.c

$(PLATFORM)/obj/cache.o: \
        src/cache.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/cache.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/cache.c

$(PLATFORM)/obj/chunkFilter.o: \
        src/chunkFilter.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/chunkFilter.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/chunkFilter.c

$(PLATFORM)/obj/client.o: \
        src/client.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/client.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/client.c

$(PLATFORM)/obj/conn.o: \
        src/conn.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/conn.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/conn.c

$(PLATFORM)/obj/endpoint.o: \
        src/endpoint.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/endpoint.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/endpoint.c

$(PLATFORM)/obj/error.o: \
        src/error.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/error.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/error.c

$(PLATFORM)/obj/host.o: \
        src/host.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/host.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/host.c

$(PLATFORM)/obj/httpService.o: \
        src/httpService.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/httpService.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/httpService.c

$(PLATFORM)/obj/log.o: \
        src/log.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/log.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/log.c

$(PLATFORM)/obj/netConnector.o: \
        src/netConnector.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/netConnector.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/netConnector.c

$(PLATFORM)/obj/packet.o: \
        src/packet.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/packet.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/packet.c

$(PLATFORM)/obj/passHandler.o: \
        src/passHandler.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/passHandler.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/passHandler.c

$(PLATFORM)/obj/pipeline.o: \
        src/pipeline.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/pipeline.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/pipeline.c

$(PLATFORM)/obj/queue.o: \
        src/queue.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/queue.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/queue.c

$(PLATFORM)/obj/rangeFilter.o: \
        src/rangeFilter.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/rangeFilter.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/rangeFilter.c

$(PLATFORM)/obj/route.o: \
        src/route.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h \
        src/deps/pcre/pcre.h
	$(CC) -c -o $(PLATFORM)/obj/route.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/route.c

$(PLATFORM)/obj/rx.o: \
        src/rx.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/rx.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/rx.c

$(PLATFORM)/obj/sendConnector.o: \
        src/sendConnector.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/sendConnector.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/sendConnector.c

$(PLATFORM)/obj/stage.o: \
        src/stage.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/stage.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/stage.c

$(PLATFORM)/obj/trace.o: \
        src/trace.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/trace.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/trace.c

$(PLATFORM)/obj/tx.o: \
        src/tx.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/tx.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/tx.c

$(PLATFORM)/obj/uploadFilter.o: \
        src/uploadFilter.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/uploadFilter.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/uploadFilter.c

$(PLATFORM)/obj/uri.o: \
        src/uri.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/uri.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/uri.c

$(PLATFORM)/obj/var.o: \
        src/var.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/var.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/var.c

$(PLATFORM)/lib/libhttp.so:  \
        $(PLATFORM)/lib/libmpr.so \
        $(PLATFORM)/lib/libpcre.so \
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
	$(CC) -shared -o $(PLATFORM)/lib/libhttp.so $(LDFLAGS) $(PLATFORM)/obj/auth.o $(PLATFORM)/obj/authCheck.o $(PLATFORM)/obj/authFile.o $(PLATFORM)/obj/authPam.o $(PLATFORM)/obj/cache.o $(PLATFORM)/obj/chunkFilter.o $(PLATFORM)/obj/client.o $(PLATFORM)/obj/conn.o $(PLATFORM)/obj/endpoint.o $(PLATFORM)/obj/error.o $(PLATFORM)/obj/host.o $(PLATFORM)/obj/httpService.o $(PLATFORM)/obj/log.o $(PLATFORM)/obj/netConnector.o $(PLATFORM)/obj/packet.o $(PLATFORM)/obj/passHandler.o $(PLATFORM)/obj/pipeline.o $(PLATFORM)/obj/queue.o $(PLATFORM)/obj/rangeFilter.o $(PLATFORM)/obj/route.o $(PLATFORM)/obj/rx.o $(PLATFORM)/obj/sendConnector.o $(PLATFORM)/obj/stage.o $(PLATFORM)/obj/trace.o $(PLATFORM)/obj/tx.o $(PLATFORM)/obj/uploadFilter.o $(PLATFORM)/obj/uri.o $(PLATFORM)/obj/var.o $(LIBS) -lmpr -lpcre

$(PLATFORM)/obj/http.o: \
        src/utils/http.c \
        $(PLATFORM)/inc/buildConfig.h \
        src/http.h
	$(CC) -c -o $(PLATFORM)/obj/http.o $(CFLAGS) $(DFLAGS) -I$(PLATFORM)/inc -Isrc/deps/pcre -Isrc src/utils/http.c

$(PLATFORM)/bin/http:  \
        $(PLATFORM)/lib/libhttp.so \
        $(PLATFORM)/obj/http.o
	$(CC) -o $(PLATFORM)/bin/http $(LDFLAGS) -L$(PLATFORM)/lib $(PLATFORM)/obj/http.o $(LIBS) -lhttp -lmpr -lpcre $(LDFLAGS)

