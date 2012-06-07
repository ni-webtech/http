#
#   http-macosx.sh -- Build It Shell Script to build Http Library
#

ARCH="x64"
ARCH="$(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/')"
OS="macosx"
PROFILE="xcode"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="/usr/bin/clang"
LD="/usr/bin/ld"
CFLAGS="-Wall -Wno-deprecated-declarations -O3 -Wno-unused-result -Wshorten-64-to-32"
DFLAGS=""
IFLAGS="-I${CONFIG}/inc -Isrc"
LDFLAGS="-Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/"
LIBPATHS="-L${CONFIG}/bin"
LIBS="-lpthread -lm -ldl"

[ ! -x ${CONFIG}/inc ] && mkdir -p ${CONFIG}/inc ${CONFIG}/obj ${CONFIG}/lib ${CONFIG}/bin

[ ! -f ${CONFIG}/inc/bit.h ] && cp projects/http-${OS}-bit.h ${CONFIG}/inc/bit.h
if ! diff ${CONFIG}/inc/bit.h projects/http-${OS}-bit.h >/dev/null ; then
	cp projects/http-${OS}-bit.h ${CONFIG}/inc/bit.h
fi

rm -rf ${CONFIG}/inc/mpr.h
cp -r src/deps/mpr/mpr.h ${CONFIG}/inc/mpr.h

${DFLAGS}${CC} -c -o ${CONFIG}/obj/mprLib.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/deps/mpr/mprLib.c

${DFLAGS}${CC} -dynamiclib -o ${CONFIG}/bin/libmpr.dylib -arch x86_64 ${LDFLAGS} -compatibility_version 1.0.0 -current_version 1.0.0 ${LIBPATHS} -install_name @rpath/libmpr.dylib ${CONFIG}/obj/mprLib.o ${LIBS}

${DFLAGS}${CC} -c -o ${CONFIG}/obj/mprSsl.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/deps/mpr/mprSsl.c

${DFLAGS}${CC} -dynamiclib -o ${CONFIG}/bin/libmprssl.dylib -arch x86_64 ${LDFLAGS} -compatibility_version 1.0.0 -current_version 1.0.0 ${LIBPATHS} -install_name @rpath/libmprssl.dylib ${CONFIG}/obj/mprSsl.o ${LIBS} -lmpr

${DFLAGS}${CC} -c -o ${CONFIG}/obj/makerom.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/deps/mpr/makerom.c

${DFLAGS}${CC} -o ${CONFIG}/bin/makerom -arch x86_64 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/makerom.o ${LIBS} -lmpr

rm -rf ${CONFIG}/inc/pcre.h
cp -r src/deps/pcre/pcre.h ${CONFIG}/inc/pcre.h

${DFLAGS}${CC} -c -o ${CONFIG}/obj/pcre.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/deps/pcre/pcre.c

${DFLAGS}${CC} -dynamiclib -o ${CONFIG}/bin/libpcre.dylib -arch x86_64 ${LDFLAGS} ${LIBPATHS} -install_name @rpath/libpcre.dylib ${CONFIG}/obj/pcre.o ${LIBS}

rm -rf ${CONFIG}/inc/http.h
cp -r src/http.h ${CONFIG}/inc/http.h

${DFLAGS}${CC} -c -o ${CONFIG}/obj/auth.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/auth.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/authCheck.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/authCheck.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/authFile.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/authFile.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/authPam.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/authPam.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/cache.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/cache.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/chunkFilter.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/chunkFilter.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/client.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/client.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/conn.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/conn.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/endpoint.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/endpoint.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/error.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/error.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/host.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/host.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/httpService.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/httpService.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/log.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/log.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/netConnector.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/netConnector.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/packet.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/packet.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/passHandler.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/passHandler.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/pipeline.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/pipeline.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/queue.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/queue.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/rangeFilter.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/rangeFilter.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/route.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/route.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/rx.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/rx.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/sendConnector.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/sendConnector.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/stage.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/stage.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/trace.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/trace.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/tx.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/tx.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/uploadFilter.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/uploadFilter.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/uri.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/uri.c

${DFLAGS}${CC} -c -o ${CONFIG}/obj/var.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/var.c

${DFLAGS}${CC} -dynamiclib -o ${CONFIG}/bin/libhttp.dylib -arch x86_64 ${LDFLAGS} ${LIBPATHS} -install_name @rpath/libhttp.dylib ${CONFIG}/obj/auth.o ${CONFIG}/obj/authCheck.o ${CONFIG}/obj/authFile.o ${CONFIG}/obj/authPam.o ${CONFIG}/obj/cache.o ${CONFIG}/obj/chunkFilter.o ${CONFIG}/obj/client.o ${CONFIG}/obj/conn.o ${CONFIG}/obj/endpoint.o ${CONFIG}/obj/error.o ${CONFIG}/obj/host.o ${CONFIG}/obj/httpService.o ${CONFIG}/obj/log.o ${CONFIG}/obj/netConnector.o ${CONFIG}/obj/packet.o ${CONFIG}/obj/passHandler.o ${CONFIG}/obj/pipeline.o ${CONFIG}/obj/queue.o ${CONFIG}/obj/rangeFilter.o ${CONFIG}/obj/route.o ${CONFIG}/obj/rx.o ${CONFIG}/obj/sendConnector.o ${CONFIG}/obj/stage.o ${CONFIG}/obj/trace.o ${CONFIG}/obj/tx.o ${CONFIG}/obj/uploadFilter.o ${CONFIG}/obj/uri.o ${CONFIG}/obj/var.o ${LIBS} -lmpr -lpcre -lmprssl -lpam

${DFLAGS}${CC} -c -o ${CONFIG}/obj/http.o -arch x86_64 ${CFLAGS} -I${CONFIG}/inc -Isrc src/utils/http.c

${DFLAGS}${CC} -o ${CONFIG}/bin/http -arch x86_64 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/http.o ${LIBS} -lhttp -lmpr -lpcre -lmprssl

