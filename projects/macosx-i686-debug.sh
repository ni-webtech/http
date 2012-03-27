#
#   build.sh -- Build It Shell Script to build Http Library
#

PLATFORM="macosx-i686-debug"
CC="cc"
CFLAGS="-fPIC -Wall -g"
DFLAGS="-DPIC -DCPU=I686"
IFLAGS="-Imacosx-i686-debug/inc -Isrc/deps/pcre -Isrc"
LDFLAGS="-Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -L${PLATFORM}/lib -g -ldl
LIBS="-lpthread -lm"

[ ! -x ${PLATFORM}/inc ] && mkdir -p ${PLATFORM}/inc ${PLATFORM}/obj ${PLATFORM}/lib ${PLATFORM}/bin
[ ! -f ${PLATFORM}/inc/buildConfig.h ] && cp projects/buildConfig.${PLATFORM} ${PLATFORM}/inc/buildConfig.h

rm -rf macosx-i686-debug/inc/mpr.h
cp -r src/deps/mpr/mpr.h macosx-i686-debug/inc/mpr.h

rm -rf macosx-i686-debug/inc/mprSsl.h
cp -r src/deps/mpr/mprSsl.h macosx-i686-debug/inc/mprSsl.h

${CC} -c -o ${PLATFORM}/obj/mprLib.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

${CC} -dynamiclib -o ${PLATFORM}/lib/libmpr.dylib -arch i686 ${LDFLAGS} -install_name @rpath/libmpr.dylib ${PLATFORM}/obj/mprLib.o ${LIBS}

${CC} -c -o ${PLATFORM}/obj/mprSsl.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc -I../../../../usr/include src/deps/mpr/mprSsl.c

${CC} -dynamiclib -o ${PLATFORM}/lib/libmprssl.dylib -arch i686 ${LDFLAGS} -L/usr/lib -install_name @rpath/libmprssl.dylib ${PLATFORM}/obj/mprSsl.o ${LIBS} -lmpr -lssl -lcrypto

${CC} -c -o ${PLATFORM}/obj/makerom.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

${CC} -o ${PLATFORM}/bin/makerom -arch i686 ${LDFLAGS} -L${PLATFORM}/lib ${PLATFORM}/obj/makerom.o ${LIBS} -lmpr

${CC} -c -o ${PLATFORM}/obj/pcre.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

${CC} -dynamiclib -o ${PLATFORM}/lib/libpcre.dylib -arch i686 ${LDFLAGS} -install_name @rpath/libpcre.dylib ${PLATFORM}/obj/pcre.o ${LIBS}

rm -rf macosx-i686-debug/inc/http.h
cp -r src/http.h macosx-i686-debug/inc/http.h

${CC} -c -o ${PLATFORM}/obj/auth.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/auth.c

${CC} -c -o ${PLATFORM}/obj/authCheck.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/authCheck.c

${CC} -c -o ${PLATFORM}/obj/authFile.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/authFile.c

${CC} -c -o ${PLATFORM}/obj/authPam.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/authPam.c

${CC} -c -o ${PLATFORM}/obj/cache.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/cache.c

${CC} -c -o ${PLATFORM}/obj/chunkFilter.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/chunkFilter.c

${CC} -c -o ${PLATFORM}/obj/client.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/client.c

${CC} -c -o ${PLATFORM}/obj/conn.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/conn.c

${CC} -c -o ${PLATFORM}/obj/endpoint.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/endpoint.c

${CC} -c -o ${PLATFORM}/obj/error.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/error.c

${CC} -c -o ${PLATFORM}/obj/host.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/host.c

${CC} -c -o ${PLATFORM}/obj/httpService.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/httpService.c

${CC} -c -o ${PLATFORM}/obj/log.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/log.c

${CC} -c -o ${PLATFORM}/obj/netConnector.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/netConnector.c

${CC} -c -o ${PLATFORM}/obj/packet.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/packet.c

${CC} -c -o ${PLATFORM}/obj/passHandler.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/passHandler.c

${CC} -c -o ${PLATFORM}/obj/pipeline.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/pipeline.c

${CC} -c -o ${PLATFORM}/obj/queue.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/queue.c

${CC} -c -o ${PLATFORM}/obj/rangeFilter.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/rangeFilter.c

${CC} -c -o ${PLATFORM}/obj/route.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/route.c

${CC} -c -o ${PLATFORM}/obj/rx.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/rx.c

${CC} -c -o ${PLATFORM}/obj/sendConnector.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/sendConnector.c

${CC} -c -o ${PLATFORM}/obj/stage.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/stage.c

${CC} -c -o ${PLATFORM}/obj/trace.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/trace.c

${CC} -c -o ${PLATFORM}/obj/tx.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/tx.c

${CC} -c -o ${PLATFORM}/obj/uploadFilter.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/uploadFilter.c

${CC} -c -o ${PLATFORM}/obj/uri.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/uri.c

${CC} -c -o ${PLATFORM}/obj/var.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/var.c

${CC} -dynamiclib -o ${PLATFORM}/lib/libhttp.dylib -arch i686 ${LDFLAGS} -L/usr/lib -install_name @rpath/libhttp.dylib ${PLATFORM}/obj/auth.o ${PLATFORM}/obj/authCheck.o ${PLATFORM}/obj/authFile.o ${PLATFORM}/obj/authPam.o ${PLATFORM}/obj/cache.o ${PLATFORM}/obj/chunkFilter.o ${PLATFORM}/obj/client.o ${PLATFORM}/obj/conn.o ${PLATFORM}/obj/endpoint.o ${PLATFORM}/obj/error.o ${PLATFORM}/obj/host.o ${PLATFORM}/obj/httpService.o ${PLATFORM}/obj/log.o ${PLATFORM}/obj/netConnector.o ${PLATFORM}/obj/packet.o ${PLATFORM}/obj/passHandler.o ${PLATFORM}/obj/pipeline.o ${PLATFORM}/obj/queue.o ${PLATFORM}/obj/rangeFilter.o ${PLATFORM}/obj/route.o ${PLATFORM}/obj/rx.o ${PLATFORM}/obj/sendConnector.o ${PLATFORM}/obj/stage.o ${PLATFORM}/obj/trace.o ${PLATFORM}/obj/tx.o ${PLATFORM}/obj/uploadFilter.o ${PLATFORM}/obj/uri.o ${PLATFORM}/obj/var.o ${LIBS} -lmpr -lpcre -lmprssl -lssl -lcrypto

${CC} -c -o ${PLATFORM}/obj/http.o -arch i686 ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/pcre -Isrc src/utils/http.c

${CC} -o ${PLATFORM}/bin/http -arch i686 ${LDFLAGS} -L/usr/lib -L${PLATFORM}/lib ${PLATFORM}/obj/http.o ${LIBS} -lhttp -lmpr -lpcre -lmprssl -lssl -lcrypto

