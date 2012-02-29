#
#   build.sh -- Build It Shell Script to build Http Library
#

CC="cc"
CFLAGS="-fPIC -Wall -g"
DFLAGS="-DPIC -DCPU=I686"
IFLAGS="-Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc"
LDFLAGS="-Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -L/Users/mob/git/http/macosx-i686-debug/lib -g"
LIBS="-lpthread -lm"

${CC} -c -o macosx-i686-debug/obj/mprLib.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/deps/mpr/mprLib.c

${CC} -dynamiclib -o macosx-i686-debug/lib/libmpr.dylib -arch i686 ${LDFLAGS} -install_name @rpath/libmpr.dylib macosx-i686-debug/obj/mprLib.o ${LIBS}

${CC} -c -o macosx-i686-debug/obj/manager.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/deps/mpr/manager.c

${CC} -o macosx-i686-debug/bin/manager -arch i686 ${LDFLAGS} -Lmacosx-i686-debug/lib macosx-i686-debug/obj/manager.o ${LIBS} -lmpr

${CC} -c -o macosx-i686-debug/obj/makerom.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/deps/mpr/makerom.c

${CC} -o macosx-i686-debug/bin/makerom -arch i686 ${LDFLAGS} -Lmacosx-i686-debug/lib macosx-i686-debug/obj/makerom.o ${LIBS} -lmpr

${CC} -c -o macosx-i686-debug/obj/pcre.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/deps/pcre/pcre.c

${CC} -dynamiclib -o macosx-i686-debug/lib/libpcre.dylib -arch i686 ${LDFLAGS} -install_name @rpath/libpcre.dylib macosx-i686-debug/obj/pcre.o ${LIBS}

${CC} -c -o macosx-i686-debug/obj/auth.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/auth.c

${CC} -c -o macosx-i686-debug/obj/authCheck.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/authCheck.c

${CC} -c -o macosx-i686-debug/obj/authFile.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/authFile.c

${CC} -c -o macosx-i686-debug/obj/authPam.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/authPam.c

${CC} -c -o macosx-i686-debug/obj/cache.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/cache.c

${CC} -c -o macosx-i686-debug/obj/chunkFilter.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/chunkFilter.c

${CC} -c -o macosx-i686-debug/obj/client.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/client.c

${CC} -c -o macosx-i686-debug/obj/conn.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/conn.c

${CC} -c -o macosx-i686-debug/obj/endpoint.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/endpoint.c

${CC} -c -o macosx-i686-debug/obj/error.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/error.c

${CC} -c -o macosx-i686-debug/obj/host.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/host.c

${CC} -c -o macosx-i686-debug/obj/httpService.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/httpService.c

${CC} -c -o macosx-i686-debug/obj/log.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/log.c

${CC} -c -o macosx-i686-debug/obj/netConnector.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/netConnector.c

${CC} -c -o macosx-i686-debug/obj/packet.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/packet.c

${CC} -c -o macosx-i686-debug/obj/passHandler.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/passHandler.c

${CC} -c -o macosx-i686-debug/obj/pipeline.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/pipeline.c

${CC} -c -o macosx-i686-debug/obj/queue.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/queue.c

${CC} -c -o macosx-i686-debug/obj/rangeFilter.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/rangeFilter.c

${CC} -c -o macosx-i686-debug/obj/route.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/route.c

${CC} -c -o macosx-i686-debug/obj/rx.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/rx.c

${CC} -c -o macosx-i686-debug/obj/sendConnector.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/sendConnector.c

${CC} -c -o macosx-i686-debug/obj/stage.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/stage.c

${CC} -c -o macosx-i686-debug/obj/trace.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/trace.c

${CC} -c -o macosx-i686-debug/obj/tx.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/tx.c

${CC} -c -o macosx-i686-debug/obj/uploadFilter.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/uploadFilter.c

${CC} -c -o macosx-i686-debug/obj/uri.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/uri.c

${CC} -c -o macosx-i686-debug/obj/var.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/var.c

${CC} -dynamiclib -o macosx-i686-debug/lib/libhttp.dylib -arch i686 ${LDFLAGS} -install_name @rpath/libhttp.dylib macosx-i686-debug/obj/auth.o macosx-i686-debug/obj/authCheck.o macosx-i686-debug/obj/authFile.o macosx-i686-debug/obj/authPam.o macosx-i686-debug/obj/cache.o macosx-i686-debug/obj/chunkFilter.o macosx-i686-debug/obj/client.o macosx-i686-debug/obj/conn.o macosx-i686-debug/obj/endpoint.o macosx-i686-debug/obj/error.o macosx-i686-debug/obj/host.o macosx-i686-debug/obj/httpService.o macosx-i686-debug/obj/log.o macosx-i686-debug/obj/netConnector.o macosx-i686-debug/obj/packet.o macosx-i686-debug/obj/passHandler.o macosx-i686-debug/obj/pipeline.o macosx-i686-debug/obj/queue.o macosx-i686-debug/obj/rangeFilter.o macosx-i686-debug/obj/route.o macosx-i686-debug/obj/rx.o macosx-i686-debug/obj/sendConnector.o macosx-i686-debug/obj/stage.o macosx-i686-debug/obj/trace.o macosx-i686-debug/obj/tx.o macosx-i686-debug/obj/uploadFilter.o macosx-i686-debug/obj/uri.o macosx-i686-debug/obj/var.o ${LIBS} -lmpr -lpcre

${CC} -c -o macosx-i686-debug/obj/http.o -arch i686 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-i686-debug/inc src/utils/http.c

${CC} -o macosx-i686-debug/bin/http -arch i686 ${LDFLAGS} -Lmacosx-i686-debug/lib macosx-i686-debug/obj/http.o ${LIBS} -lhttp -lmpr -lpcre

