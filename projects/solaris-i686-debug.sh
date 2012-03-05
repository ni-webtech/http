#
#   build.sh -- Build It Shell Script to build Http Library
#

PLATFORM="solaris-i686-debug"
CC="cc"
CFLAGS="-fPIC -g -mcpu=i686"
DFLAGS="-DPIC"
IFLAGS="-Isolaris-i686-debug/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc"
LDFLAGS="-L/Users/mob/git/http/${PLATFORM}/lib -g"
LIBS="-lpthread -lm"

${CC} -c -o ${PLATFORM}/obj/mprLib.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

${CC} -shared -o ${PLATFORM}/lib/libmpr.so -L${PLATFORM}/lib -g ${PLATFORM}/obj/mprLib.o ${LIBS}

${CC} -c -o ${PLATFORM}/obj/manager.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/deps/mpr/manager.c

${CC} -o ${PLATFORM}/bin/manager -L${PLATFORM}/lib -g -L${PLATFORM}/lib ${PLATFORM}/obj/manager.o ${LIBS} -lmpr -L${PLATFORM}/lib -g

${CC} -c -o ${PLATFORM}/obj/makerom.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

${CC} -o ${PLATFORM}/bin/makerom -L${PLATFORM}/lib -g -L${PLATFORM}/lib ${PLATFORM}/obj/makerom.o ${LIBS} -lmpr -L${PLATFORM}/lib -g

${CC} -c -o ${PLATFORM}/obj/pcre.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

${CC} -shared -o ${PLATFORM}/lib/libpcre.so -L${PLATFORM}/lib -g ${PLATFORM}/obj/pcre.o ${LIBS}

${CC} -c -o ${PLATFORM}/obj/auth.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/auth.c

${CC} -c -o ${PLATFORM}/obj/authCheck.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/authCheck.c

${CC} -c -o ${PLATFORM}/obj/authFile.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/authFile.c

${CC} -c -o ${PLATFORM}/obj/authPam.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/authPam.c

${CC} -c -o ${PLATFORM}/obj/cache.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/cache.c

${CC} -c -o ${PLATFORM}/obj/chunkFilter.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/chunkFilter.c

${CC} -c -o ${PLATFORM}/obj/client.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/client.c

${CC} -c -o ${PLATFORM}/obj/conn.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/conn.c

${CC} -c -o ${PLATFORM}/obj/endpoint.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/endpoint.c

${CC} -c -o ${PLATFORM}/obj/error.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/error.c

${CC} -c -o ${PLATFORM}/obj/host.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/host.c

${CC} -c -o ${PLATFORM}/obj/httpService.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/httpService.c

${CC} -c -o ${PLATFORM}/obj/log.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/log.c

${CC} -c -o ${PLATFORM}/obj/netConnector.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/netConnector.c

${CC} -c -o ${PLATFORM}/obj/packet.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/packet.c

${CC} -c -o ${PLATFORM}/obj/passHandler.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/passHandler.c

${CC} -c -o ${PLATFORM}/obj/pipeline.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/pipeline.c

${CC} -c -o ${PLATFORM}/obj/queue.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/queue.c

${CC} -c -o ${PLATFORM}/obj/rangeFilter.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/rangeFilter.c

${CC} -c -o ${PLATFORM}/obj/route.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/route.c

${CC} -c -o ${PLATFORM}/obj/rx.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/rx.c

${CC} -c -o ${PLATFORM}/obj/sendConnector.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/sendConnector.c

${CC} -c -o ${PLATFORM}/obj/stage.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/stage.c

${CC} -c -o ${PLATFORM}/obj/trace.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/trace.c

${CC} -c -o ${PLATFORM}/obj/tx.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/tx.c

${CC} -c -o ${PLATFORM}/obj/uploadFilter.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/uploadFilter.c

${CC} -c -o ${PLATFORM}/obj/uri.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/uri.c

${CC} -c -o ${PLATFORM}/obj/var.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/var.c

${CC} -shared -o ${PLATFORM}/lib/libhttp.so -L${PLATFORM}/lib -g ${PLATFORM}/obj/auth.o ${PLATFORM}/obj/authCheck.o ${PLATFORM}/obj/authFile.o ${PLATFORM}/obj/authPam.o ${PLATFORM}/obj/cache.o ${PLATFORM}/obj/chunkFilter.o ${PLATFORM}/obj/client.o ${PLATFORM}/obj/conn.o ${PLATFORM}/obj/endpoint.o ${PLATFORM}/obj/error.o ${PLATFORM}/obj/host.o ${PLATFORM}/obj/httpService.o ${PLATFORM}/obj/log.o ${PLATFORM}/obj/netConnector.o ${PLATFORM}/obj/packet.o ${PLATFORM}/obj/passHandler.o ${PLATFORM}/obj/pipeline.o ${PLATFORM}/obj/queue.o ${PLATFORM}/obj/rangeFilter.o ${PLATFORM}/obj/route.o ${PLATFORM}/obj/rx.o ${PLATFORM}/obj/sendConnector.o ${PLATFORM}/obj/stage.o ${PLATFORM}/obj/trace.o ${PLATFORM}/obj/tx.o ${PLATFORM}/obj/uploadFilter.o ${PLATFORM}/obj/uri.o ${PLATFORM}/obj/var.o ${LIBS} -lmpr -lpcre

${CC} -c -o ${PLATFORM}/obj/http.o ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/utils/http.c

${CC} -o ${PLATFORM}/bin/http -L${PLATFORM}/lib -g -L${PLATFORM}/lib ${PLATFORM}/obj/http.o ${LIBS} -lhttp -lmpr -lpcre -L${PLATFORM}/lib -g

