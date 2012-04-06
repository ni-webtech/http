#
#   solaris-i686-debug.mk -- Build It Makefile to build Http Library for solaris on i686
#

CONFIG   := solaris-i686-debug
CC       := cc
LD       := ld
CFLAGS   := -Wall -fPIC -O3 -mcpu=i686 -fPIC -O3 -mcpu=i686
DFLAGS   := -D_REENTRANT -DCPU=${ARCH} -DPIC -DPIC
IFLAGS   := -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc
LDFLAGS  := 
LIBPATHS := -L$(CONFIG)/lib -L$(CONFIG)/lib
LIBS     := -llxnet -lrt -lsocket -lpthread -lm -lpthread -lm

all: prep \
        $(CONFIG)/lib/libmpr.so \
        $(CONFIG)/bin/makerom \
        $(CONFIG)/lib/libpcre.so \
        $(CONFIG)/lib/libhttp.so \
        $(CONFIG)/bin/http

.PHONY: prep

prep:
	@[ ! -x $(CONFIG)/inc ] && mkdir -p $(CONFIG)/inc $(CONFIG)/obj $(CONFIG)/lib $(CONFIG)/bin ; true
	@[ ! -f $(CONFIG)/inc/buildConfig.h ] && cp projects/buildConfig.$(CONFIG) $(CONFIG)/inc/buildConfig.h ; true
	@if ! diff $(CONFIG)/inc/buildConfig.h projects/buildConfig.$(CONFIG) >/dev/null ; then\
		echo cp projects/buildConfig.$(CONFIG) $(CONFIG)/inc/buildConfig.h  ; \
		cp projects/buildConfig.$(CONFIG) $(CONFIG)/inc/buildConfig.h  ; \
	fi; true

clean:
	rm -rf $(CONFIG)/lib/libmpr.so
	rm -rf $(CONFIG)/lib/libmprssl.so
	rm -rf $(CONFIG)/bin/${settings.manager}
	rm -rf $(CONFIG)/bin/makerom
	rm -rf $(CONFIG)/lib/libpcre.so
	rm -rf $(CONFIG)/lib/libhttp.so
	rm -rf $(CONFIG)/bin/http
	rm -rf $(CONFIG)/obj/mprLib.o
	rm -rf $(CONFIG)/obj/mprSsl.o
	rm -rf $(CONFIG)/obj/manager.o
	rm -rf $(CONFIG)/obj/makerom.o
	rm -rf $(CONFIG)/obj/pcre.o
	rm -rf $(CONFIG)/obj/auth.o
	rm -rf $(CONFIG)/obj/authCheck.o
	rm -rf $(CONFIG)/obj/authFile.o
	rm -rf $(CONFIG)/obj/authPam.o
	rm -rf $(CONFIG)/obj/cache.o
	rm -rf $(CONFIG)/obj/chunkFilter.o
	rm -rf $(CONFIG)/obj/client.o
	rm -rf $(CONFIG)/obj/conn.o
	rm -rf $(CONFIG)/obj/endpoint.o
	rm -rf $(CONFIG)/obj/error.o
	rm -rf $(CONFIG)/obj/host.o
	rm -rf $(CONFIG)/obj/httpService.o
	rm -rf $(CONFIG)/obj/log.o
	rm -rf $(CONFIG)/obj/netConnector.o
	rm -rf $(CONFIG)/obj/packet.o
	rm -rf $(CONFIG)/obj/passHandler.o
	rm -rf $(CONFIG)/obj/pipeline.o
	rm -rf $(CONFIG)/obj/queue.o
	rm -rf $(CONFIG)/obj/rangeFilter.o
	rm -rf $(CONFIG)/obj/route.o
	rm -rf $(CONFIG)/obj/rx.o
	rm -rf $(CONFIG)/obj/sendConnector.o
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
	rm -fr solaris-i686-debug/inc/mpr.h
	cp -r src/deps/mpr/mpr.h solaris-i686-debug/inc/mpr.h

$(CONFIG)/inc/mprSsl.h: 
	rm -fr solaris-i686-debug/inc/mprSsl.h
	cp -r src/deps/mpr/mprSsl.h solaris-i686-debug/inc/mprSsl.h

$(CONFIG)/obj/mprLib.o: \
        src/deps/mpr/mprLib.c \
        $(CONFIG)/inc/buildConfig.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/mprLib.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

$(CONFIG)/lib/libmpr.so:  \
        $(CONFIG)/inc/mpr.h \
        $(CONFIG)/inc/mprSsl.h \
        $(CONFIG)/obj/mprLib.o
	$(LDFLAGS)$(LDFLAGS)$(CC) -shared -o $(CONFIG)/lib/libmpr.so $(LIBPATHS) $(CONFIG)/obj/mprLib.o $(LIBS)

$(CONFIG)/obj/makerom.o: \
        src/deps/mpr/makerom.c \
        $(CONFIG)/inc/buildConfig.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/makerom.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

$(CONFIG)/bin/makerom:  \
        $(CONFIG)/lib/libmpr.so \
        $(CONFIG)/obj/makerom.o
	$(LDFLAGS)$(LDFLAGS)$(CC) -o $(CONFIG)/bin/makerom $(LIBPATHS) $(CONFIG)/obj/makerom.o $(LIBS) -lmpr 

$(CONFIG)/obj/pcre.o: \
        src/deps/pcre/pcre.c \
        $(CONFIG)/inc/buildConfig.h \
        src/deps/pcre/pcre.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/pcre.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

$(CONFIG)/lib/libpcre.so:  \
        $(CONFIG)/obj/pcre.o
	$(LDFLAGS)$(LDFLAGS)$(CC) -shared -o $(CONFIG)/lib/libpcre.so $(LIBPATHS) $(CONFIG)/obj/pcre.o $(LIBS)

$(CONFIG)/inc/http.h: 
	rm -fr solaris-i686-debug/inc/http.h
	cp -r src/http.h solaris-i686-debug/inc/http.h

$(CONFIG)/obj/auth.o: \
        src/auth.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/auth.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/auth.c

$(CONFIG)/obj/authCheck.o: \
        src/authCheck.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/authCheck.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/authCheck.c

$(CONFIG)/obj/authFile.o: \
        src/authFile.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/authFile.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/authFile.c

$(CONFIG)/obj/authPam.o: \
        src/authPam.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/authPam.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/authPam.c

$(CONFIG)/obj/cache.o: \
        src/cache.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/cache.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/cache.c

$(CONFIG)/obj/chunkFilter.o: \
        src/chunkFilter.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/chunkFilter.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/chunkFilter.c

$(CONFIG)/obj/client.o: \
        src/client.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/client.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/client.c

$(CONFIG)/obj/conn.o: \
        src/conn.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/conn.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/conn.c

$(CONFIG)/obj/endpoint.o: \
        src/endpoint.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/endpoint.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/endpoint.c

$(CONFIG)/obj/error.o: \
        src/error.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/error.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/error.c

$(CONFIG)/obj/host.o: \
        src/host.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/host.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/host.c

$(CONFIG)/obj/httpService.o: \
        src/httpService.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/httpService.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/httpService.c

$(CONFIG)/obj/log.o: \
        src/log.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/log.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/log.c

$(CONFIG)/obj/netConnector.o: \
        src/netConnector.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/netConnector.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/netConnector.c

$(CONFIG)/obj/packet.o: \
        src/packet.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/packet.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/packet.c

$(CONFIG)/obj/passHandler.o: \
        src/passHandler.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/passHandler.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/passHandler.c

$(CONFIG)/obj/pipeline.o: \
        src/pipeline.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/pipeline.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/pipeline.c

$(CONFIG)/obj/queue.o: \
        src/queue.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/queue.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/queue.c

$(CONFIG)/obj/rangeFilter.o: \
        src/rangeFilter.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/rangeFilter.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/rangeFilter.c

$(CONFIG)/obj/route.o: \
        src/route.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h \
        src/deps/pcre/pcre.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/route.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/route.c

$(CONFIG)/obj/rx.o: \
        src/rx.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/rx.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/rx.c

$(CONFIG)/obj/sendConnector.o: \
        src/sendConnector.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/sendConnector.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/sendConnector.c

$(CONFIG)/obj/stage.o: \
        src/stage.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/stage.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/stage.c

$(CONFIG)/obj/trace.o: \
        src/trace.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/trace.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/trace.c

$(CONFIG)/obj/tx.o: \
        src/tx.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/tx.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/tx.c

$(CONFIG)/obj/uploadFilter.o: \
        src/uploadFilter.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/uploadFilter.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/uploadFilter.c

$(CONFIG)/obj/uri.o: \
        src/uri.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/uri.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/uri.c

$(CONFIG)/obj/var.o: \
        src/var.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/var.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/var.c

$(CONFIG)/lib/libhttp.so:  \
        $(CONFIG)/lib/libmpr.so \
        $(CONFIG)/lib/libpcre.so \
        $(CONFIG)/inc/http.h \
        $(CONFIG)/obj/auth.o \
        $(CONFIG)/obj/authCheck.o \
        $(CONFIG)/obj/authFile.o \
        $(CONFIG)/obj/authPam.o \
        $(CONFIG)/obj/cache.o \
        $(CONFIG)/obj/chunkFilter.o \
        $(CONFIG)/obj/client.o \
        $(CONFIG)/obj/conn.o \
        $(CONFIG)/obj/endpoint.o \
        $(CONFIG)/obj/error.o \
        $(CONFIG)/obj/host.o \
        $(CONFIG)/obj/httpService.o \
        $(CONFIG)/obj/log.o \
        $(CONFIG)/obj/netConnector.o \
        $(CONFIG)/obj/packet.o \
        $(CONFIG)/obj/passHandler.o \
        $(CONFIG)/obj/pipeline.o \
        $(CONFIG)/obj/queue.o \
        $(CONFIG)/obj/rangeFilter.o \
        $(CONFIG)/obj/route.o \
        $(CONFIG)/obj/rx.o \
        $(CONFIG)/obj/sendConnector.o \
        $(CONFIG)/obj/stage.o \
        $(CONFIG)/obj/trace.o \
        $(CONFIG)/obj/tx.o \
        $(CONFIG)/obj/uploadFilter.o \
        $(CONFIG)/obj/uri.o \
        $(CONFIG)/obj/var.o
	$(LDFLAGS)$(LDFLAGS)$(CC) -shared -o $(CONFIG)/lib/libhttp.so $(LIBPATHS) $(CONFIG)/obj/auth.o $(CONFIG)/obj/authCheck.o $(CONFIG)/obj/authFile.o $(CONFIG)/obj/authPam.o $(CONFIG)/obj/cache.o $(CONFIG)/obj/chunkFilter.o $(CONFIG)/obj/client.o $(CONFIG)/obj/conn.o $(CONFIG)/obj/endpoint.o $(CONFIG)/obj/error.o $(CONFIG)/obj/host.o $(CONFIG)/obj/httpService.o $(CONFIG)/obj/log.o $(CONFIG)/obj/netConnector.o $(CONFIG)/obj/packet.o $(CONFIG)/obj/passHandler.o $(CONFIG)/obj/pipeline.o $(CONFIG)/obj/queue.o $(CONFIG)/obj/rangeFilter.o $(CONFIG)/obj/route.o $(CONFIG)/obj/rx.o $(CONFIG)/obj/sendConnector.o $(CONFIG)/obj/stage.o $(CONFIG)/obj/trace.o $(CONFIG)/obj/tx.o $(CONFIG)/obj/uploadFilter.o $(CONFIG)/obj/uri.o $(CONFIG)/obj/var.o $(LIBS) -lmpr -lpcre

$(CONFIG)/obj/http.o: \
        src/utils/http.c \
        $(CONFIG)/inc/buildConfig.h \
        src/http.h
	$(LDFLAGS)$(LDFLAGS)$(CC) -c -o $(CONFIG)/obj/http.o $(CFLAGS) -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I$(CONFIG)/inc -I$(CONFIG)/inc -Isrc/deps/pcre -Isrc src/utils/http.c

$(CONFIG)/bin/http:  \
        $(CONFIG)/lib/libhttp.so \
        $(CONFIG)/obj/http.o
	$(LDFLAGS)$(LDFLAGS)$(CC) -o $(CONFIG)/bin/http $(LIBPATHS) $(CONFIG)/obj/http.o $(LIBS) -lhttp -lmpr -lpcre 

