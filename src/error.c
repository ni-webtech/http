/*
    error.c -- Http error handling
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

static void httpErrorV(HttpConn *conn, int flags, cchar *fmt, va_list args);
static void httpFormatErrorV(HttpConn *conn, int status, cchar *fmt, va_list args);

/*********************************** Code *************************************/

void httpDisconnect(HttpConn *conn)
{
    if (conn->sock) {
        mprDisconnectSocket(conn->sock);
    }
    conn->connError = 1;
    conn->keepAliveCount = -1;
    if (conn->rx) {
        conn->rx->eof = 1;
    }
}


void httpError(HttpConn *conn, int flags, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt);
    httpErrorV(conn, flags, fmt, args);
    va_end(args);
}


/*
    The current request has an error and cannot complete as normal. This call sets the Http response status and 
    overrides the normal output with an alternate error message. If the output has alread started (headers sent), then
    the connection MUST be closed so the client can get some indication the request failed.
 */
//  MOB - remove the http prefix. Make V lower case
static void httpErrorV(HttpConn *conn, int flags, cchar *fmt, va_list args)
{
    HttpTx      *tx;
    int         status;

    mprAssert(fmt);
    tx = conn->tx;

    if (conn == 0) {
        return;
    }
    if (flags & HTTP_ABORT) {
        conn->connError = 1;
    }
    if (conn->rx) {
        conn->rx->eof = 1;
    }
    conn->error = 1;
    status = flags & HTTP_CODE_MASK;
    if (status == 0) {
        status = HTTP_CODE_INTERNAL_SERVER_ERROR;
    }
    httpFormatErrorV(conn, status, fmt, args);

    if (flags & (HTTP_ABORT | HTTP_CLOSE)) {
        conn->keepAliveCount = -1;
    }
    if (conn->endpoint) {
        /*
            Server side must not call httpCloseConn() as it will remove wait handlers.
         */
        if (flags & HTTP_ABORT || (tx && tx->flags & HTTP_TX_HEADERS_CREATED)) {
            /* 
                If headers have been sent, must let the client know the request has failed - abort is the only way.
                Disconnect will cause a readable (EOF) event
             */
            httpDisconnect(conn);
        } else {
            httpFormatResponseError(conn, status, conn->errorMsg);
        }
    } else {
        if (flags & HTTP_ABORT || (tx && tx->flags & HTTP_TX_HEADERS_CREATED)) {
            httpDisconnect(conn);
        }
    }
    conn->responded = 1;
    httpFinalize(conn);
}


/*
    Just format conn->errorMsg and set status - nothing more
    NOTE: this is an internal API. Users should use httpError()
 */
//  MOB - rename to remove the http prefix
static void httpFormatErrorV(HttpConn *conn, int status, cchar *fmt, va_list args)
{
    if (conn->errorMsg == 0) {
        conn->errorMsg = sfmtv(fmt, args);
        //  MOB - need an escape option in status
        if (status) {
            if (status < 0) {
                status = HTTP_CODE_INTERNAL_SERVER_ERROR;
            }
            if (conn->endpoint && conn->tx) {
                conn->tx->status = status;
            } else if (conn->rx) {
                conn->rx->status = status;
            }
        }
        if (conn->rx == 0 || conn->rx->uri == 0) {
            mprLog(2, "\"%s\", status %d: %s.", httpLookupStatus(conn->http, status), status, conn->errorMsg);
        } else {
            mprLog(2, "Error: \"%s\", status %d for URI \"%s\": %s.",
                httpLookupStatus(conn->http, status), status, conn->rx->uri ? conn->rx->uri : "", conn->errorMsg);
        }
    }
}


/*
    Just format conn->errorMsg and set status - nothing more
    NOTE: this is an internal API. Users should use httpError()
 */
void httpFormatError(HttpConn *conn, int status, cchar *fmt, ...)
{
    va_list     args;

    va_start(args, fmt); 
    httpFormatErrorV(conn, status, fmt, args);
    va_end(args); 
}


cchar *httpGetError(HttpConn *conn)
{
    if (conn->errorMsg) {
        return conn->errorMsg;
    } else if (conn->state >= HTTP_STATE_FIRST) {
        return httpLookupStatus(conn->http, conn->rx->status);
    } else {
        return "";
    }
}


void httpMemoryError(HttpConn *conn)
{
    httpError(conn, HTTP_CODE_INTERNAL_SERVER_ERROR, "Memory allocation error");
}


/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.

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
