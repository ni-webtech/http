#
#   http-solaris.sh -- Build It Shell Script to build Http Library
#

ARCH="x86"
ARCH="$(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/')"
OS="solaris"
PROFILE="debug"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="gcc"
LD="ld"
CFLAGS="-Wall -fPIC -g -mcpu=generic"
DFLAGS="-D_REENTRANT -DPIC -DBLD_DEBUG"
IFLAGS="-I${CONFIG}/inc -Isrc/deps/pcre -Isrc"
LDFLAGS="-g"
LIBPATHS="-L${CONFIG}/bin"
LIBS="-llxnet -lrt -lsocket -lpthread -lm"

[ ! -x ${CONFIG}/inc ] && mkdir -p ${CONFIG}/inc ${CONFIG}/obj ${CONFIG}/lib ${CONFIG}/bin

[ ! -f ${CONFIG}/inc/bit.h ] && cp projects/http-${OS}-bit.h ${CONFIG}/inc/bit.h
if ! diff ${CONFIG}/inc/bit.h projects/http-${OS}-bit.h >/dev/null ; then
	cp projects/http-${OS}-bit.h ${CONFIG}/inc/bit.h
fi

rm -rf ${CONFIG}/inc/mpr.h
cp -r src/deps/mpr/mpr.h ${CONFIG}/inc/mpr.h

rm -rf ${CONFIG}/inc/mprSsl.h
cp -r src/deps/mpr/mprSsl.h ${CONFIG}/inc/mprSsl.h

${CC} -c -o ${CONFIG}/obj/mprLib.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

${CC} -shared -o ${CONFIG}/bin/libmpr.so ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/mprLib.o ${LIBS}

${CC} -c -o ${CONFIG}/obj/mprSsl.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprSsl.c

${CC} -shared -o ${CONFIG}/bin/libmprssl.so ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/mprSsl.o ${LIBS} -lmpr

${CC} -c -o ${CONFIG}/obj/makerom.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

${CC} -o ${CONFIG}/bin/makerom ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/makerom.o ${LIBS} -lmpr ${LDFLAGS}

${CC} -c -o ${CONFIG}/obj/pcre.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

${CC} -shared -o ${CONFIG}/bin/libpcre.so ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/pcre.o ${LIBS}

rm -rf ${CONFIG}/inc/http.h
cp -r src/http.h ${CONFIG}/inc/http.h

${CC} -c -o ${CONFIG}/obj/auth.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/auth.c

${CC} -c -o ${CONFIG}/obj/authCheck.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authCheck.c

${CC} -c -o ${CONFIG}/obj/authFile.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authFile.c

${CC} -c -o ${CONFIG}/obj/authPam.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authPam.c

${CC} -c -o ${CONFIG}/obj/cache.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/cache.c

${CC} -c -o ${CONFIG}/obj/chunkFilter.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/chunkFilter.c

${CC} -c -o ${CONFIG}/obj/client.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/client.c

${CC} -c -o ${CONFIG}/obj/conn.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/conn.c

${CC} -c -o ${CONFIG}/obj/endpoint.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/endpoint.c

${CC} -c -o ${CONFIG}/obj/error.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/error.c

${CC} -c -o ${CONFIG}/obj/host.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/host.c

${CC} -c -o ${CONFIG}/obj/httpService.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/httpService.c

${CC} -c -o ${CONFIG}/obj/log.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/log.c

${CC} -c -o ${CONFIG}/obj/netConnector.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/netConnector.c

${CC} -c -o ${CONFIG}/obj/packet.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/packet.c

${CC} -c -o ${CONFIG}/obj/passHandler.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/passHandler.c

${CC} -c -o ${CONFIG}/obj/pipeline.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/pipeline.c

${CC} -c -o ${CONFIG}/obj/queue.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/queue.c

${CC} -c -o ${CONFIG}/obj/rangeFilter.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/rangeFilter.c

${CC} -c -o ${CONFIG}/obj/route.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/route.c

${CC} -c -o ${CONFIG}/obj/rx.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/rx.c

${CC} -c -o ${CONFIG}/obj/sendConnector.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/sendConnector.c

${CC} -c -o ${CONFIG}/obj/stage.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/stage.c

${CC} -c -o ${CONFIG}/obj/trace.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/trace.c

${CC} -c -o ${CONFIG}/obj/tx.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/tx.c

${CC} -c -o ${CONFIG}/obj/uploadFilter.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/uploadFilter.c

${CC} -c -o ${CONFIG}/obj/uri.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/uri.c

${CC} -c -o ${CONFIG}/obj/var.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/var.c

${CC} -shared -o ${CONFIG}/bin/libhttp.so ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/auth.o ${CONFIG}/obj/authCheck.o ${CONFIG}/obj/authFile.o ${CONFIG}/obj/authPam.o ${CONFIG}/obj/cache.o ${CONFIG}/obj/chunkFilter.o ${CONFIG}/obj/client.o ${CONFIG}/obj/conn.o ${CONFIG}/obj/endpoint.o ${CONFIG}/obj/error.o ${CONFIG}/obj/host.o ${CONFIG}/obj/httpService.o ${CONFIG}/obj/log.o ${CONFIG}/obj/netConnector.o ${CONFIG}/obj/packet.o ${CONFIG}/obj/passHandler.o ${CONFIG}/obj/pipeline.o ${CONFIG}/obj/queue.o ${CONFIG}/obj/rangeFilter.o ${CONFIG}/obj/route.o ${CONFIG}/obj/rx.o ${CONFIG}/obj/sendConnector.o ${CONFIG}/obj/stage.o ${CONFIG}/obj/trace.o ${CONFIG}/obj/tx.o ${CONFIG}/obj/uploadFilter.o ${CONFIG}/obj/uri.o ${CONFIG}/obj/var.o ${LIBS} -lmpr -lpcre -lmprssl

${CC} -c -o ${CONFIG}/obj/http.o -Wall -fPIC ${LDFLAGS} -mcpu=generic ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/utils/http.c

${CC} -o ${CONFIG}/bin/http ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/http.o ${LIBS} -lhttp -lmpr -lpcre -lmprssl ${LDFLAGS}

