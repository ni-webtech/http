/*
    passHandler.c -- Pass through handler

    This handler simply relays all content onto a connector. It is used to when there is no handler defined 
    and to convey errors when the actual handler fails.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/*********************************** Code *************************************/

void httpHandleOptionsTrace(HttpQueue *q)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpConn    *conn;
    int         flags;

    conn = q->conn;
    tx = conn->tx;
    rx = conn->rx;

    if (rx->flags & HTTP_TRACE) {
        if (!conn->limits->enableTraceMethod) {
            tx->status = HTTP_CODE_NOT_ACCEPTABLE;
            httpFormatBody(conn, "Trace Request Denied", "The TRACE method is disabled on this server.");
        } else {
            httpFormatResponse(conn, "%s %s %s\r\n", rx->method, rx->uri, conn->protocol);
        }
        httpFinalize(conn);

    } else if (rx->flags & HTTP_OPTIONS) {
        flags = tx->traceMethods;
        httpSetHeader(conn, "Allow", "OPTIONS%s%s%s%s%s%s",
            (conn->limits->enableTraceMethod) ? ",TRACE" : "",
            (flags & HTTP_STAGE_GET) ? ",GET" : "",
            (flags & HTTP_STAGE_HEAD) ? ",HEAD" : "",
            (flags & HTTP_STAGE_POST) ? ",POST" : "",
            (flags & HTTP_STAGE_PUT) ? ",PUT" : "",
            (flags & HTTP_STAGE_DELETE) ? ",DELETE" : "");
        httpOmitBody(conn);
        httpFinalize(conn);
        tx->length = 0;
    }
}


static void openPass(HttpQueue *q)
{
    mprLog(5, "Open passHandler");
    if (q->conn->rx->flags & (HTTP_OPTIONS | HTTP_TRACE)) {
        httpHandleOptionsTrace(q);
    }
}


static void processPass(HttpQueue *q)
{
    httpFinalize(q->conn);
}


int httpOpenPassHandler(Http *http)
{
    HttpStage     *stage;

    if ((stage = httpCreateHandler(http, "passHandler", HTTP_STAGE_ALL, NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    http->passHandler = stage;
    stage->open = openPass;
    stage->process = processPass;
    return 0;
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
    details at: http://www.embedthis.com/downloads/gplLicense.html

    This program is distributed WITHOUT ANY WARRANTY; without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    This GPL license does NOT permit incorporating this software into
    proprietary programs. If you are unable to comply with the GPL, you must
    acquire a commercial license to use this software. Commercial licenses
    for this software and support services are available from Embedthis
    Software at http://www.embedthis.com

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
