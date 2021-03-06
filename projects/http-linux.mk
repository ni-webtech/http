#
#   http-linux.mk -- Makefile to build Http Library for linux
#

ARCH     ?= $(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/')
OS       ?= linux
CC       ?= gcc
LD       ?= /usr/bin/ld
PROFILE  ?= debug
CONFIG   ?= $(OS)-$(ARCH)-$(PROFILE)

CFLAGS   += -fPIC -mtune=generic -w
DFLAGS   += -D_REENTRANT -DPIC 
IFLAGS   += -I$(CONFIG)/inc -Isrc
LDFLAGS  += '-Wl,--enable-new-dtags' '-Wl,-rpath,$$ORIGIN/' '-Wl,-rpath,$$ORIGIN/../bin' '-rdynamic'
LIBPATHS += -L$(CONFIG)/bin
LIBS     += -lpthread -lm -ldl

CFLAGS-debug    := -DBIT_DEBUG -g
CFLAGS-release  := -O2
LDFLAGS-debug   := -g
LDFLAGS-release := 
CFLAGS          += $(CFLAGS-$(PROFILE))
LDFLAGS         += $(LDFLAGS-$(PROFILE))

all: prep \
        $(CONFIG)/bin/libmpr.so \
        $(CONFIG)/bin/libmprssl.so \
        $(CONFIG)/bin/makerom \
        $(CONFIG)/bin/libpcre.so \
        $(CONFIG)/bin/libhttp.so \
        $(CONFIG)/bin/http

.PHONY: prep

prep:
	@if [ "$(CONFIG)" = "" ] ; then echo WARNING: CONFIG not set ; exit 255 ; fi
	@[ ! -x $(CONFIG)/inc ] && mkdir -p $(CONFIG)/inc $(CONFIG)/obj $(CONFIG)/lib $(CONFIG)/bin ; true
	@[ ! -f $(CONFIG)/inc/bit.h ] && cp projects/http-$(OS)-bit.h $(CONFIG)/inc/bit.h ; true
	@if ! diff $(CONFIG)/inc/bit.h projects/http-$(OS)-bit.h >/dev/null ; then\
		echo cp projects/http-$(OS)-bit.h $(CONFIG)/inc/bit.h  ; \
		cp projects/http-$(OS)-bit.h $(CONFIG)/inc/bit.h  ; \
	fi; true

clean:
	rm -rf $(CONFIG)/bin/libmpr.so
	rm -rf $(CONFIG)/bin/libmprssl.so
	rm -rf $(CONFIG)/bin/makerom
	rm -rf $(CONFIG)/bin/libpcre.so
	rm -rf $(CONFIG)/bin/libhttp.so
	rm -rf $(CONFIG)/bin/http
	rm -rf $(CONFIG)/obj/mprLib.o
	rm -rf $(CONFIG)/obj/mprSsl.o
	rm -rf $(CONFIG)/obj/manager.o
	rm -rf $(CONFIG)/obj/makerom.o
	rm -rf $(CONFIG)/obj/pcre.o
	rm -rf $(CONFIG)/obj/auth.o
	rm -rf $(CONFIG)/obj/basic.o
	rm -rf $(CONFIG)/obj/cache.o
	rm -rf $(CONFIG)/obj/chunkFilter.o
	rm -rf $(CONFIG)/obj/client.o
	rm -rf $(CONFIG)/obj/conn.o
	rm -rf $(CONFIG)/obj/digest.o
	rm -rf $(CONFIG)/obj/endpoint.o
	rm -rf $(CONFIG)/obj/error.o
	rm -rf $(CONFIG)/obj/host.o
	rm -rf $(CONFIG)/obj/httpService.o
	rm -rf $(CONFIG)/obj/log.o
	rm -rf $(CONFIG)/obj/netConnector.o
	rm -rf $(CONFIG)/obj/packet.o
	rm -rf $(CONFIG)/obj/pam.o
	rm -rf $(CONFIG)/obj/passHandler.o
	rm -rf $(CONFIG)/obj/pipeline.o
	rm -rf $(CONFIG)/obj/procHandler.o
	rm -rf $(CONFIG)/obj/queue.o
	rm -rf $(CONFIG)/obj/rangeFilter.o
	rm -rf $(CONFIG)/obj/route.o
	rm -rf $(CONFIG)/obj/rx.o
	rm -rf $(CONFIG)/obj/sendConnector.o
	rm -rf $(CONFIG)/obj/session.o
	rm -rf $(CONFIG)/obj/sockFilter.o
	rm -rf $(CONFIG)/obj/stage.o
	rm -rf $(CONFIG)/obj/trace.o
	rm -rf $(CONFIG)/obj/tx.o
	rm -rf $(CONFIG)/obj/uploadFilter.o
	rm -rf $(CONFIG)/obj/uri.o
	rm -rf $(CONFIG)/obj/var.o
	rm -rf $(CONFIG)/obj/http.o

clobber: clean
	rm -fr ./$(CONFIG)

$(CONFIG)/inc/mpr.h: 
	rm -fr $(CONFIG)/inc/mpr.h
	cp -r src/deps/mpr/mpr.h $(CONFIG)/inc/mpr.h

$(CONFIG)/obj/mprLib.o: \
        src/deps/mpr/mprLib.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/mprLib.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/deps/mpr/mprLib.c

$(CONFIG)/bin/libmpr.so:  \
        $(CONFIG)/inc/mpr.h \
        $(CONFIG)/obj/mprLib.o
	$(CC) -shared -o $(CONFIG)/bin/libmpr.so $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/mprLib.o $(LIBS)

$(CONFIG)/obj/mprSsl.o: \
        src/deps/mpr/mprSsl.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/mprSsl.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/deps/mpr/mprSsl.c

$(CONFIG)/bin/libmprssl.so:  \
        $(CONFIG)/bin/libmpr.so \
        $(CONFIG)/obj/mprSsl.o
	$(CC) -shared -o $(CONFIG)/bin/libmprssl.so $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/mprSsl.o $(LIBS) -lmpr

$(CONFIG)/obj/makerom.o: \
        src/deps/mpr/makerom.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/makerom.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/deps/mpr/makerom.c

$(CONFIG)/bin/makerom:  \
        $(CONFIG)/bin/libmpr.so \
        $(CONFIG)/obj/makerom.o
	$(CC) -o $(CONFIG)/bin/makerom $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/makerom.o $(LIBS) -lmpr $(LDFLAGS)

$(CONFIG)/inc/pcre.h: 
	rm -fr $(CONFIG)/inc/pcre.h
	cp -r src/deps/pcre/pcre.h $(CONFIG)/inc/pcre.h

$(CONFIG)/obj/pcre.o: \
        src/deps/pcre/pcre.c \
        $(CONFIG)/inc/bit.h
	$(CC) -c -o $(CONFIG)/obj/pcre.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/deps/pcre/pcre.c

$(CONFIG)/bin/libpcre.so:  \
        $(CONFIG)/inc/pcre.h \
        $(CONFIG)/obj/pcre.o
	$(CC) -shared -o $(CONFIG)/bin/libpcre.so $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/pcre.o $(LIBS)

$(CONFIG)/inc/http.h: 
	rm -fr $(CONFIG)/inc/http.h
	cp -r src/http.h $(CONFIG)/inc/http.h

$(CONFIG)/obj/auth.o: \
        src/auth.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/auth.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/auth.c

$(CONFIG)/obj/basic.o: \
        src/basic.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/basic.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/basic.c

$(CONFIG)/obj/cache.o: \
        src/cache.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/cache.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/cache.c

$(CONFIG)/obj/chunkFilter.o: \
        src/chunkFilter.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/chunkFilter.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/chunkFilter.c

$(CONFIG)/obj/client.o: \
        src/client.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/client.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/client.c

$(CONFIG)/obj/conn.o: \
        src/conn.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/conn.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/conn.c

$(CONFIG)/obj/digest.o: \
        src/digest.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/digest.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/digest.c

$(CONFIG)/obj/endpoint.o: \
        src/endpoint.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/endpoint.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/endpoint.c

$(CONFIG)/obj/error.o: \
        src/error.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/error.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/error.c

$(CONFIG)/obj/host.o: \
        src/host.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/host.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/host.c

$(CONFIG)/obj/httpService.o: \
        src/httpService.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/httpService.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/httpService.c

$(CONFIG)/obj/log.o: \
        src/log.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/log.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/log.c

$(CONFIG)/obj/netConnector.o: \
        src/netConnector.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/netConnector.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/netConnector.c

$(CONFIG)/obj/packet.o: \
        src/packet.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/packet.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/packet.c

$(CONFIG)/obj/pam.o: \
        src/pam.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/pam.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/pam.c

$(CONFIG)/obj/passHandler.o: \
        src/passHandler.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/passHandler.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/passHandler.c

$(CONFIG)/obj/pipeline.o: \
        src/pipeline.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/pipeline.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/pipeline.c

$(CONFIG)/obj/procHandler.o: \
        src/procHandler.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/procHandler.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/procHandler.c

$(CONFIG)/obj/queue.o: \
        src/queue.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/queue.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/queue.c

$(CONFIG)/obj/rangeFilter.o: \
        src/rangeFilter.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/rangeFilter.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/rangeFilter.c

$(CONFIG)/obj/route.o: \
        src/route.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/route.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/route.c

$(CONFIG)/obj/rx.o: \
        src/rx.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/rx.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/rx.c

$(CONFIG)/obj/sendConnector.o: \
        src/sendConnector.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/sendConnector.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/sendConnector.c

$(CONFIG)/obj/session.o: \
        src/session.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/session.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/session.c

$(CONFIG)/obj/sockFilter.o: \
        src/sockFilter.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/sockFilter.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/sockFilter.c

$(CONFIG)/obj/stage.o: \
        src/stage.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/stage.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/stage.c

$(CONFIG)/obj/trace.o: \
        src/trace.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/trace.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/trace.c

$(CONFIG)/obj/tx.o: \
        src/tx.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/tx.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/tx.c

$(CONFIG)/obj/uploadFilter.o: \
        src/uploadFilter.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/uploadFilter.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/uploadFilter.c

$(CONFIG)/obj/uri.o: \
        src/uri.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/uri.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/uri.c

$(CONFIG)/obj/var.o: \
        src/var.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/var.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/var.c

$(CONFIG)/bin/libhttp.so:  \
        $(CONFIG)/bin/libmpr.so \
        $(CONFIG)/bin/libpcre.so \
        $(CONFIG)/inc/http.h \
        $(CONFIG)/obj/auth.o \
        $(CONFIG)/obj/basic.o \
        $(CONFIG)/obj/cache.o \
        $(CONFIG)/obj/chunkFilter.o \
        $(CONFIG)/obj/client.o \
        $(CONFIG)/obj/conn.o \
        $(CONFIG)/obj/digest.o \
        $(CONFIG)/obj/endpoint.o \
        $(CONFIG)/obj/error.o \
        $(CONFIG)/obj/host.o \
        $(CONFIG)/obj/httpService.o \
        $(CONFIG)/obj/log.o \
        $(CONFIG)/obj/netConnector.o \
        $(CONFIG)/obj/packet.o \
        $(CONFIG)/obj/pam.o \
        $(CONFIG)/obj/passHandler.o \
        $(CONFIG)/obj/pipeline.o \
        $(CONFIG)/obj/procHandler.o \
        $(CONFIG)/obj/queue.o \
        $(CONFIG)/obj/rangeFilter.o \
        $(CONFIG)/obj/route.o \
        $(CONFIG)/obj/rx.o \
        $(CONFIG)/obj/sendConnector.o \
        $(CONFIG)/obj/session.o \
        $(CONFIG)/obj/sockFilter.o \
        $(CONFIG)/obj/stage.o \
        $(CONFIG)/obj/trace.o \
        $(CONFIG)/obj/tx.o \
        $(CONFIG)/obj/uploadFilter.o \
        $(CONFIG)/obj/uri.o \
        $(CONFIG)/obj/var.o
	$(CC) -shared -o $(CONFIG)/bin/libhttp.so $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/auth.o $(CONFIG)/obj/basic.o $(CONFIG)/obj/cache.o $(CONFIG)/obj/chunkFilter.o $(CONFIG)/obj/client.o $(CONFIG)/obj/conn.o $(CONFIG)/obj/digest.o $(CONFIG)/obj/endpoint.o $(CONFIG)/obj/error.o $(CONFIG)/obj/host.o $(CONFIG)/obj/httpService.o $(CONFIG)/obj/log.o $(CONFIG)/obj/netConnector.o $(CONFIG)/obj/packet.o $(CONFIG)/obj/pam.o $(CONFIG)/obj/passHandler.o $(CONFIG)/obj/pipeline.o $(CONFIG)/obj/procHandler.o $(CONFIG)/obj/queue.o $(CONFIG)/obj/rangeFilter.o $(CONFIG)/obj/route.o $(CONFIG)/obj/rx.o $(CONFIG)/obj/sendConnector.o $(CONFIG)/obj/session.o $(CONFIG)/obj/sockFilter.o $(CONFIG)/obj/stage.o $(CONFIG)/obj/trace.o $(CONFIG)/obj/tx.o $(CONFIG)/obj/uploadFilter.o $(CONFIG)/obj/uri.o $(CONFIG)/obj/var.o $(LIBS) -lmpr -lpcre

$(CONFIG)/obj/http.o: \
        src/http.c \
        $(CONFIG)/inc/bit.h \
        src/http.h
	$(CC) -c -o $(CONFIG)/obj/http.o $(CFLAGS) $(DFLAGS) -I$(CONFIG)/inc -Isrc src/http.c

$(CONFIG)/bin/http:  \
        $(CONFIG)/bin/libhttp.so \
        $(CONFIG)/obj/http.o
	$(CC) -o $(CONFIG)/bin/http $(LDFLAGS) $(LIBPATHS) $(CONFIG)/obj/http.o $(LIBS) -lhttp -lmpr -lpcre $(LDFLAGS)

