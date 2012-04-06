#
#   macosx-x86_64-debug.sh -- Build It Shell Script to build Http Library
#

CONFIG="macosx-x86_64-debug"
CC="/usr/bin/cc"
LD="/usr/bin/ld"
CFLAGS="-fPIC -Wall -fast -Wshorten-64-to-32"
DFLAGS="-DPIC -DCPU=X86_64"
IFLAGS="-Imacosx-x86_64-debug/inc -Imacosx-x86_64-debug/inc -Isrc/deps/pcre -Isrc"
LDFLAGS="-Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/"
LIBPATHS="-L${CONFIG}/lib"
LIBS="-lpthread -lm -ldl"

[ ! -x ${CONFIG}/inc ] && mkdir -p ${CONFIG}/inc ${CONFIG}/obj ${CONFIG}/lib ${CONFIG}/bin
cp projects/buildConfig.${CONFIG} ${CONFIG}/inc/buildConfig.h

rm -rf macosx-x86_64-debug/inc/mpr.h
cp -r src/deps/mpr/mpr.h macosx-x86_64-debug/inc/mpr.h

rm -rf macosx-x86_64-debug/inc/mprSsl.h
cp -r src/deps/mpr/mprSsl.h macosx-x86_64-debug/inc/mprSsl.h

${CC} -c -o ${CONFIG}/obj/mprLib.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

${CC} -dynamiclib -o ${CONFIG}/lib/libmpr.dylib -arch x86_64 ${LDFLAGS} ${LIBPATHS} -install_name @rpath/libmpr.dylib ${CONFIG}/obj/mprLib.o ${LIBS}

${CC} -c -o ${CONFIG}/obj/mprSsl.o -arch x86_64 ${CFLAGS} ${DFLAGS} -DPOSIX -DMATRIX_USE_FILE_SYSTEM -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc -I../packages-macosx-x86_64/openssl/openssl-1.0.0d/include -I../packages-macosx-x86_64/matrixssl/matrixssl-3-3-open/matrixssl -I../packages-macosx-x86_64/matrixssl/matrixssl-3-3-open src/deps/mpr/mprSsl.c

${CC} -dynamiclib -o ${CONFIG}/lib/libmprssl.dylib -arch x86_64 ${LDFLAGS} ${LIBPATHS} -L/Users/mob/git/packages-macosx-x86_64/openssl/openssl-1.0.0d -L/Users/mob/git/packages-macosx-x86_64/matrixssl/matrixssl-3-3-open -install_name @rpath/libmprssl.dylib ${CONFIG}/obj/mprSsl.o ${LIBS} -lmpr -lssl -lcrypto -lmatrixssl

${CC} -c -o ${CONFIG}/obj/makerom.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

${CC} -o ${CONFIG}/bin/makerom -arch x86_64 ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/makerom.o ${LIBS} -lmpr

${CC} -c -o ${CONFIG}/obj/pcre.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

${CC} -dynamiclib -o ${CONFIG}/lib/libpcre.dylib -arch x86_64 ${LDFLAGS} ${LIBPATHS} -install_name @rpath/libpcre.dylib ${CONFIG}/obj/pcre.o ${LIBS}

rm -rf macosx-x86_64-debug/inc/http.h
cp -r src/http.h macosx-x86_64-debug/inc/http.h

${CC} -c -o ${CONFIG}/obj/auth.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/auth.c

${CC} -c -o ${CONFIG}/obj/authCheck.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authCheck.c

${CC} -c -o ${CONFIG}/obj/authFile.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authFile.c

${CC} -c -o ${CONFIG}/obj/authPam.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authPam.c

${CC} -c -o ${CONFIG}/obj/cache.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/cache.c

${CC} -c -o ${CONFIG}/obj/chunkFilter.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/chunkFilter.c

${CC} -c -o ${CONFIG}/obj/client.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/client.c

${CC} -c -o ${CONFIG}/obj/conn.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/conn.c

${CC} -c -o ${CONFIG}/obj/endpoint.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/endpoint.c

${CC} -c -o ${CONFIG}/obj/error.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/error.c

${CC} -c -o ${CONFIG}/obj/host.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/host.c

${CC} -c -o ${CONFIG}/obj/httpService.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/httpService.c

${CC} -c -o ${CONFIG}/obj/log.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/log.c

${CC} -c -o ${CONFIG}/obj/netConnector.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/netConnector.c

${CC} -c -o ${CONFIG}/obj/packet.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/packet.c

${CC} -c -o ${CONFIG}/obj/passHandler.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/passHandler.c

${CC} -c -o ${CONFIG}/obj/pipeline.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/pipeline.c

${CC} -c -o ${CONFIG}/obj/queue.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/queue.c

${CC} -c -o ${CONFIG}/obj/rangeFilter.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/rangeFilter.c

${CC} -c -o ${CONFIG}/obj/route.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/route.c

${CC} -c -o ${CONFIG}/obj/rx.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/rx.c

${CC} -c -o ${CONFIG}/obj/sendConnector.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/sendConnector.c

${CC} -c -o ${CONFIG}/obj/stage.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/stage.c

${CC} -c -o ${CONFIG}/obj/trace.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/trace.c

${CC} -c -o ${CONFIG}/obj/tx.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/tx.c

${CC} -c -o ${CONFIG}/obj/uploadFilter.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/uploadFilter.c

${CC} -c -o ${CONFIG}/obj/uri.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/uri.c

${CC} -c -o ${CONFIG}/obj/var.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/var.c

${CC} -dynamiclib -o ${CONFIG}/lib/libhttp.dylib -arch x86_64 ${LDFLAGS} ${LIBPATHS} -L/Users/mob/git/packages-macosx-x86_64/openssl/openssl-1.0.0d -L/Users/mob/git/packages-macosx-x86_64/matrixssl/matrixssl-3-3-open -install_name @rpath/libhttp.dylib ${CONFIG}/obj/auth.o ${CONFIG}/obj/authCheck.o ${CONFIG}/obj/authFile.o ${CONFIG}/obj/authPam.o ${CONFIG}/obj/cache.o ${CONFIG}/obj/chunkFilter.o ${CONFIG}/obj/client.o ${CONFIG}/obj/conn.o ${CONFIG}/obj/endpoint.o ${CONFIG}/obj/error.o ${CONFIG}/obj/host.o ${CONFIG}/obj/httpService.o ${CONFIG}/obj/log.o ${CONFIG}/obj/netConnector.o ${CONFIG}/obj/packet.o ${CONFIG}/obj/passHandler.o ${CONFIG}/obj/pipeline.o ${CONFIG}/obj/queue.o ${CONFIG}/obj/rangeFilter.o ${CONFIG}/obj/route.o ${CONFIG}/obj/rx.o ${CONFIG}/obj/sendConnector.o ${CONFIG}/obj/stage.o ${CONFIG}/obj/trace.o ${CONFIG}/obj/tx.o ${CONFIG}/obj/uploadFilter.o ${CONFIG}/obj/uri.o ${CONFIG}/obj/var.o ${LIBS} -lmpr -lpcre -lmprssl -lssl -lcrypto -lmatrixssl

${CC} -c -o ${CONFIG}/obj/http.o -arch x86_64 ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/utils/http.c

${CC} -o ${CONFIG}/bin/http -arch x86_64 ${LDFLAGS} ${LIBPATHS} -L/Users/mob/git/packages-macosx-x86_64/openssl/openssl-1.0.0d -L/Users/mob/git/packages-macosx-x86_64/matrixssl/matrixssl-3-3-open ${CONFIG}/obj/http.o ${LIBS} -lhttp -lmpr -lpcre -lmprssl -lssl -lcrypto -lmatrixssl

