#
#   build.sh -- Build It Shell Script to build Http Library
#

CC="cl"
CFLAGS="-nologo -GR- -W3 -Zi -Od -MDd"
DFLAGS="-D_REENTRANT -D_MT"
IFLAGS="-Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc"
LDFLAGS="-nologo -nodefaultlib -incremental:no -libpath:/Users/mob/git/http/win-i686-debug/bin -debug -machine:x86"
LIBS="ws2_32.lib advapi32.lib user32.lib kernel32.lib oldnames.lib msvcrt.lib"

export PATH="%VS%/Bin:%VS%/VC/Bin:%VS%/Common7/IDE:%VS%/Common7/Tools:%VS%/SDK/v3.5/bin:%VS%/VC/VCPackages"
export INCLUDE="%VS%/INCLUDE:%VS%/VC/INCLUDE"
export LIB="%VS%/lib:%VS%/VC/lib"
"${CC}" -c -Fowin-i686-debug/obj/mprLib.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/deps/mpr/mprLib.c

"link" -dll -out:win-i686-debug/bin/libmpr.dll -entry:_DllMainCRTStartup@12 -def:win-i686-debug/bin/libmpr.def ${LDFLAGS} win-i686-debug/obj/mprLib.obj ${LIBS}

"${CC}" -c -Fowin-i686-debug/obj/manager.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/deps/mpr/manager.c

"link" -out:win-i686-debug/bin/manager.exe -entry:WinMainCRTStartup -subsystem:Windows ${LDFLAGS} win-i686-debug/obj/manager.obj ${LIBS} mpr.lib shell32.lib

"${CC}" -c -Fowin-i686-debug/obj/makerom.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/deps/mpr/makerom.c

"link" -out:win-i686-debug/bin/makerom.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} win-i686-debug/obj/makerom.obj ${LIBS} mpr.lib

"${CC}" -c -Fowin-i686-debug/obj/pcre.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/deps/pcre/pcre.c

"link" -dll -out:win-i686-debug/bin/libpcre.dll -entry:_DllMainCRTStartup@12 -def:win-i686-debug/bin/libpcre.def ${LDFLAGS} win-i686-debug/obj/pcre.obj ${LIBS}

"${CC}" -c -Fowin-i686-debug/obj/auth.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/auth.c

"${CC}" -c -Fowin-i686-debug/obj/authCheck.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/authCheck.c

"${CC}" -c -Fowin-i686-debug/obj/authFile.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/authFile.c

"${CC}" -c -Fowin-i686-debug/obj/authPam.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/authPam.c

"${CC}" -c -Fowin-i686-debug/obj/cache.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/cache.c

"${CC}" -c -Fowin-i686-debug/obj/chunkFilter.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/chunkFilter.c

"${CC}" -c -Fowin-i686-debug/obj/client.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/client.c

"${CC}" -c -Fowin-i686-debug/obj/conn.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/conn.c

"${CC}" -c -Fowin-i686-debug/obj/endpoint.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/endpoint.c

"${CC}" -c -Fowin-i686-debug/obj/error.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/error.c

"${CC}" -c -Fowin-i686-debug/obj/host.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/host.c

"${CC}" -c -Fowin-i686-debug/obj/httpService.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/httpService.c

"${CC}" -c -Fowin-i686-debug/obj/log.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/log.c

"${CC}" -c -Fowin-i686-debug/obj/netConnector.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/netConnector.c

"${CC}" -c -Fowin-i686-debug/obj/packet.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/packet.c

"${CC}" -c -Fowin-i686-debug/obj/passHandler.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/passHandler.c

"${CC}" -c -Fowin-i686-debug/obj/pipeline.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/pipeline.c

"${CC}" -c -Fowin-i686-debug/obj/queue.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/queue.c

"${CC}" -c -Fowin-i686-debug/obj/rangeFilter.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/rangeFilter.c

"${CC}" -c -Fowin-i686-debug/obj/route.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/route.c

"${CC}" -c -Fowin-i686-debug/obj/rx.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/rx.c

"${CC}" -c -Fowin-i686-debug/obj/sendConnector.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/sendConnector.c

"${CC}" -c -Fowin-i686-debug/obj/stage.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/stage.c

"${CC}" -c -Fowin-i686-debug/obj/trace.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/trace.c

"${CC}" -c -Fowin-i686-debug/obj/tx.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/tx.c

"${CC}" -c -Fowin-i686-debug/obj/uploadFilter.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/uploadFilter.c

"${CC}" -c -Fowin-i686-debug/obj/uri.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/uri.c

"${CC}" -c -Fowin-i686-debug/obj/var.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/var.c

"link" -dll -out:win-i686-debug/bin/libhttp.dll -entry:_DllMainCRTStartup@12 -def:win-i686-debug/bin/libhttp.def ${LDFLAGS} win-i686-debug/obj/auth.obj win-i686-debug/obj/authCheck.obj win-i686-debug/obj/authFile.obj win-i686-debug/obj/authPam.obj win-i686-debug/obj/cache.obj win-i686-debug/obj/chunkFilter.obj win-i686-debug/obj/${CC}ient.obj win-i686-debug/obj/conn.obj win-i686-debug/obj/endpoint.obj win-i686-debug/obj/error.obj win-i686-debug/obj/host.obj win-i686-debug/obj/httpService.obj win-i686-debug/obj/log.obj win-i686-debug/obj/netConnector.obj win-i686-debug/obj/packet.obj win-i686-debug/obj/passHandler.obj win-i686-debug/obj/pipeline.obj win-i686-debug/obj/queue.obj win-i686-debug/obj/rangeFilter.obj win-i686-debug/obj/route.obj win-i686-debug/obj/rx.obj win-i686-debug/obj/sendConnector.obj win-i686-debug/obj/stage.obj win-i686-debug/obj/trace.obj win-i686-debug/obj/tx.obj win-i686-debug/obj/uploadFilter.obj win-i686-debug/obj/uri.obj win-i686-debug/obj/var.obj ${LIBS} mpr.lib pcre.lib

"${CC}" -c -Fowin-i686-debug/obj/http.obj -Fdwin-i686-debug/obj ${CFLAGS} ${DFLAGS} -Isrc/deps/mpr -Isrc/deps/pcre -Isrc -Iwin-i686-debug/inc src/utils/http.c

"link" -out:win-i686-debug/bin/http.exe -entry:mainCRTStartup -subsystem:console ${LDFLAGS} win-i686-debug/obj/http.obj ${LIBS} http.lib mpr.lib pcre.lib

