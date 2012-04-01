#
#   solaris-i686-debug.sh -- Build It Shell Script to build Http Library
#

PLATFORM="solaris-i686-debug"
CC="cc"
LD="ld"
CFLAGS="-Wall -fPIC -g -mcpu=i686"
DFLAGS="-D_REENTRANT -DCPU=i686 -DPIC"
IFLAGS="-Isolaris-i686-debug/inc -Isrc/deps/pcre -Isrc"
LDFLAGS="-g"
LIBPATHS="-L${PLATFORM}/lib"
LIBS="-llxnet -lrt -lsocket -lpthread -lm"

[ ! -x ${PLATFORM}/inc ] && mkdir -p ${PLATFORM}/inc ${PLATFORM}/obj ${PLATFORM}/lib ${PLATFORM}/bin
cp projects/buildConfig.${PLATFORM} ${PLATFORM}/inc/buildConfig.h

rm -rf solaris-i686-debug/inc/mpr.h
cp -r src/deps/mpr/mpr.h solaris-i686-debug/inc/mpr.h

rm -rf solaris-i686-debug/inc/mprSsl.h
cp -r src/deps/mpr/mprSsl.h solaris-i686-debug/inc/mprSsl.h

${CC} -c -o ${PLATFORM}/obj/mprLib.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

${CC} -shared -o ${PLATFORM}/lib/libmpr.so ${LDFLAGS} ${LIBPATHS} ${PLATFORM}/obj/mprLib.o ${LIBS}

${CC} -c -o ${PLATFORM}/obj/makerom.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

${CC} -o ${PLATFORM}/bin/makerom ${LDFLAGS} ${LIBPATHS} ${PLATFORM}/obj/makerom.o ${LIBS} -lmpr ${LDFLAGS}

${CC} -c -o ${PLATFORM}/obj/pcre.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

${CC} -shared -o ${PLATFORM}/lib/libpcre.so ${LDFLAGS} ${LIBPATHS} ${PLATFORM}/obj/pcre.o ${LIBS}

rm -rf solaris-i686-debug/inc/http.h
cp -r src/http.h solaris-i686-debug/inc/http.h

${CC} -c -o ${PLATFORM}/obj/auth.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/auth.c

${CC} -c -o ${PLATFORM}/obj/authCheck.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/authCheck.c

${CC} -c -o ${PLATFORM}/obj/authFile.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/authFile.c

${CC} -c -o ${PLATFORM}/obj/authPam.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/authPam.c

${CC} -c -o ${PLATFORM}/obj/cache.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/cache.c

${CC} -c -o ${PLATFORM}/obj/chunkFilter.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/chunkFilter.c

${CC} -c -o ${PLATFORM}/obj/client.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/client.c

${CC} -c -o ${PLATFORM}/obj/conn.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/conn.c

${CC} -c -o ${PLATFORM}/obj/endpoint.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/endpoint.c

${CC} -c -o ${PLATFORM}/obj/error.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/error.c

${CC} -c -o ${PLATFORM}/obj/host.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/host.c

${CC} -c -o ${PLATFORM}/obj/httpService.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/httpService.c

${CC} -c -o ${PLATFORM}/obj/log.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/log.c

${CC} -c -o ${PLATFORM}/obj/netConnector.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/netConnector.c

${CC} -c -o ${PLATFORM}/obj/packet.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/packet.c

${CC} -c -o ${PLATFORM}/obj/passHandler.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/passHandler.c

${CC} -c -o ${PLATFORM}/obj/pipeline.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/pipeline.c

${CC} -c -o ${PLATFORM}/obj/queue.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/queue.c

${CC} -c -o ${PLATFORM}/obj/rangeFilter.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/rangeFilter.c

${CC} -c -o ${PLATFORM}/obj/route.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/route.c

${CC} -c -o ${PLATFORM}/obj/rx.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/rx.c

${CC} -c -o ${PLATFORM}/obj/sendConnector.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/sendConnector.c

${CC} -c -o ${PLATFORM}/obj/stage.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/stage.c

${CC} -c -o ${PLATFORM}/obj/trace.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/trace.c

${CC} -c -o ${PLATFORM}/obj/tx.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/tx.c

${CC} -c -o ${PLATFORM}/obj/uploadFilter.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/uploadFilter.c

${CC} -c -o ${PLATFORM}/obj/uri.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/uri.c

${CC} -c -o ${PLATFORM}/obj/var.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/var.c

${CC} -shared -o ${PLATFORM}/lib/libhttp.so ${LDFLAGS} ${LIBPATHS} ${PLATFORM}/obj/auth.o ${PLATFORM}/obj/authCheck.o ${PLATFORM}/obj/authFile.o ${PLATFORM}/obj/authPam.o ${PLATFORM}/obj/cache.o ${PLATFORM}/obj/chunkFilter.o ${PLATFORM}/obj/client.o ${PLATFORM}/obj/conn.o ${PLATFORM}/obj/endpoint.o ${PLATFORM}/obj/error.o ${PLATFORM}/obj/host.o ${PLATFORM}/obj/httpService.o ${PLATFORM}/obj/log.o ${PLATFORM}/obj/netConnector.o ${PLATFORM}/obj/packet.o ${PLATFORM}/obj/passHandler.o ${PLATFORM}/obj/pipeline.o ${PLATFORM}/obj/queue.o ${PLATFORM}/obj/rangeFilter.o ${PLATFORM}/obj/route.o ${PLATFORM}/obj/rx.o ${PLATFORM}/obj/sendConnector.o ${PLATFORM}/obj/stage.o ${PLATFORM}/obj/trace.o ${PLATFORM}/obj/tx.o ${PLATFORM}/obj/uploadFilter.o ${PLATFORM}/obj/uri.o ${PLATFORM}/obj/var.o ${LIBS} -lmpr -lpcre

${CC} -c -o ${PLATFORM}/obj/http.o -Wall -fPIC ${LDFLAGS} -mcpu=i686 ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/utils/http.c

${CC} -o ${PLATFORM}/bin/http ${LDFLAGS} ${LIBPATHS} ${PLATFORM}/obj/http.o ${LIBS} -lhttp -lmpr -lpcre ${LDFLAGS}

