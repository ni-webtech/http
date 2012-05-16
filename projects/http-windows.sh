#
#   http-windows.sh -- Build It Shell Script to build Http Library
#

export PATH="$(SDK)/Bin:$(VS)/VC/Bin:$(VS)/Common7/IDE:$(VS)/Common7/Tools:$(VS)/SDK/v3.5/bin:$(VS)/VC/VCPackages;$(PATH)"
export INCLUDE="$(INCLUDE);$(SDK)/INCLUDE:$(VS)/VC/INCLUDE"
export LIB="$(LIB);$(SDK)/lib:$(VS)/VC/lib"

ARCH="x86"
ARCH="$(shell uname -m | sed 's/i.86/x86/;s/x86_64/x64/')"
OS="windows"
PROFILE="debug"
CONFIG="${OS}-${ARCH}-${PROFILE}"
CC="cl.exe"
LD="link.exe"
CFLAGS="-nologo -GR- -W3 -Zi -Od -MDd"
DFLAGS="-D_REENTRANT -D_MT -DBIT_DEBUG"
IFLAGS="-I${CONFIG}/inc -Isrc"
LDFLAGS="-nologo -nodefaultlib -incremental:no -debug -machine:x86"
LIBPATHS="-libpath:${CONFIG}/bin"
LIBS="ws2_32.lib advapi32.lib user32.lib kernel32.lib oldnames.lib msvcrt.lib shell32.lib"

[ ! -x ${CONFIG}/inc ] && mkdir -p ${CONFIG}/inc ${CONFIG}/obj ${CONFIG}/lib ${CONFIG}/bin

[ ! -f ${CONFIG}/inc/bit.h ] && cp projects/http-${OS}-bit.h ${CONFIG}/inc/bit.h
if ! diff ${CONFIG}/inc/bit.h projects/http-${OS}-bit.h >/dev/null ; then
	cp projects/http-${OS}-bit.h ${CONFIG}/inc/bit.h
fi

rm -rf ${CONFIG}/inc/mpr.h
cp -r src/deps/mpr/mpr.h ${CONFIG}/inc/mpr.h

"${CC}" -c -Fo${CONFIG}/obj/mprLib.obj -Fd${CONFIG}/obj/mprLib.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/deps/mpr/mprLib.c

"${LD}" -dll -out:${CONFIG}/bin/libmpr.dll -entry:_DllMainCRTStartup@12 -def:${CONFIG}/bin/libmpr.def ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/mprLib.obj ${LIBS}

"${CC}" -c -Fo${CONFIG}/obj/mprSsl.obj -Fd${CONFIG}/obj/mprSsl.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/deps/mpr/mprSsl.c

"${LD}" -dll -out:${CONFIG}/bin/libmprssl.dll -entry:_DllMainCRTStartup@12 -def:${CONFIG}/bin/libmprssl.def ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/mprSsl.obj ${LIBS} libmpr.lib

"${CC}" -c -Fo${CONFIG}/obj/makerom.obj -Fd${CONFIG}/obj/makerom.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/deps/mpr/makerom.c

"${LD}" -out:${CONFIG}/bin/makerom.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/makerom.obj ${LIBS} libmpr.lib

rm -rf ${CONFIG}/inc/pcre.h
cp -r src/deps/pcre/pcre.h ${CONFIG}/inc/pcre.h

"${CC}" -c -Fo${CONFIG}/obj/pcre.obj -Fd${CONFIG}/obj/pcre.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/deps/pcre/pcre.c

"${LD}" -dll -out:${CONFIG}/bin/libpcre.dll -entry:_DllMainCRTStartup@12 -def:${CONFIG}/bin/libpcre.def ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/pcre.obj ${LIBS}

rm -rf ${CONFIG}/inc/http.h
cp -r src/http.h ${CONFIG}/inc/http.h

"${CC}" -c -Fo${CONFIG}/obj/auth.obj -Fd${CONFIG}/obj/auth.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/auth.c

"${CC}" -c -Fo${CONFIG}/obj/authCheck.obj -Fd${CONFIG}/obj/authCheck.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/authCheck.c

"${CC}" -c -Fo${CONFIG}/obj/authFile.obj -Fd${CONFIG}/obj/authFile.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/authFile.c

"${CC}" -c -Fo${CONFIG}/obj/authPam.obj -Fd${CONFIG}/obj/authPam.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/authPam.c

"${CC}" -c -Fo${CONFIG}/obj/cache.obj -Fd${CONFIG}/obj/cache.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/cache.c

"${CC}" -c -Fo${CONFIG}/obj/chunkFilter.obj -Fd${CONFIG}/obj/chunkFilter.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/chunkFilter.c

"${CC}" -c -Fo${CONFIG}/obj/client.obj -Fd${CONFIG}/obj/client.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/client.c

"${CC}" -c -Fo${CONFIG}/obj/conn.obj -Fd${CONFIG}/obj/conn.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/conn.c

"${CC}" -c -Fo${CONFIG}/obj/endpoint.obj -Fd${CONFIG}/obj/endpoint.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/endpoint.c

"${CC}" -c -Fo${CONFIG}/obj/error.obj -Fd${CONFIG}/obj/error.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/error.c

"${CC}" -c -Fo${CONFIG}/obj/host.obj -Fd${CONFIG}/obj/host.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/host.c

"${CC}" -c -Fo${CONFIG}/obj/httpService.obj -Fd${CONFIG}/obj/httpService.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/httpService.c

"${CC}" -c -Fo${CONFIG}/obj/log.obj -Fd${CONFIG}/obj/log.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/log.c

"${CC}" -c -Fo${CONFIG}/obj/netConnector.obj -Fd${CONFIG}/obj/netConnector.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/netConnector.c

"${CC}" -c -Fo${CONFIG}/obj/packet.obj -Fd${CONFIG}/obj/packet.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/packet.c

"${CC}" -c -Fo${CONFIG}/obj/passHandler.obj -Fd${CONFIG}/obj/passHandler.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/passHandler.c

"${CC}" -c -Fo${CONFIG}/obj/pipeline.obj -Fd${CONFIG}/obj/pipeline.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/pipeline.c

"${CC}" -c -Fo${CONFIG}/obj/queue.obj -Fd${CONFIG}/obj/queue.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/queue.c

"${CC}" -c -Fo${CONFIG}/obj/rangeFilter.obj -Fd${CONFIG}/obj/rangeFilter.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/rangeFilter.c

"${CC}" -c -Fo${CONFIG}/obj/route.obj -Fd${CONFIG}/obj/route.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/route.c

"${CC}" -c -Fo${CONFIG}/obj/rx.obj -Fd${CONFIG}/obj/rx.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/rx.c

"${CC}" -c -Fo${CONFIG}/obj/sendConnector.obj -Fd${CONFIG}/obj/sendConnector.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/sendConnector.c

"${CC}" -c -Fo${CONFIG}/obj/stage.obj -Fd${CONFIG}/obj/stage.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/stage.c

"${CC}" -c -Fo${CONFIG}/obj/trace.obj -Fd${CONFIG}/obj/trace.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/trace.c

"${CC}" -c -Fo${CONFIG}/obj/tx.obj -Fd${CONFIG}/obj/tx.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/tx.c

"${CC}" -c -Fo${CONFIG}/obj/uploadFilter.obj -Fd${CONFIG}/obj/uploadFilter.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/uploadFilter.c

"${CC}" -c -Fo${CONFIG}/obj/uri.obj -Fd${CONFIG}/obj/uri.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/uri.c

"${CC}" -c -Fo${CONFIG}/obj/var.obj -Fd${CONFIG}/obj/var.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/var.c

"${LD}" -dll -out:${CONFIG}/bin/libhttp.dll -entry:_DllMainCRTStartup@12 -def:${CONFIG}/bin/libhttp.def ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/auth.obj ${CONFIG}/obj/authCheck.obj ${CONFIG}/obj/authFile.obj ${CONFIG}/obj/authPam.obj ${CONFIG}/obj/cache.obj ${CONFIG}/obj/chunkFilter.obj ${CONFIG}/obj/client.obj ${CONFIG}/obj/conn.obj ${CONFIG}/obj/endpoint.obj ${CONFIG}/obj/error.obj ${CONFIG}/obj/host.obj ${CONFIG}/obj/httpService.obj ${CONFIG}/obj/log.obj ${CONFIG}/obj/netConnector.obj ${CONFIG}/obj/packet.obj ${CONFIG}/obj/passHandler.obj ${CONFIG}/obj/pipeline.obj ${CONFIG}/obj/queue.obj ${CONFIG}/obj/rangeFilter.obj ${CONFIG}/obj/route.obj ${CONFIG}/obj/rx.obj ${CONFIG}/obj/sendConnector.obj ${CONFIG}/obj/stage.obj ${CONFIG}/obj/trace.obj ${CONFIG}/obj/tx.obj ${CONFIG}/obj/uploadFilter.obj ${CONFIG}/obj/uri.obj ${CONFIG}/obj/var.obj ${LIBS} libmpr.lib libpcre.lib libmprssl.lib

"${CC}" -c -Fo${CONFIG}/obj/http.obj -Fd${CONFIG}/obj/http.pdb ${CFLAGS} ${DFLAGS} -I${CONFIG}/inc -Isrc src/utils/http.c

"${LD}" -out:${CONFIG}/bin/http.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} ${LIBPATHS} ${CONFIG}/obj/http.obj ${LIBS} libhttp.lib libmpr.lib libpcre.lib libmprssl.lib

