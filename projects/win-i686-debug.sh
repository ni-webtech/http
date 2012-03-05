#
#   build.sh -- Build It Shell Script to build Http Library
#

PLATFORM="win-i686-debug"
CC="cl"
CFLAGS="-nologo -GR- -W3 -Zi -Od -MDd"
DFLAGS="-D_REENTRANT -D_MT"
IFLAGS="-Iwin-i686-debug/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc"
LDFLAGS="-nologo -nodefaultlib -incremental:no -libpath:/Users/mob/git/http/${PLATFORM}/bin -debug -machine:x86"
LIBS="ws2_32.lib advapi32.lib user32.lib kernel32.lib oldnames.lib msvcrt.lib"

export PATH="%VS%/Bin:%VS%/VC/Bin:%VS%/Common7/IDE:%VS%/Common7/Tools:%VS%/SDK/v3.5/bin:%VS%/VC/VCPackages"
export INCLUDE="%VS%/INCLUDE:%VS%/VC/INCLUDE"
export LIB="%VS%/lib:%VS%/VC/lib"
"${CC}" -c -Fo${PLATFORM}/obj/mprLib.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/deps/mpr/mprLib.c

"link" -dll -out:${PLATFORM}/bin/libmpr.dll -entry:_DllMainCRTStartup@12 -def:${PLATFORM}/bin/libmpr.def -nologo -nodefaultlib -incremental:no -libpath:${PLATFORM}/bin -debug -machine:x86 ${PLATFORM}/obj/mprLib.obj ${LIBS}

"${CC}" -c -Fo${PLATFORM}/obj/manager.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/deps/mpr/manager.c

"link" -out:${PLATFORM}/bin/manager.exe -entry:WinMainCRTStartup -subsystem:Windows -nologo -nodefaultlib -incremental:no -libpath:${PLATFORM}/bin -debug -machine:x86 ${PLATFORM}/obj/manager.obj ${LIBS} mpr.lib shell32.lib

"${CC}" -c -Fo${PLATFORM}/obj/makerom.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/deps/mpr/makerom.c

"link" -out:${PLATFORM}/bin/makerom.exe -entry:mainCRTStartup -subsystem:console -nologo -nodefaultlib -incremental:no -libpath:${PLATFORM}/bin -debug -machine:x86 ${PLATFORM}/obj/makerom.obj ${LIBS} mpr.lib

"${CC}" -c -Fo${PLATFORM}/obj/pcre.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/deps/pcre/pcre.c

"link" -dll -out:${PLATFORM}/bin/libpcre.dll -entry:_DllMainCRTStartup@12 -def:${PLATFORM}/bin/libpcre.def -nologo -nodefaultlib -incremental:no -libpath:${PLATFORM}/bin -debug -machine:x86 ${PLATFORM}/obj/pcre.obj ${LIBS}

"${CC}" -c -Fo${PLATFORM}/obj/auth.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/auth.c

"${CC}" -c -Fo${PLATFORM}/obj/authCheck.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/authCheck.c

"${CC}" -c -Fo${PLATFORM}/obj/authFile.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/authFile.c

"${CC}" -c -Fo${PLATFORM}/obj/authPam.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/authPam.c

"${CC}" -c -Fo${PLATFORM}/obj/cache.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/cache.c

"${CC}" -c -Fo${PLATFORM}/obj/chunkFilter.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/chunkFilter.c

"${CC}" -c -Fo${PLATFORM}/obj/client.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/client.c

"${CC}" -c -Fo${PLATFORM}/obj/conn.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/conn.c

"${CC}" -c -Fo${PLATFORM}/obj/endpoint.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/endpoint.c

"${CC}" -c -Fo${PLATFORM}/obj/error.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/error.c

"${CC}" -c -Fo${PLATFORM}/obj/host.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/host.c

"${CC}" -c -Fo${PLATFORM}/obj/httpService.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/httpService.c

"${CC}" -c -Fo${PLATFORM}/obj/log.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/log.c

"${CC}" -c -Fo${PLATFORM}/obj/netConnector.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/netConnector.c

"${CC}" -c -Fo${PLATFORM}/obj/packet.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/packet.c

"${CC}" -c -Fo${PLATFORM}/obj/passHandler.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/passHandler.c

"${CC}" -c -Fo${PLATFORM}/obj/pipeline.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/pipeline.c

"${CC}" -c -Fo${PLATFORM}/obj/queue.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/queue.c

"${CC}" -c -Fo${PLATFORM}/obj/rangeFilter.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/rangeFilter.c

"${CC}" -c -Fo${PLATFORM}/obj/route.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/route.c

"${CC}" -c -Fo${PLATFORM}/obj/rx.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/rx.c

"${CC}" -c -Fo${PLATFORM}/obj/sendConnector.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/sendConnector.c

"${CC}" -c -Fo${PLATFORM}/obj/stage.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/stage.c

"${CC}" -c -Fo${PLATFORM}/obj/trace.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/trace.c

"${CC}" -c -Fo${PLATFORM}/obj/tx.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/tx.c

"${CC}" -c -Fo${PLATFORM}/obj/uploadFilter.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/uploadFilter.c

"${CC}" -c -Fo${PLATFORM}/obj/uri.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/uri.c

"${CC}" -c -Fo${PLATFORM}/obj/var.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/var.c

"link" -dll -out:${PLATFORM}/bin/libhttp.dll -entry:_DllMainCRTStartup@12 -def:${PLATFORM}/bin/libhttp.def -nologo -nodefaultlib -incremental:no -libpath:${PLATFORM}/bin -debug -machine:x86 ${PLATFORM}/obj/auth.obj ${PLATFORM}/obj/authCheck.obj ${PLATFORM}/obj/authFile.obj ${PLATFORM}/obj/authPam.obj ${PLATFORM}/obj/cache.obj ${PLATFORM}/obj/chunkFilter.obj ${PLATFORM}/obj/${CC}ient.obj ${PLATFORM}/obj/conn.obj ${PLATFORM}/obj/endpoint.obj ${PLATFORM}/obj/error.obj ${PLATFORM}/obj/host.obj ${PLATFORM}/obj/httpService.obj ${PLATFORM}/obj/log.obj ${PLATFORM}/obj/netConnector.obj ${PLATFORM}/obj/packet.obj ${PLATFORM}/obj/passHandler.obj ${PLATFORM}/obj/pipeline.obj ${PLATFORM}/obj/queue.obj ${PLATFORM}/obj/rangeFilter.obj ${PLATFORM}/obj/route.obj ${PLATFORM}/obj/rx.obj ${PLATFORM}/obj/sendConnector.obj ${PLATFORM}/obj/stage.obj ${PLATFORM}/obj/trace.obj ${PLATFORM}/obj/tx.obj ${PLATFORM}/obj/uploadFilter.obj ${PLATFORM}/obj/uri.obj ${PLATFORM}/obj/var.obj ${LIBS} mpr.lib pcre.lib

"${CC}" -c -Fo${PLATFORM}/obj/http.obj -Fd${PLATFORM}/obj ${CFLAGS} ${DFLAGS} -I${PLATFORM}/inc -Isrc/deps/mpr -Isrc/deps/pcre -Isrc src/utils/http.c

"link" -out:${PLATFORM}/bin/http.exe -entry:mainCRTStartup -subsystem:console -nologo -nodefaultlib -incremental:no -libpath:${PLATFORM}/bin -debug -machine:x86 ${PLATFORM}/obj/http.obj ${LIBS} http.lib mpr.lib pcre.lib

