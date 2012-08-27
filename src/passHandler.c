/*
    passHandler.c -- Pass through handler

    This handler simply relays all content to a network connector. It is used to when there is no handler defined 
    and to convey errors when the actual handler fails. It is configured as the "passHandler" and "errorHandler".
    The pipeline will also select this handler if any routing errors occur. 

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/*********************************** Code *************************************/
/*
    Handle Trace and Options requests. Handlers can do this themselves if they desire, but typically
    all Trace/Options requests come here.
 */
void httpHandleOptionsTrace(HttpConn *conn)
{
    HttpRx      *rx;
    HttpTx      *tx;
    int         flags;

    tx = conn->tx;
    rx = conn->rx;

    if (rx->flags & HTTP_TRACE) {
        /* The trace method is disabled by default unless 'TraceMethod on' is specified */
        if (!conn->limits->enableTraceMethod) {
            tx->status = HTTP_CODE_NOT_ACCEPTABLE;
            httpFormatResponseBody(conn, "Trace Request Denied", "The TRACE method is disabled on this server.");
        } else {
            httpFormatResponse(conn, "%s %s %s\r\n", rx->method, rx->uri, conn->protocol);
        }
        /* This finalizes output and indicates the request is now complete */
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
        httpSetContentLength(conn, 0);
        httpFinalize(conn);
    }
}


static void startPass(HttpQueue *q)
{
    mprLog(5, "Start passHandler");
    if (q->conn->rx->flags & (HTTP_OPTIONS | HTTP_TRACE)) {
        httpHandleOptionsTrace(q->conn);
    }
}


static void readyPass(HttpQueue *q)
{
    httpFinalize(q->conn);
}


static void readyError(HttpQueue *q)
{
    if (!q->conn->error) {
        httpError(q->conn, HTTP_CODE_SERVICE_UNAVAILABLE, "The requested resource is not available");
    }
    httpFinalize(q->conn);
}


int httpOpenPassHandler(Http *http)
{
    HttpStage     *stage;

    if ((stage = httpCreateHandler(http, "passHandler", HTTP_STAGE_ALL, NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    http->passHandler = stage;
    stage->start = startPass;
    stage->ready = readyPass;

    /*
        PassHandler is an alias as the ErrorHandler too
     */
    if ((stage = httpCreateHandler(http, "errorHandler", HTTP_STAGE_ALL, NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    stage->start = startPass;
    stage->ready = readyError;
    return 0;
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
