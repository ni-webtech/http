#
#   build.sh -- Build It Shell Script to build Http Library
#

CC="cc"
CFLAGS="-fPIC -g -mcpu=i686"
DFLAGS="-DPIC"
IFLAGS="-Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc"
LDFLAGS="-L/Users/mob/git/http/linux-i686-debug/lib -g"
LIBS="-lpthread -lm"

${CC} -c -o linux-i686-debug/obj/mprLib.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/deps/mpr/mprLib.c

${CC} -shared -o linux-i686-debug/lib/libmpr.so ${LDFLAGS} linux-i686-debug/obj/mprLib.o ${LIBS}

${CC} -c -o linux-i686-debug/obj/manager.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/deps/mpr/manager.c

${CC} -o linux-i686-debug/bin/manager ${LDFLAGS} -Llinux-i686-debug/lib linux-i686-debug/obj/manager.o ${LIBS} -lmpr -Llinux-i686-debug/lib -g

${CC} -c -o linux-i686-debug/obj/makerom.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/deps/mpr/makerom.c

${CC} -o linux-i686-debug/bin/makerom ${LDFLAGS} -Llinux-i686-debug/lib linux-i686-debug/obj/makerom.o ${LIBS} -lmpr -Llinux-i686-debug/lib -g

${CC} -c -o linux-i686-debug/obj/pcre.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/deps/pcre/pcre.c

${CC} -shared -o linux-i686-debug/lib/libpcre.so ${LDFLAGS} linux-i686-debug/obj/pcre.o ${LIBS}

${CC} -c -o linux-i686-debug/obj/auth.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/auth.c

${CC} -c -o linux-i686-debug/obj/authCheck.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/authCheck.c

${CC} -c -o linux-i686-debug/obj/authFile.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/authFile.c

${CC} -c -o linux-i686-debug/obj/authPam.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/authPam.c

${CC} -c -o linux-i686-debug/obj/cache.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/cache.c

${CC} -c -o linux-i686-debug/obj/chunkFilter.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/chunkFilter.c

${CC} -c -o linux-i686-debug/obj/client.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/client.c

${CC} -c -o linux-i686-debug/obj/conn.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/conn.c

${CC} -c -o linux-i686-debug/obj/endpoint.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/endpoint.c

${CC} -c -o linux-i686-debug/obj/error.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/error.c

${CC} -c -o linux-i686-debug/obj/host.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/host.c

${CC} -c -o linux-i686-debug/obj/httpService.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/httpService.c

${CC} -c -o linux-i686-debug/obj/log.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/log.c

${CC} -c -o linux-i686-debug/obj/netConnector.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/netConnector.c

${CC} -c -o linux-i686-debug/obj/packet.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/packet.c

${CC} -c -o linux-i686-debug/obj/passHandler.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/passHandler.c

${CC} -c -o linux-i686-debug/obj/pipeline.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/pipeline.c

${CC} -c -o linux-i686-debug/obj/queue.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/queue.c

${CC} -c -o linux-i686-debug/obj/rangeFilter.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/rangeFilter.c

${CC} -c -o linux-i686-debug/obj/route.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/route.c

${CC} -c -o linux-i686-debug/obj/rx.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/rx.c

${CC} -c -o linux-i686-debug/obj/sendConnector.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/sendConnector.c

${CC} -c -o linux-i686-debug/obj/stage.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/stage.c

${CC} -c -o linux-i686-debug/obj/trace.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/trace.c

${CC} -c -o linux-i686-debug/obj/tx.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/tx.c

${CC} -c -o linux-i686-debug/obj/uploadFilter.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/uploadFilter.c

${CC} -c -o linux-i686-debug/obj/uri.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/uri.c

${CC} -c -o linux-i686-debug/obj/var.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/var.c

${CC} -shared -o linux-i686-debug/lib/libhttp.so ${LDFLAGS} linux-i686-debug/obj/auth.o linux-i686-debug/obj/authCheck.o linux-i686-debug/obj/authFile.o linux-i686-debug/obj/authPam.o linux-i686-debug/obj/cache.o linux-i686-debug/obj/chunkFilter.o linux-i686-debug/obj/client.o linux-i686-debug/obj/conn.o linux-i686-debug/obj/endpoint.o linux-i686-debug/obj/error.o linux-i686-debug/obj/host.o linux-i686-debug/obj/httpService.o linux-i686-debug/obj/log.o linux-i686-debug/obj/netConnector.o linux-i686-debug/obj/packet.o linux-i686-debug/obj/passHandler.o linux-i686-debug/obj/pipeline.o linux-i686-debug/obj/queue.o linux-i686-debug/obj/rangeFilter.o linux-i686-debug/obj/route.o linux-i686-debug/obj/rx.o linux-i686-debug/obj/sendConnector.o linux-i686-debug/obj/stage.o linux-i686-debug/obj/trace.o linux-i686-debug/obj/tx.o linux-i686-debug/obj/uploadFilter.o linux-i686-debug/obj/uri.o linux-i686-debug/obj/var.o ${LIBS} -lmpr -lpcre

${CC} -c -o linux-i686-debug/obj/http.o ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Ilinux-i686-debug/inc src/utils/http.c

${CC} -o linux-i686-debug/bin/http ${LDFLAGS} -Llinux-i686-debug/lib linux-i686-debug/obj/http.o ${LIBS} -lhttp -lmpr -lpcre -Llinux-i686-debug/lib -g

