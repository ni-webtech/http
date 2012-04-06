#
#   solaris-i686-debug.sh -- Build It Shell Script to build Http Library
#

CONFIG="solaris-i686-debug"
CC="cc"
LD="ld"
CFLAGS="-Wall -fPIC -O3 -mcpu=i686 -fPIC -O3 -mcpu=i686"
DFLAGS="-D_REENTRANT -DCPU=${ARCH} -DPIC -DPIC"
IFLAGS="-Isolaris-i686-debug/inc -Isolaris-i686-debug/inc -Isrc/deps/pcre -Isrc"
LDFLAGS=""
LIBPATHS="-L${CONFIG}/lib -L${CONFIG}/lib"
LIBS="-llxnet -lrt -lsocket -lpthread -lm -lpthread -lm"

[ ! -x ${CONFIG}/inc ] && mkdir -p ${CONFIG}/inc ${CONFIG}/obj ${CONFIG}/lib ${CONFIG}/bin
cp projects/buildConfig.${CONFIG} ${CONFIG}/inc/buildConfig.h

rm -rf solaris-i686-debug/inc/mpr.h
cp -r src/deps/mpr/mpr.h solaris-i686-debug/inc/mpr.h

rm -rf solaris-i686-debug/inc/mprSsl.h
cp -r src/deps/mpr/mprSsl.h solaris-i686-debug/inc/mprSsl.h

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/mprLib.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

${LDFLAGS}${LDFLAGS}${CC} -shared -o ${CONFIG}/lib/libmpr.so ${LIBPATHS} ${CONFIG}/obj/mprLib.o ${LIBS}

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/makerom.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

${LDFLAGS}${LDFLAGS}${CC} -o ${CONFIG}/bin/makerom ${LIBPATHS} ${CONFIG}/obj/makerom.o ${LIBS} -lmpr 

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/pcre.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

${LDFLAGS}${LDFLAGS}${CC} -shared -o ${CONFIG}/lib/libpcre.so ${LIBPATHS} ${CONFIG}/obj/pcre.o ${LIBS}

rm -rf solaris-i686-debug/inc/http.h
cp -r src/http.h solaris-i686-debug/inc/http.h

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/auth.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/auth.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/authCheck.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authCheck.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/authFile.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authFile.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/authPam.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authPam.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/cache.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/cache.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/chunkFilter.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/chunkFilter.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/client.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/client.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/conn.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/conn.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/endpoint.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/endpoint.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/error.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/error.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/host.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/host.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/httpService.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/httpService.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/log.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/log.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/netConnector.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/netConnector.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/packet.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/packet.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/passHandler.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/passHandler.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/pipeline.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/pipeline.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/queue.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/queue.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/rangeFilter.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/rangeFilter.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/route.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/route.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/rx.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/rx.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/sendConnector.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/sendConnector.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/stage.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/stage.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/trace.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/trace.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/tx.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/tx.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/uploadFilter.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/uploadFilter.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/uri.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/uri.c

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/var.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/var.c

${LDFLAGS}${LDFLAGS}${CC} -shared -o ${CONFIG}/lib/libhttp.so ${LIBPATHS} ${CONFIG}/obj/auth.o ${CONFIG}/obj/authCheck.o ${CONFIG}/obj/authFile.o ${CONFIG}/obj/authPam.o ${CONFIG}/obj/cache.o ${CONFIG}/obj/chunkFilter.o ${CONFIG}/obj/client.o ${CONFIG}/obj/conn.o ${CONFIG}/obj/endpoint.o ${CONFIG}/obj/error.o ${CONFIG}/obj/host.o ${CONFIG}/obj/httpService.o ${CONFIG}/obj/log.o ${CONFIG}/obj/netConnector.o ${CONFIG}/obj/packet.o ${CONFIG}/obj/passHandler.o ${CONFIG}/obj/pipeline.o ${CONFIG}/obj/queue.o ${CONFIG}/obj/rangeFilter.o ${CONFIG}/obj/route.o ${CONFIG}/obj/rx.o ${CONFIG}/obj/sendConnector.o ${CONFIG}/obj/stage.o ${CONFIG}/obj/trace.o ${CONFIG}/obj/tx.o ${CONFIG}/obj/uploadFilter.o ${CONFIG}/obj/uri.o ${CONFIG}/obj/var.o ${LIBS} -lmpr -lpcre

${LDFLAGS}${LDFLAGS}${CC} -c -o ${CONFIG}/obj/http.o ${CFLAGS} -D_REENTRANT -DCPU=i686 -DPIC -DPIC -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/utils/http.c

${LDFLAGS}${LDFLAGS}${CC} -o ${CONFIG}/bin/http ${LIBPATHS} ${CONFIG}/obj/http.o ${LIBS} -lhttp -lmpr -lpcre 

