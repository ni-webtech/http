#
#   win-i686-debug.sh -- Build It Shell Script to build Http Library
#

VS="${VSINSTALLDIR}"
: ${VS:="$(VS)"}
SDK="${WindowsSDKDir}"
: ${SDK:="$(SDK)"}

export SDK VS
export PATH="$(SDK)/Bin:$(VS)/VC/Bin:$(VS)/Common7/IDE:$(VS)/Common7/Tools:$(VS)/SDK/v3.5/bin:$(VS)/VC/VCPackages;$(PATH)"
export INCLUDE="$(INCLUDE);$(SDK)/INCLUDE:$(VS)/VC/INCLUDE"
export LIB="$(LIB);$(SDK)/lib:$(VS)/VC/lib"

CONFIG="win-i686-debug"
CC="cl.exe"
LD="link.exe"
CFLAGS="-nologo -GR- -W3 -Zi -Od -MDd -Zi -Od -MDd"
DFLAGS="-D_REENTRANT -D_MT"
IFLAGS="-Iwin-i686-debug/inc -Iwin-i686-debug/inc -Isrc/deps/pcre -Isrc"
LDFLAGS="-nologo -nodefaultlib -incremental:no -machine:x86 -machine:x86"
LIBPATHS="-libpath:${CONFIG}/bin -libpath:${CONFIG}/bin"
LIBS="ws2_32.lib advapi32.lib user32.lib kernel32.lib oldnames.lib msvcrt.lib shell32.lib"

[ ! -x ${CONFIG}/inc ] && mkdir -p ${CONFIG}/inc ${CONFIG}/obj ${CONFIG}/lib ${CONFIG}/bin
cp projects/buildConfig.${CONFIG} ${CONFIG}/inc/buildConfig.h

rm -rf win-i686-debug/inc/mpr.h
cp -r src/deps/mpr/mpr.h win-i686-debug/inc/mpr.h

rm -rf win-i686-debug/inc/mprSsl.h
cp -r src/deps/mpr/mprSsl.h win-i686-debug/inc/mprSsl.h

"${CC}" -c -Fo${CONFIG}/obj/mprLib.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

"${LD}" -dll -out:${CONFIG}/bin/libmpr.dll -entry:_DllMainCRTStartup@12 -def:${CONFIG}/bin/libmpr.def ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/mprLib.obj ${LIBS}

"${CC}" -c -Fo${CONFIG}/obj/makerom.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

"${LD}" -out:${CONFIG}/bin/makerom.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/makerom.obj ${LIBS} libmpr.lib

"${CC}" -c -Fo${CONFIG}/obj/pcre.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

"${LD}" -dll -out:${CONFIG}/bin/libpcre.dll -entry:_DllMainCRTStartup@12 -def:${CONFIG}/bin/libpcre.def ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/pcre.obj ${LIBS}

rm -rf win-i686-debug/inc/http.h
cp -r src/http.h win-i686-debug/inc/http.h

"${CC}" -c -Fo${CONFIG}/obj/auth.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/auth.c

"${CC}" -c -Fo${CONFIG}/obj/authCheck.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authCheck.c

"${CC}" -c -Fo${CONFIG}/obj/authFile.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authFile.c

"${CC}" -c -Fo${CONFIG}/obj/authPam.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/authPam.c

"${CC}" -c -Fo${CONFIG}/obj/cache.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/cache.c

"${CC}" -c -Fo${CONFIG}/obj/chunkFilter.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/chunkFilter.c

"${CC}" -c -Fo${CONFIG}/obj/client.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/client.c

"${CC}" -c -Fo${CONFIG}/obj/conn.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/conn.c

"${CC}" -c -Fo${CONFIG}/obj/endpoint.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/endpoint.c

"${CC}" -c -Fo${CONFIG}/obj/error.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/error.c

"${CC}" -c -Fo${CONFIG}/obj/host.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/host.c

"${CC}" -c -Fo${CONFIG}/obj/httpService.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/httpService.c

"${CC}" -c -Fo${CONFIG}/obj/log.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/log.c

"${CC}" -c -Fo${CONFIG}/obj/netConnector.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/netConnector.c

"${CC}" -c -Fo${CONFIG}/obj/packet.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/packet.c

"${CC}" -c -Fo${CONFIG}/obj/passHandler.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/passHandler.c

"${CC}" -c -Fo${CONFIG}/obj/pipeline.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/pipeline.c

"${CC}" -c -Fo${CONFIG}/obj/queue.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/queue.c

"${CC}" -c -Fo${CONFIG}/obj/rangeFilter.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/rangeFilter.c

"${CC}" -c -Fo${CONFIG}/obj/route.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/route.c

"${CC}" -c -Fo${CONFIG}/obj/rx.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/rx.c

"${CC}" -c -Fo${CONFIG}/obj/sendConnector.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/sendConnector.c

"${CC}" -c -Fo${CONFIG}/obj/stage.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/stage.c

"${CC}" -c -Fo${CONFIG}/obj/trace.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/trace.c

"${CC}" -c -Fo${CONFIG}/obj/tx.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/tx.c

"${CC}" -c -Fo${CONFIG}/obj/uploadFilter.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/uploadFilter.c

"${CC}" -c -Fo${CONFIG}/obj/uri.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/uri.c

"${CC}" -c -Fo${CONFIG}/obj/var.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/var.c

"${LD}" -dll -out:${CONFIG}/bin/libhttp.dll -entry:_DllMainCRTStartup@12 -def:${CONFIG}/bin/libhttp.def ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/auth.obj ${CONFIG}/obj/authCheck.obj ${CONFIG}/obj/authFile.obj ${CONFIG}/obj/authPam.obj ${CONFIG}/obj/cache.obj ${CONFIG}/obj/chunkFilter.obj ${CONFIG}/obj/client.obj ${CONFIG}/obj/conn.obj ${CONFIG}/obj/endpoint.obj ${CONFIG}/obj/error.obj ${CONFIG}/obj/host.obj ${CONFIG}/obj/httpService.obj ${CONFIG}/obj/log.obj ${CONFIG}/obj/netConnector.obj ${CONFIG}/obj/packet.obj ${CONFIG}/obj/passHandler.obj ${CONFIG}/obj/pipeline.obj ${CONFIG}/obj/queue.obj ${CONFIG}/obj/rangeFilter.obj ${CONFIG}/obj/route.obj ${CONFIG}/obj/rx.obj ${CONFIG}/obj/sendConnector.obj ${CONFIG}/obj/stage.obj ${CONFIG}/obj/trace.obj ${CONFIG}/obj/tx.obj ${CONFIG}/obj/uploadFilter.obj ${CONFIG}/obj/uri.obj ${CONFIG}/obj/var.obj ${LIBS} libmpr.lib libpcre.lib

"${CC}" -c -Fo${CONFIG}/obj/http.obj -Fd${CONFIG}/obj ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -I${CONFIG}/inc -Isrc/deps/pcre -Isrc src/utils/http.c

"${LD}" -out:${CONFIG}/bin/http.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/http.obj ${LIBS} libhttp.lib libmpr.lib libpcre.lib

