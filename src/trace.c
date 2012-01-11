/*
    trace.c -- Trace data
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/*********************************** Code *************************************/

void httpSetRouteTraceFilter(HttpRoute *route, int dir, int levels[HTTP_TRACE_MAX_ITEM], ssize len, 
    cchar *include, cchar *exclude)
{
    HttpTrace   *trace;
    char        *word, *tok, *line;
    int         i;

    trace = &route->trace[dir];
    trace->size = len;
    for (i = 0; i < HTTP_TRACE_MAX_ITEM; i++) {
        trace->levels[i] = levels[i];
    }
    if (include && strcmp(include, "*") != 0) {
        trace->include = mprCreateHash(0, 0);
        line = sclone(include);
        word = stok(line, ", \t\r\n", &tok);
        while (word) {
            if (word[0] == '*' && word[1] == '.') {
                word += 2;
            }
            mprAddKey(trace->include, word, route);
            word = stok(NULL, ", \t\r\n", &tok);
        }
    }
    if (exclude) {
        trace->exclude = mprCreateHash(0, 0);
        line = sclone(exclude);
        word = stok(line, ", \t\r\n", &tok);
        while (word) {
            if (word[0] == '*' && word[1] == '.') {
                word += 2;
            }
            mprAddKey(trace->exclude, word, route);
            word = stok(NULL, ", \t\r\n", &tok);
        }
    }
}


void httpManageTrace(HttpTrace *trace, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(trace->include);
        mprMark(trace->exclude);
    }
}


void httpInitTrace(HttpTrace *trace)
{
    int     dir;

    mprAssert(trace);

    for (dir = 0; dir < HTTP_TRACE_MAX_DIR; dir++) {
        trace[dir].levels[HTTP_TRACE_CONN] = 3;
        trace[dir].levels[HTTP_TRACE_FIRST] = 2;
        trace[dir].levels[HTTP_TRACE_HEADER] = 3;
        trace[dir].levels[HTTP_TRACE_BODY] = 4;
        trace[dir].levels[HTTP_TRACE_LIMITS] = 5;
        trace[dir].levels[HTTP_TRACE_TIME] = 6;
        trace[dir].size = -1;
    }
}


/*
    If tracing should occur, return the level
 */
int httpShouldTrace(HttpConn *conn, int dir, int item, cchar *ext)
{
    HttpTrace   *trace;

    mprAssert(0 <= dir && dir < HTTP_TRACE_MAX_DIR);
    mprAssert(0 <= item && item < HTTP_TRACE_MAX_ITEM);

    trace = &conn->trace[dir];
    if (trace->disable || trace->levels[item] > MPR->logLevel) {
        return -1;
    }
    if (ext) {
        if (trace->include && !mprLookupKey(trace->include, ext)) {
            trace->disable = 1;
            return -1;
        }
        if (trace->exclude && mprLookupKey(trace->exclude, ext)) {
            trace->disable = 1;
            return -1;
        }
    }
    return trace->levels[item];
}


//  OPT
static void traceBuf(HttpConn *conn, int dir, int level, cchar *msg, cchar *buf, ssize len)
{
    cchar       *start, *cp, *tag, *digits;
    char        *data, *dp;
    static int  txSeq = 0;
    static int  rxSeq = 0;
    int         seqno, i, printable;

    start = buf;
    if (len > 3 && start[0] == (char) 0xef && start[1] == (char) 0xbb && start[2] == (char) 0xbf) {
        start += 3;
    }
    for (printable = 1, i = 0; i < len; i++) {
        if (!isascii(start[i])) {
            printable = 0;
        }
    }
    if (dir == HTTP_TRACE_TX) {
        tag = "Transmit";
        seqno = txSeq++;
    } else {
        tag = "Receive";
        seqno = rxSeq++;
    }
    if (printable) {
        data = mprAlloc(len + 1);
        memcpy(data, start, len);
        data[len] = '\0';
        mprRawLog(level, "\n>>>>>>>>>> %s %s packet %d, len %d (conn %d) >>>>>>>>>>\n%s", tag, msg, seqno, 
            len, conn->seqno, data);
    } else {
        mprRawLog(level, "\n>>>>>>>>>> %s %s packet %d, len %d (conn %d) >>>>>>>>>> (binary)\n", tag, msg, seqno, 
            len, conn->seqno);
        data = mprAlloc(len * 3 + ((len / 16) + 1) + 1);
        digits = "0123456789ABCDEF";
        for (i = 0, cp = start, dp = data; cp < &start[len]; cp++) {
            *dp++ = digits[(*cp >> 4) & 0x0f];
            *dp++ = digits[*cp++ & 0x0f];
            *dp++ = ' ';
            if ((++i % 16) == 0) {
                *dp++ = '\n';
            }
        }
        *dp++ = '\n';
        *dp = '\0';
        mprRawLog(level, "%s", data);
    }
    mprRawLog(level, "<<<<<<<<<< End %s packet, conn %d\n\n", tag, conn->seqno);
}


void httpTraceContent(HttpConn *conn, int dir, int item, HttpPacket *packet, ssize len, MprOff total)
{
    HttpTrace   *trace;
    ssize       size;
    int         level;

    trace = &conn->trace[dir];
    level = trace->levels[item];

    if (trace->size >= 0 && total >= trace->size) {
        mprLog(level, "Abbreviating response trace for conn %d", conn->seqno);
        trace->disable = 1;
        return;
    }
    if (len <= 0) {
        len = MAXINT;
    }
    if (packet->prefix) {
        size = mprGetBufLength(packet->prefix);
        size = min(size, len);
        traceBuf(conn, dir, level, "prefix", mprGetBufStart(packet->prefix), size);
    }
    if (packet->content) {
        size = httpGetPacketLength(packet);
        size = min(size, len);
        traceBuf(conn, dir, level, "content", mprGetBufStart(packet->content), size);
    }
}


/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2012. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.TXT distributed with
    this software for full details.

    This software is open source; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version. See the GNU General Public License for more
    details at: http://embedthis.com/downloads/gplLicense.html

    This program is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    This GPL license does NOT permit incorporating this software into
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses
    for this software and support services are available from Embedthis
    Software at http://embedthis.com
 
    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
