/*
    log.c -- Http request access logging

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/************************************ Code ************************************/

void httpSetRouteLog(HttpRoute *route, cchar *path, ssize size, int backup, cchar *format, int flags)
{
    char    *src, *dest;

    mprAssert(route);
    mprAssert(path && *path);
    mprAssert(format);
    
    if (format == NULL || *format == '\0') {
        format = HTTP_LOG_FORMAT;
    }
    route->logFlags = flags;
    route->logSize = size;
    route->logBackup = backup;
    route->logPath = sclone(path);
    route->logFormat = sclone(format);

    for (src = dest = route->logFormat; *src; src++) {
        if (*src == '\\' && src[1] != '\\') {
            continue;
        }
        *dest++ = *src;
    }
    *dest = '\0';
}


void httpBackupRouteLog(HttpRoute *route)
{
    MprPath     info;

    mprAssert(route->logBackup);
    mprAssert(route->logSize > 100);

    lock(route);
    if (route->parent && route->parent->log == route->log) {
        httpBackupRouteLog(route->parent);
        route->log = route->parent->log;
        unlock(route);
        return;
    }
    mprGetPathInfo(route->logPath, &info);
    if (info.valid && ((route->logFlags & MPR_LOG_ANEW) || info.size > route->logSize || route->logSize <= 0)) {
        if (route->log) {
            mprCloseFile(route->log);
            route->log = 0;
        }
        mprBackupLog(route->logPath, route->logBackup);
        route->logFlags &= ~MPR_LOG_ANEW;
    }
    unlock(route);
}


MprFile *httpOpenRouteLog(HttpRoute *route)
{
    MprFile     *file;
    int         mode;

    mprAssert(route->log == 0);
    mode = O_CREAT | O_WRONLY | O_TEXT;
    if ((file = mprOpenFile(route->logPath, mode, 0664)) == 0) {
        mprError("Can't open log file %s", route->logPath);
        unlock(MPR);
        return 0;
    }
    route->log = file;
    return file;
}


void httpWriteRouteLog(HttpRoute *route, cchar *buf, ssize len)
{
    lock(MPR);
    if (route->logBackup > 0) {
        //  TODO OPT - don't check this on every write
        httpBackupRouteLog(route);
        if (!route->log && !httpOpenRouteLog(route)) {
            unlock(MPR);
            return;
        }
    }
    if (mprWriteFile(route->log, (char*) buf, len) != len) {
        mprError("Can't write to access log %s", route->logPath);
        mprCloseFile(route->log);
        route->log = 0;
    }
    unlock(MPR);
}


void httpLogRequest(HttpConn *conn)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpRoute   *route;
    MprBuf      *buf;
    char        keyBuf[80], *timeText, *fmt, *cp, *qualifier, *value, c;
    int         len;

    if ((rx = conn->rx) == 0) {
        return;
    }
    tx = conn->tx;
    if ((route = rx->route) == 0 || route->log == 0) {
        return;
    }
    fmt = route->logFormat;
    if (fmt == 0) {
        fmt = HTTP_LOG_FORMAT;
    }
    len = MPR_MAX_URL + 256;
    buf = mprCreateBuf(len, len);

    while ((c = *fmt++) != '\0') {
        if (c != '%' || (c = *fmt++) == '%') {
            mprPutCharToBuf(buf, c);
            continue;
        }
        switch (c) {
        case 'a':                           /* Remote IP */
            mprPutStringToBuf(buf, conn->ip);
            break;

        case 'A':                           /* Local IP */
            mprPutStringToBuf(buf, conn->sock->listenSock->ip);
            break;

        case 'b':
            if (tx->bytesWritten == 0) {
                mprPutCharToBuf(buf, '-');
            } else {
                mprPutIntToBuf(buf, tx->bytesWritten);
            } 
            break;

        case 'B':                           /* Bytes written (minus headers) */
            mprPutIntToBuf(buf, (tx->bytesWritten - tx->headerSize));
            break;

        case 'h':                           /* Remote host */
            //  TODO - Should this trigger a reverse DNS?
            mprPutStringToBuf(buf, conn->ip);
            break;

        case 'n':                           /* Local host */
            mprPutStringToBuf(buf, rx->parsedUri->host);
            break;

        case 'O':                           /* Bytes written (including headers) */
            mprPutIntToBuf(buf, tx->bytesWritten);
            break;

        case 'r':                           /* First line of request */
            mprPutFmtToBuf(buf, "%s %s %s", rx->method, rx->uri, conn->protocol);
            break;

        case 's':                           /* Response code */
            mprPutIntToBuf(buf, tx->status);
            break;

        case 't':                           /* Time */
            mprPutCharToBuf(buf, '[');
            timeText = mprFormatLocalTime(MPR_DEFAULT_DATE, mprGetTime());
            mprPutStringToBuf(buf, timeText);
            mprPutCharToBuf(buf, ']');
            break;

        case 'u':                           /* Remote username */
            mprPutStringToBuf(buf, conn->authUser ? conn->authUser : "-");
            break;

        case '{':                           /* Header line */
            qualifier = fmt;
            if ((cp = strchr(qualifier, '}')) != 0) {
                fmt = &cp[1];
                *cp = '\0';
                c = *fmt++;
                scopy(keyBuf, sizeof(keyBuf), "HTTP_");
                scopy(&keyBuf[5], sizeof(keyBuf) - 5, qualifier);
                switch (c) {
                case 'i':
                    value = (char*) mprLookupKey(rx->headers, supper(keyBuf));
                    mprPutStringToBuf(buf, value ? value : "-");
                    break;
                default:
                    mprPutStringToBuf(buf, qualifier);
                }
                *cp = '}';

            } else {
                mprPutCharToBuf(buf, c);
            }
            break;

        case '>':
            if (*fmt == 's') {
                fmt++;
                mprPutIntToBuf(buf, tx->status);
            }
            break;

        default:
            mprPutCharToBuf(buf, c);
            break;
        }
    }
    mprPutCharToBuf(buf, '\n');
    mprAddNullToBuf(buf);
    httpWriteRouteLog(route, mprGetBufStart(buf), mprGetBufLength(buf));
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
