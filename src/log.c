/*
    log.c -- Http request access logging

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/************************************ Code ************************************/

int httpSetRouteLog(HttpRoute *route, cchar *path, ssize size, int backup, cchar *format, int flags)
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
    if (route->logBackup > 0) {
        httpBackupRouteLog(route);
    }
    route->logFlags &= ~MPR_LOG_ANEW;
    if (!httpOpenRouteLog(route)) {
        return MPR_ERR_CANT_OPEN;
    }
    return 0;
}


void httpBackupRouteLog(HttpRoute *route)
{
    MprPath     info;

    mprAssert(route->logBackup);
    mprAssert(route->logSize > 100);

    if (route->parent && route->parent->log == route->log) {
        httpBackupRouteLog(route->parent);
        return;
    }
    lock(route);
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
        return 0;
    }
    route->log = file;
    return file;
}


void httpWriteRouteLog(HttpRoute *route, cchar *buf, ssize len)
{
    lock(MPR);
    if (route->logBackup > 0) {
        //  OPT - don't check this on every write
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
            mprPutStringToBuf(buf, conn->username ? conn->username : "-");
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

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
