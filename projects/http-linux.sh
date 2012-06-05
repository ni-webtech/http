#
#   http-linux.sh -- Build It Shell Script to build Http Library
#

ARCH="x86"
ARCH="$(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/')"
OS="linux"
PROFILE="debug"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="/usr/bin/gcc"
LD="/usr/bin/ld"
CFLAGS="-Wall -fPIC -g -Wno-unused-result -mtune=generic"
DFLAGS="-D_REENTRANT -DPIC -DBIT_DEBUG"
IFLAGS="-I${CONFIG}/inc -Isrc"
LDFLAGS="-Wl,--enable-new-dtags -Wl,-rpath,\$ORIGIN/ -Wl,-rpath,\$ORIGIN/../bin -rdynamic -g"
LIBPATHS="-L${CONFIG}/bin"
LIBS="-lpthread -lm -ldl"

[ ! -x ${CONFIG}/inc ] && mkdir -p ${CONFIG}/inc ${CONFIG}/obj ${CONFIG}/lib ${CONFIG}/bin

[ ! -f ${CONFIG}/inc/bit.h ] && cp projects/http-${OS}-bit.h ${CONFIG}/inc/bit.h
if ! diff ${CONFIG}/inc/bit.h projects/http-${OS}-bit.h >/dev/null ; then
	cp projects/http-${OS}-bit.h ${CONFIG}/inc/bit.h
fi

rm -rf ${CONFIG}/inc/mpr.h
cp -r src/deps/mpr/mpr.h ${CONFIG}/inc/mpr.h

${CC} -c -o ${CONFIG}/obj/mprLib.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/deps/mpr/mprLib.c

${CC} -shared -o ${CONFIG}/bin/libmpr.so ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/mprLib.o ${LIBS}

${CC} -c -o ${CONFIG}/obj/mprSsl.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/deps/mpr/mprSsl.c

${CC} -shared -o ${CONFIG}/bin/libmprssl.so ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/mprSsl.o ${LIBS} -lmpr

${CC} -c -o ${CONFIG}/obj/makerom.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/deps/mpr/makerom.c

${CC} -o ${CONFIG}/bin/makerom ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/makerom.o ${LIBS} -lmpr ${LDFLAGS}

rm -rf ${CONFIG}/inc/pcre.h
cp -r src/deps/pcre/pcre.h ${CONFIG}/inc/pcre.h

${CC} -c -o ${CONFIG}/obj/pcre.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/deps/pcre/pcre.c

${CC} -shared -o ${CONFIG}/bin/libpcre.so ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/pcre.o ${LIBS}

rm -rf ${CONFIG}/inc/http.h
cp -r src/http.h ${CONFIG}/inc/http.h

${CC} -c -o ${CONFIG}/obj/auth.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/auth.c

${CC} -c -o ${CONFIG}/obj/authCheck.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/authCheck.c

${CC} -c -o ${CONFIG}/obj/authFile.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/authFile.c

${CC} -c -o ${CONFIG}/obj/authPam.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/authPam.c

${CC} -c -o ${CONFIG}/obj/cache.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/cache.c

${CC} -c -o ${CONFIG}/obj/chunkFilter.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/chunkFilter.c

${CC} -c -o ${CONFIG}/obj/client.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/client.c

${CC} -c -o ${CONFIG}/obj/conn.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/conn.c

${CC} -c -o ${CONFIG}/obj/endpoint.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/endpoint.c

${CC} -c -o ${CONFIG}/obj/error.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/error.c

${CC} -c -o ${CONFIG}/obj/host.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/host.c

${CC} -c -o ${CONFIG}/obj/httpService.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/httpService.c

${CC} -c -o ${CONFIG}/obj/log.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/log.c

${CC} -c -o ${CONFIG}/obj/netConnector.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/netConnector.c

${CC} -c -o ${CONFIG}/obj/packet.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/packet.c

${CC} -c -o ${CONFIG}/obj/passHandler.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/passHandler.c

${CC} -c -o ${CONFIG}/obj/pipeline.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/pipeline.c

${CC} -c -o ${CONFIG}/obj/queue.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/queue.c

${CC} -c -o ${CONFIG}/obj/rangeFilter.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/rangeFilter.c

${CC} -c -o ${CONFIG}/obj/route.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/route.c

${CC} -c -o ${CONFIG}/obj/rx.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/rx.c

${CC} -c -o ${CONFIG}/obj/sendConnector.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/sendConnector.c

${CC} -c -o ${CONFIG}/obj/stage.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/stage.c

${CC} -c -o ${CONFIG}/obj/trace.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/trace.c

${CC} -c -o ${CONFIG}/obj/tx.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/tx.c

${CC} -c -o ${CONFIG}/obj/uploadFilter.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/uploadFilter.c

${CC} -c -o ${CONFIG}/obj/uri.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/uri.c

${CC} -c -o ${CONFIG}/obj/var.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/var.c

${CC} -shared -o ${CONFIG}/bin/libhttp.so ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/auth.o ${CONFIG}/obj/authCheck.o ${CONFIG}/obj/authFile.o ${CONFIG}/obj/authPam.o ${CONFIG}/obj/cache.o ${CONFIG}/obj/chunkFilter.o ${CONFIG}/obj/client.o ${CONFIG}/obj/conn.o ${CONFIG}/obj/endpoint.o ${CONFIG}/obj/error.o ${CONFIG}/obj/host.o ${CONFIG}/obj/httpService.o ${CONFIG}/obj/log.o ${CONFIG}/obj/netConnector.o ${CONFIG}/obj/packet.o ${CONFIG}/obj/passHandler.o ${CONFIG}/obj/pipeline.o ${CONFIG}/obj/queue.o ${CONFIG}/obj/rangeFilter.o ${CONFIG}/obj/route.o ${CONFIG}/obj/rx.o ${CONFIG}/obj/sendConnector.o ${CONFIG}/obj/stage.o ${CONFIG}/obj/trace.o ${CONFIG}/obj/tx.o ${CONFIG}/obj/uploadFilter.o ${CONFIG}/obj/uri.o ${CONFIG}/obj/var.o ${LIBS} -lmpr -lpcre -lmprssl

${CC} -c -o ${CONFIG}/obj/http.o ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/utils/http.c

${CC} -o ${CONFIG}/bin/http ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/http.o ${LIBS} -lhttp -lmpr -lpcre -lmprssl ${LDFLAGS}

