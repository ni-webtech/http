/*
    match.c -- HTTP pipeline matching.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"
#include    "pcre.h"

/*********************************** Code *************************************/

void httpMatchHost(HttpConn *conn)
{ 
    MprSocket       *listenSock;
    HttpEndpoint    *endpoint;
    HttpHost        *host;
    Http            *http;

    http = conn->http;
    listenSock = conn->sock->listenSock;

    if ((endpoint = httpLookupEndpoint(http, listenSock->ip, listenSock->port)) == 0) {
        mprError("No listening endpoint for request from %s:%d", listenSock->ip, listenSock->port);
        mprCloseSocket(conn->sock, 0);
        return;
    }
    if (httpHasNamedVirtualHosts(endpoint)) {
        host = httpLookupHostOnEndpoint(endpoint, conn->rx->hostHeader);
    } else {
        host = mprGetFirstItem(endpoint->hosts);
    }
    if (host == 0) {
        httpSetConnHost(conn, 0);
        httpError(conn, HTTP_CODE_NOT_FOUND, "No host to serve request. Searching for %s", conn->rx->hostHeader);
        conn->host = mprGetFirstItem(endpoint->hosts);
        return;
    }
    mprLog(4, "Select host: \"%s\"", host->name);
    conn->host = host;
}


/*
    Find the matching route and handler for a request. If any errors occur, the pass handler is used to pass errors onto the 
    net/sendfile connectors to send to the client. This routine may rewrite the request URI and may redirect the request.
 */
void httpRouteRequest(HttpConn *conn)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpRoute   *route;
    int         next, rewrites, match;

    rx = conn->rx;
    tx = conn->tx;

    for (next = rewrites = 0; rewrites < HTTP_MAX_REWRITE; ) {
        if ((route = mprGetNextItem(conn->host->routes, &next)) == 0) {
            break;
        }
        if ((match = httpMatchRoute(conn, route)) == HTTP_ROUTE_OK) {
            mprAssert(tx->handler);
            break;
        } else if (match == HTTP_ROUTE_REROUTE) {
            next = 0;
            route = 0;
            rewrites++;
        }
    }
    if (route == 0 || tx->handler == 0) {
        httpError(conn, HTTP_CODE_INTERNAL_SERVER_ERROR, "Can't find suitable route for request");
        return;
    }
    rx->route = route;
    if (conn->error || tx->redirected || tx->altBody) {
        tx->handler = conn->http->passHandler;
        if (rewrites >= HTTP_MAX_REWRITE) {
            httpError(conn, HTTP_CODE_INTERNAL_SERVER_ERROR, "Too many request rewrites");
        }
    }
    mprLog(4, "Select handler: \"%s\" for \"%s\"", tx->handler->name, rx->uri);
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
