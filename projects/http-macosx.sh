#
#   http-macosx.sh -- Build It Shell Script to build Http Library
#

ARCH="x64"
ARCH="$(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/')"
OS="macosx"
PROFILE="debug"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="/usr/bin/clang"
LD="/usr/bin/ld"
CFLAGS="-Wall -Wno-deprecated-declarations -g -Wno-unused-result -Wshorten-64-to-32"
DFLAGS="-DBLD_DEBUG"
IFLAGS="-I${CONFIG}/inc -Isrc/deps/pcre -Isrc"
LDFLAGS="-Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -g"
LIBPATHS="-L${CONFIG}/bin"
LIBS="-lpthread -lm -ldl"

[ ! -x ${CONFIG}/inc ] && mkdir -p ${CONFIG}/inc ${CONFIG}/obj ${CONFIG}/lib ${CONFIG}/bin

[ ! -f ${CONFIG}/inc/bit.h ] && cp projects/http-${OS}-bit.h ${CONFIG}/inc/bit.h
if ! diff ${CONFIG}/inc/bit.h projects/http-${OS}-bit.h >/dev/null ; then
	cp projects/http-${OS}-bit.h ${CONFIG}/inc/bit.h
fi

rm -rf ${CONFIG}/inc/mpr.h
cp -r src/deps/mpr/mpr.h ${CONFIG}/inc/mpr.h

rm -rf ${CONFIG}/inc/mprSsl.h
cp -r src/deps/mpr/mprSsl.h ${CONFIG}/inc/mprSsl.h

${CC} -c -o ${CONFIG}/obj/mprLib.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

${CC} -dynamiclib -o ${CONFIG}/bin/libmpr.dylib -arch x86_64 ${LDFLAGS} -compatibility_version 1.0.0 -current_version 1.0.0 ${LIBPATHS} -install_name @rpath/libmpr.dylib ${CONFIG}/obj/mprLib.o ${LIBS}

${CC} -c -o ${CONFIG}/obj/mprSsl.o -arch x86_64 ${CFLAGS} ${DFLAGS} -DPOSIX -DMATRIX_USE_FILE_SYSTEM -I${CONFIG}/inc -Isrc/deps/pcre -Isrc -I../packages-macosx-x64/openssl/openssl-1.0.1b/include -I../packages-macosx-x64/matrixssl/matrixssl-3-3-open/matrixssl -I../packages-macosx-x64/matrixssl/matrixssl-3-3-open src/deps/mpr/mprSsl.c

${CC} -dynamiclib -o ${CONFIG}/bin/libmprssl.dylib -arch x86_64 ${LDFLAGS} -compatibility_version 1.0.0 -current_version 1.0.0 ${LIBPATHS} -L../packages-macosx-x64/openssl/openssl-1.0.1b -L../packages-macosx-x64/matrixssl/matrixssl-3-3-open -install_name @rpath/libmprssl.dylib ${CONFIG}/obj/mprSsl.o ${LIBS} -lmpr -lssl -lcrypto -lmatrixssl

${CC} -c -o ${CONFIG}/obj/makerom.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

${CC} -o ${CONFIG}/bin/makerom -arch x86_64 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/makerom.o ${LIBS} -lmpr

${CC} -c -o ${CONFIG}/obj/pcre.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

${CC} -dynamiclib -o ${CONFIG}/bin/libpcre.dylib -arch x86_64 ${LDFLAGS} ${LIBPATHS} -install_name @rpath/libpcre.dylib ${CONFIG}/obj/pcre.o ${LIBS}

rm -rf ${CONFIG}/inc/http.h
cp -r src/http.h ${CONFIG}/inc/http.h

${CC} -c -o ${CONFIG}/obj/auth.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/auth.c

${CC} -c -o ${CONFIG}/obj/authCheck.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authCheck.c

${CC} -c -o ${CONFIG}/obj/authFile.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authFile.c

${CC} -c -o ${CONFIG}/obj/authPam.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authPam.c

${CC} -c -o ${CONFIG}/obj/cache.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/cache.c

${CC} -c -o ${CONFIG}/obj/chunkFilter.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/chunkFilter.c

${CC} -c -o ${CONFIG}/obj/client.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/client.c

${CC} -c -o ${CONFIG}/obj/conn.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/conn.c

${CC} -c -o ${CONFIG}/obj/endpoint.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/endpoint.c

${CC} -c -o ${CONFIG}/obj/error.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/error.c

${CC} -c -o ${CONFIG}/obj/host.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/host.c

${CC} -c -o ${CONFIG}/obj/httpService.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/httpService.c

${CC} -c -o ${CONFIG}/obj/log.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/log.c

${CC} -c -o ${CONFIG}/obj/netConnector.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/netConnector.c

${CC} -c -o ${CONFIG}/obj/packet.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/packet.c

${CC} -c -o ${CONFIG}/obj/passHandler.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/passHandler.c

${CC} -c -o ${CONFIG}/obj/pipeline.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/pipeline.c

${CC} -c -o ${CONFIG}/obj/queue.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/queue.c

${CC} -c -o ${CONFIG}/obj/rangeFilter.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/rangeFilter.c

${CC} -c -o ${CONFIG}/obj/route.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/route.c

${CC} -c -o ${CONFIG}/obj/rx.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/rx.c

${CC} -c -o ${CONFIG}/obj/sendConnector.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/sendConnector.c

${CC} -c -o ${CONFIG}/obj/stage.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/stage.c

${CC} -c -o ${CONFIG}/obj/trace.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/trace.c

${CC} -c -o ${CONFIG}/obj/tx.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/tx.c

${CC} -c -o ${CONFIG}/obj/uploadFilter.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/uploadFilter.c

${CC} -c -o ${CONFIG}/obj/uri.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/uri.c

${CC} -c -o ${CONFIG}/obj/var.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/var.c

${CC} -dynamiclib -o ${CONFIG}/bin/libhttp.dylib -arch x86_64 ${LDFLAGS} -compatibility_version 1.0.0 -current_version 1.0.0 ${LIBPATHS} -L../packages-macosx-x64/openssl/openssl-1.0.1b -L../packages-macosx-x64/matrixssl/matrixssl-3-3-open -install_name @rpath/libhttp.dylib ${CONFIG}/obj/auth.o ${CONFIG}/obj/authCheck.o ${CONFIG}/obj/authFile.o ${CONFIG}/obj/authPam.o ${CONFIG}/obj/cache.o ${CONFIG}/obj/chunkFilter.o ${CONFIG}/obj/client.o ${CONFIG}/obj/conn.o ${CONFIG}/obj/endpoint.o ${CONFIG}/obj/error.o ${CONFIG}/obj/host.o ${CONFIG}/obj/httpService.o ${CONFIG}/obj/log.o ${CONFIG}/obj/netConnector.o ${CONFIG}/obj/packet.o ${CONFIG}/obj/passHandler.o ${CONFIG}/obj/pipeline.o ${CONFIG}/obj/queue.o ${CONFIG}/obj/rangeFilter.o ${CONFIG}/obj/route.o ${CONFIG}/obj/rx.o ${CONFIG}/obj/sendConnector.o ${CONFIG}/obj/stage.o ${CONFIG}/obj/trace.o ${CONFIG}/obj/tx.o ${CONFIG}/obj/uploadFilter.o ${CONFIG}/obj/uri.o ${CONFIG}/obj/var.o ${LIBS} -lmpr -lpcre -lmprssl -lssl -lcrypto -lmatrixssl

${CC} -c -o ${CONFIG}/obj/http.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/utils/http.c

${CC} -o ${CONFIG}/bin/http -arch x86_64 ${LDFLAGS} ${LIBPATHS} -L../packages-macosx-x64/openssl/openssl-1.0.1b -L../packages-macosx-x64/matrixssl/matrixssl-3-3-open ${CONFIG}/obj/http.o ${LIBS} -lhttp -lmpr -lpcre -lmprssl -lssl -lcrypto -lmatrixssl

