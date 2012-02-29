#
#   build.sh -- Build It Shell Script to build Http Library
#

CC="cc"
CFLAGS="-fPIC -g -mcpu=i686"
DFLAGS="-DPIC"
IFLAGS="-Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc"
LDFLAGS="-L/Users/mob/git/http/solaris-i686-debug/lib -g"
LIBS="-lpthread -lm"

${CC} -c -o solaris-i686-debug/obj/mprLib.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/deps/mpr/mprLib.c

${CC} -shared -o solaris-i686-debug/lib/libmpr.so ${LDFLAGS} solaris-i686-debug/obj/mprLib.o ${LIBS}

${CC} -c -o solaris-i686-debug/obj/manager.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/deps/mpr/manager.c

${CC} -o solaris-i686-debug/bin/manager ${LDFLAGS} -Lsolaris-i686-debug/lib solaris-i686-debug/obj/manager.o ${LIBS} -lmpr -Lsolaris-i686-debug/lib -g

${CC} -c -o solaris-i686-debug/obj/makerom.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/deps/mpr/makerom.c

${CC} -o solaris-i686-debug/bin/makerom ${LDFLAGS} -Lsolaris-i686-debug/lib solaris-i686-debug/obj/makerom.o ${LIBS} -lmpr -Lsolaris-i686-debug/lib -g

${CC} -c -o solaris-i686-debug/obj/pcre.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/deps/pcre/pcre.c

${CC} -shared -o solaris-i686-debug/lib/libpcre.so ${LDFLAGS} solaris-i686-debug/obj/pcre.o ${LIBS}

${CC} -c -o solaris-i686-debug/obj/auth.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/auth.c

${CC} -c -o solaris-i686-debug/obj/authCheck.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/authCheck.c

${CC} -c -o solaris-i686-debug/obj/authFile.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/authFile.c

${CC} -c -o solaris-i686-debug/obj/authPam.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/authPam.c

${CC} -c -o solaris-i686-debug/obj/cache.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/cache.c

${CC} -c -o solaris-i686-debug/obj/chunkFilter.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/chunkFilter.c

${CC} -c -o solaris-i686-debug/obj/client.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/client.c

${CC} -c -o solaris-i686-debug/obj/conn.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/conn.c

${CC} -c -o solaris-i686-debug/obj/endpoint.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/endpoint.c

${CC} -c -o solaris-i686-debug/obj/error.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/error.c

${CC} -c -o solaris-i686-debug/obj/host.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/host.c

${CC} -c -o solaris-i686-debug/obj/httpService.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/httpService.c

${CC} -c -o solaris-i686-debug/obj/log.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/log.c

${CC} -c -o solaris-i686-debug/obj/netConnector.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/netConnector.c

${CC} -c -o solaris-i686-debug/obj/packet.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/packet.c

${CC} -c -o solaris-i686-debug/obj/passHandler.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/passHandler.c

${CC} -c -o solaris-i686-debug/obj/pipeline.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/pipeline.c

${CC} -c -o solaris-i686-debug/obj/queue.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/queue.c

${CC} -c -o solaris-i686-debug/obj/rangeFilter.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/rangeFilter.c

${CC} -c -o solaris-i686-debug/obj/route.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/route.c

${CC} -c -o solaris-i686-debug/obj/rx.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/rx.c

${CC} -c -o solaris-i686-debug/obj/sendConnector.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/sendConnector.c

${CC} -c -o solaris-i686-debug/obj/stage.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/stage.c

${CC} -c -o solaris-i686-debug/obj/trace.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/trace.c

${CC} -c -o solaris-i686-debug/obj/tx.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/tx.c

${CC} -c -o solaris-i686-debug/obj/uploadFilter.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/uploadFilter.c

${CC} -c -o solaris-i686-debug/obj/uri.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/uri.c

${CC} -c -o solaris-i686-debug/obj/var.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/var.c

${CC} -shared -o solaris-i686-debug/lib/libhttp.so ${LDFLAGS} solaris-i686-debug/obj/auth.o solaris-i686-debug/obj/authCheck.o solaris-i686-debug/obj/authFile.o solaris-i686-debug/obj/authPam.o solaris-i686-debug/obj/cache.o solaris-i686-debug/obj/chunkFilter.o solaris-i686-debug/obj/client.o solaris-i686-debug/obj/conn.o solaris-i686-debug/obj/endpoint.o solaris-i686-debug/obj/error.o solaris-i686-debug/obj/host.o solaris-i686-debug/obj/httpService.o solaris-i686-debug/obj/log.o solaris-i686-debug/obj/netConnector.o solaris-i686-debug/obj/packet.o solaris-i686-debug/obj/passHandler.o solaris-i686-debug/obj/pipeline.o solaris-i686-debug/obj/queue.o solaris-i686-debug/obj/rangeFilter.o solaris-i686-debug/obj/route.o solaris-i686-debug/obj/rx.o solaris-i686-debug/obj/sendConnector.o solaris-i686-debug/obj/stage.o solaris-i686-debug/obj/trace.o solaris-i686-debug/obj/tx.o solaris-i686-debug/obj/uploadFilter.o solaris-i686-debug/obj/uri.o solaris-i686-debug/obj/var.o ${LIBS} -lmpr -lpcre

${CC} -c -o solaris-i686-debug/obj/http.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Isolaris-i686-debug/inc src/utils/http.c

${CC} -o solaris-i686-debug/bin/http ${LDFLAGS} -Lsolaris-i686-debug/lib solaris-i686-debug/obj/http.o ${LIBS} -lhttp -lmpr -lpcre -Lsolaris-i686-debug/lib -g

