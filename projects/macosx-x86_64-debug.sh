#
#   build.sh -- Build It Shell Script to build Http Library
#

CC="/usr/bin/cc"
CFLAGS="-fPIC -Wall -g -Wshorten-64-to-32"
DFLAGS="-DPIC -DCPU=X86_64"
IFLAGS="-Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc"
LDFLAGS="-Wl,-rpath,@executable_path/../lib -Wl,-rpath,@executable_path/ -Wl,-rpath,@loader_path/ -L/Users/mob/git/http/macosx-x86_64-debug/lib -g -ldl"
LIBS="-lpthread -lm"

${CC} -c -o macosx-x86_64-debug/obj/mprLib.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/deps/mpr/mprLib.c

${CC} -dynamiclib -o macosx-x86_64-debug/lib/libmpr.dylib -arch x86_64 ${LDFLAGS} -install_name @rpath/libmpr.dylib macosx-x86_64-debug/obj/mprLib.o ${LIBS}

${CC} -c -o macosx-x86_64-debug/obj/manager.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/deps/mpr/manager.c

${CC} -o macosx-x86_64-debug/bin/manager -arch x86_64 ${LDFLAGS} -Lmacosx-x86_64-debug/lib macosx-x86_64-debug/obj/manager.o ${LIBS} -lmpr

${CC} -c -o macosx-x86_64-debug/obj/makerom.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/deps/mpr/makerom.c

${CC} -o macosx-x86_64-debug/bin/makerom -arch x86_64 ${LDFLAGS} -Lmacosx-x86_64-debug/lib macosx-x86_64-debug/obj/makerom.o ${LIBS} -lmpr

${CC} -c -o macosx-x86_64-debug/obj/pcre.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/deps/pcre/pcre.c

${CC} -dynamiclib -o macosx-x86_64-debug/lib/libpcre.dylib -arch x86_64 ${LDFLAGS} -install_name @rpath/libpcre.dylib macosx-x86_64-debug/obj/pcre.o ${LIBS}

${CC} -c -o macosx-x86_64-debug/obj/auth.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/auth.c

${CC} -c -o macosx-x86_64-debug/obj/authCheck.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/authCheck.c

${CC} -c -o macosx-x86_64-debug/obj/authFile.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/authFile.c

${CC} -c -o macosx-x86_64-debug/obj/authPam.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/authPam.c

${CC} -c -o macosx-x86_64-debug/obj/cache.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/cache.c

${CC} -c -o macosx-x86_64-debug/obj/chunkFilter.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/chunkFilter.c

${CC} -c -o macosx-x86_64-debug/obj/client.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/client.c

${CC} -c -o macosx-x86_64-debug/obj/conn.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/conn.c

${CC} -c -o macosx-x86_64-debug/obj/endpoint.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/endpoint.c

${CC} -c -o macosx-x86_64-debug/obj/error.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/error.c

${CC} -c -o macosx-x86_64-debug/obj/host.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/host.c

${CC} -c -o macosx-x86_64-debug/obj/httpService.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/httpService.c

${CC} -c -o macosx-x86_64-debug/obj/log.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/log.c

${CC} -c -o macosx-x86_64-debug/obj/netConnector.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/netConnector.c

${CC} -c -o macosx-x86_64-debug/obj/packet.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/packet.c

${CC} -c -o macosx-x86_64-debug/obj/passHandler.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/passHandler.c

${CC} -c -o macosx-x86_64-debug/obj/pipeline.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/pipeline.c

${CC} -c -o macosx-x86_64-debug/obj/queue.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/queue.c

${CC} -c -o macosx-x86_64-debug/obj/rangeFilter.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/rangeFilter.c

${CC} -c -o macosx-x86_64-debug/obj/route.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/route.c

${CC} -c -o macosx-x86_64-debug/obj/rx.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/rx.c

${CC} -c -o macosx-x86_64-debug/obj/sendConnector.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/sendConnector.c

${CC} -c -o macosx-x86_64-debug/obj/stage.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/stage.c

${CC} -c -o macosx-x86_64-debug/obj/trace.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/trace.c

${CC} -c -o macosx-x86_64-debug/obj/tx.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/tx.c

${CC} -c -o macosx-x86_64-debug/obj/uploadFilter.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/uploadFilter.c

${CC} -c -o macosx-x86_64-debug/obj/uri.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/uri.c

${CC} -c -o macosx-x86_64-debug/obj/var.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/var.c

${CC} -dynamiclib -o macosx-x86_64-debug/lib/libhttp.dylib -arch x86_64 ${LDFLAGS} -install_name @rpath/libhttp.dylib macosx-x86_64-debug/obj/auth.o macosx-x86_64-debug/obj/authCheck.o macosx-x86_64-debug/obj/authFile.o macosx-x86_64-debug/obj/authPam.o macosx-x86_64-debug/obj/cache.o macosx-x86_64-debug/obj/chunkFilter.o macosx-x86_64-debug/obj/client.o macosx-x86_64-debug/obj/conn.o macosx-x86_64-debug/obj/endpoint.o macosx-x86_64-debug/obj/error.o macosx-x86_64-debug/obj/host.o macosx-x86_64-debug/obj/httpService.o macosx-x86_64-debug/obj/log.o macosx-x86_64-debug/obj/netConnector.o macosx-x86_64-debug/obj/packet.o macosx-x86_64-debug/obj/passHandler.o macosx-x86_64-debug/obj/pipeline.o macosx-x86_64-debug/obj/queue.o macosx-x86_64-debug/obj/rangeFilter.o macosx-x86_64-debug/obj/route.o macosx-x86_64-debug/obj/rx.o macosx-x86_64-debug/obj/sendConnector.o macosx-x86_64-debug/obj/stage.o macosx-x86_64-debug/obj/trace.o macosx-x86_64-debug/obj/tx.o macosx-x86_64-debug/obj/uploadFilter.o macosx-x86_64-debug/obj/uri.o macosx-x86_64-debug/obj/var.o ${LIBS} -lmpr -lpcre

${CC} -c -o macosx-x86_64-debug/obj/http.o -arch x86_64 ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Imacosx-x86_64-debug/inc src/utils/http.c

${CC} -o macosx-x86_64-debug/bin/http -arch x86_64 ${LDFLAGS} -Lmacosx-x86_64-debug/lib macosx-x86_64-debug/obj/http.o ${LIBS} -lhttp -lmpr -lpcre

