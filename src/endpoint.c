/*
    endpoint.c -- Create and manage listening endpoints.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

static int manageEndpoint(HttpEndpoint *endpoint, int flags);
static int destroyEndpointConnections(HttpEndpoint *endpoint);

/************************************ Code ************************************/
/*
    Create a listening endpoint on ip:port. NOTE: ip may be empty which means bind to all addresses.
 */
HttpEndpoint *httpCreateEndpoint(cchar *ip, int port, MprDispatcher *dispatcher)
{
    HttpEndpoint    *endpoint;
    Http            *http;

    if ((endpoint = mprAllocObj(HttpEndpoint, manageEndpoint)) == 0) {
        return 0;
    }
    http = MPR->httpService;
    endpoint->http = http;
    endpoint->clientLoad = mprCreateHash(HTTP_CLIENTS_HASH, MPR_HASH_STATIC_VALUES);
    endpoint->async = 1;
    endpoint->http = MPR->httpService;
    endpoint->port = port;
    endpoint->ip = sclone(ip);
    endpoint->dispatcher = dispatcher;
    endpoint->hosts = mprCreateList(-1, 0);
    httpAddEndpoint(http, endpoint);
    return endpoint;
}


void httpDestroyEndpoint(HttpEndpoint *endpoint)
{
    if (endpoint->waitHandler) {
        mprRemoveWaitHandler(endpoint->waitHandler);
        endpoint->waitHandler = 0;
    }
    destroyEndpointConnections(endpoint);
    if (endpoint->sock) {
        mprCloseSocket(endpoint->sock, 0);
        endpoint->sock = 0;
    }
    httpRemoveEndpoint(MPR->httpService, endpoint);
}


static int manageEndpoint(HttpEndpoint *endpoint, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(endpoint->http);
        mprMark(endpoint->hosts);
        mprMark(endpoint->limits);
        mprMark(endpoint->waitHandler);
        mprMark(endpoint->clientLoad);
        mprMark(endpoint->ip);
        mprMark(endpoint->context);
        mprMark(endpoint->sock);
        mprMark(endpoint->dispatcher);
        mprMark(endpoint->ssl);

    } else if (flags & MPR_MANAGE_FREE) {
        httpDestroyEndpoint(endpoint);
    }
    return 0;
}


/*  
    Convenience function to create and configure a new endpoint without using a config file.
 */
HttpEndpoint *httpCreateConfiguredEndpoint(cchar *home, cchar *documents, cchar *ip, int port)
{
    Http            *http;
    HttpHost        *host;
    HttpEndpoint    *endpoint;
    HttpRoute       *route;

    http = MPR->httpService;

    if (ip == 0) {
        /*  
            If no IP:PORT specified, find the first endpoint
         */
        if ((endpoint = mprGetFirstItem(http->endpoints)) != 0) {
            ip = endpoint->ip;
            port = endpoint->port;
        } else {
            ip = "localhost";
            if (port <= 0) {
                port = HTTP_DEFAULT_PORT;
            }
            if ((endpoint = httpCreateEndpoint(ip, port, NULL)) == 0) {
                return 0;
            }
        }
    } else {
        if ((endpoint = httpCreateEndpoint(ip, port, NULL)) == 0) {
            return 0;
        }
    }
    if ((host = httpCreateHost(home)) == 0) {
        return 0;
    }
    if ((route = httpCreateRoute(host)) == 0) {
        return 0;
    }
    httpSetHostDefaultRoute(host, route);
    httpSetHostIpAddr(host, ip, port);
    httpAddHostToEndpoint(endpoint, host);
    httpSetRouteDir(route, documents);
    httpFinalizeRoute(route);
    return endpoint;
}


static int destroyEndpointConnections(HttpEndpoint *endpoint)
{
    HttpConn    *conn;
    Http        *http;
    int         next;

    http = endpoint->http;
    lock(http);

    for (next = 0; (conn = mprGetNextItem(http->connections, &next)) != 0; ) {
        if (conn->endpoint == endpoint) {
            conn->endpoint = 0;
            httpDestroyConn(conn);
            next--;
        }
    }
    unlock(http);
    return 0;
}


static bool validateEndpoint(HttpEndpoint *endpoint)
{
    HttpHost    *host;

    if ((host = mprGetFirstItem(endpoint->hosts)) == 0) {
        mprError("Missing host object on endpoint");
        return 0;
    }
    return 1;
}


int httpStartEndpoint(HttpEndpoint *endpoint)
{
    HttpHost    *host;
    cchar       *proto, *ip;
    int         next;

    if (!validateEndpoint(endpoint)) {
        return MPR_ERR_BAD_ARGS;
    }
    for (ITERATE_ITEMS(endpoint->hosts, host, next)) {
        httpStartHost(host);
    }
    if ((endpoint->sock = mprCreateSocket(endpoint->ssl)) == 0) {
        return MPR_ERR_MEMORY;
    }
    if (mprListenOnSocket(endpoint->sock, endpoint->ip, endpoint->port, MPR_SOCKET_NODELAY | MPR_SOCKET_THREAD) < 0) {
        mprError("Can't open a socket on %s:%d", *endpoint->ip ? endpoint->ip : "*", endpoint->port);
        return MPR_ERR_CANT_OPEN;
    }
    if (endpoint->http->listenCallback && (endpoint->http->listenCallback)(endpoint) < 0) {
        return MPR_ERR_CANT_OPEN;
    }
    if (endpoint->async && endpoint->waitHandler ==  0) {
        //  MOB TODO -- this really should be in endpoint->listen->handler
        endpoint->waitHandler = mprCreateWaitHandler(endpoint->sock->fd, MPR_SOCKET_READABLE, endpoint->dispatcher,
            httpAcceptConn, endpoint, (endpoint->dispatcher) ? 0 : MPR_WAIT_NEW_DISPATCHER);
    } else {
        mprSetSocketBlockingMode(endpoint->sock, 1);
    }
    proto = mprIsSocketSecure(endpoint->sock) ? "HTTPS" : "HTTP ";
    ip = *endpoint->ip ? endpoint->ip : "*";
    if (mprIsSocketV6(endpoint->sock)) {
        mprLog(2, "Started %s service on \"[%s]:%d\"", proto, ip, endpoint->port);
    } else {
        mprLog(2, "Started %s service on \"%s:%d\"", proto, ip, endpoint->port);
    }
    return 0;
}


void httpStopEndpoint(HttpEndpoint *endpoint)
{
    HttpHost    *host;
    int         next;

    for (ITERATE_ITEMS(endpoint->hosts, host, next)) {
        httpStopHost(host);
    }
    if (endpoint->waitHandler) {
        mprRemoveWaitHandler(endpoint->waitHandler);
        endpoint->waitHandler = 0;
    }
    if (endpoint->sock) {
        mprRemoveSocketHandler(endpoint->sock);
        mprCloseSocket(endpoint->sock, 0);
        endpoint->sock = 0;
    }
}


/*
    TODO OPT
 */
bool httpValidateLimits(HttpEndpoint *endpoint, int event, HttpConn *conn)
{
    HttpLimits      *limits;
    Http            *http;
    cchar           *action;
    int             count, level, dir;

    limits = conn->limits;
    dir = HTTP_TRACE_RX;
    action = "unknown";
    mprAssert(conn->endpoint == endpoint);
    http = endpoint->http;

    lock(http);

    switch (event) {
    case HTTP_VALIDATE_OPEN_CONN:
        /*
            This measures active client systems with unique IP addresses.
         */
        if (endpoint->clientCount >= limits->clientMax) {
            unlock(http);
            /*  Abort connection */
            httpError(conn, HTTP_ABORT | HTTP_CODE_SERVICE_UNAVAILABLE, 
                "Too many concurrent clients %d/%d", endpoint->clientCount, limits->clientMax);
            return 0;
        }
        count = (int) PTOL(mprLookupKey(endpoint->clientLoad, conn->ip));
        mprAddKey(endpoint->clientLoad, conn->ip, ITOP(count + 1));
        endpoint->clientCount = (int) mprGetHashLength(endpoint->clientLoad);
        action = "open conn";
        dir = HTTP_TRACE_RX;
        break;

    case HTTP_VALIDATE_CLOSE_CONN:
        count = (int) PTOL(mprLookupKey(endpoint->clientLoad, conn->ip));
        if (count > 1) {
            mprAddKey(endpoint->clientLoad, conn->ip, ITOP(count - 1));
        } else {
            mprRemoveKey(endpoint->clientLoad, conn->ip);
        }
        endpoint->clientCount = (int) mprGetHashLength(endpoint->clientLoad);
        action = "close conn";
        dir = HTTP_TRACE_TX;
        break;
    
    case HTTP_VALIDATE_OPEN_REQUEST:
        mprAssert(conn->rx);
        if (endpoint->requestCount >= limits->requestMax) {
            unlock(http);
            httpError(conn, HTTP_CODE_SERVICE_UNAVAILABLE, "Server overloaded");
            mprLog(2, "Too many concurrent requests %d/%d", endpoint->requestCount, limits->requestMax);
            return 0;
        }
        endpoint->requestCount++;
        conn->rx->flags |= HTTP_LIMITS_OPENED;
        action = "open request";
        dir = HTTP_TRACE_RX;
        break;

    case HTTP_VALIDATE_CLOSE_REQUEST:
        if (conn->rx && conn->rx->flags & HTTP_LIMITS_OPENED) {
            /* Requests incremented only when conn->rx is assigned */
            endpoint->requestCount--;
            mprAssert(endpoint->requestCount >= 0);
            action = "close request";
            dir = HTTP_TRACE_TX;
            conn->rx->flags &= ~HTTP_LIMITS_OPENED;
        }
        break;

    case HTTP_VALIDATE_OPEN_PROCESS:
        if (http->processCount >= limits->processMax) {
            unlock(http);
            httpError(conn, HTTP_CODE_SERVICE_UNAVAILABLE, "Server overloaded");
            mprLog(2, "Too many concurrent processes %d/%d", http->processCount, limits->processMax);
            return 0;
        }
        http->processCount++;
        action = "start process";
        dir = HTTP_TRACE_RX;
        break;

    case HTTP_VALIDATE_CLOSE_PROCESS:
        http->processCount--;
        mprAssert(http->processCount >= 0);
        break;
    }
    if (event == HTTP_VALIDATE_CLOSE_CONN || event == HTTP_VALIDATE_CLOSE_REQUEST) {
        if ((level = httpShouldTrace(conn, dir, HTTP_TRACE_LIMITS, NULL)) >= 0) {
            LOG(4, "Validate request for %s. Active connections %d, active requests: %d/%d, active client IP %d/%d", 
                action, mprGetListLength(http->connections), endpoint->requestCount, limits->requestMax, 
                endpoint->clientCount, limits->clientMax);
        }
    }
    unlock(http);
    return 1;
}


/*  
    Accept a new client connection on a new socket. If multithreaded, this will come in on a worker thread 
    dedicated to this connection. This is called from the listen wait handler.
 */
HttpConn *httpAcceptConn(HttpEndpoint *endpoint, MprEvent *event)
{
    HttpConn        *conn;
    MprSocket       *sock;
    MprDispatcher   *dispatcher;
    MprEvent        e;
    int             level;

    mprAssert(endpoint);
    mprAssert(event);

    /*
        This will block in sync mode until a connection arrives
     */
    if ((sock = mprAcceptSocket(endpoint->sock)) == 0) {
        if (endpoint->waitHandler) {
            mprWaitOn(endpoint->waitHandler, MPR_READABLE);
        }
        return 0;
    }
    if (endpoint->waitHandler) {
        /* Re-enable events on the listen socket */
        mprWaitOn(endpoint->waitHandler, MPR_READABLE);
    }
    dispatcher = event->dispatcher;

    if (mprShouldDenyNewRequests()) {
        mprCloseSocket(sock, 0);
        return 0;
    }
    if ((conn = httpCreateConn(endpoint->http, endpoint, dispatcher)) == 0) {
        mprCloseSocket(sock, 0);
        return 0;
    }
    conn->notifier = endpoint->notifier;
    conn->async = endpoint->async;
    conn->endpoint = endpoint;
    conn->sock = sock;
    conn->port = sock->port;
    conn->ip = sclone(sock->ip);
    conn->secure = mprIsSocketSecure(sock);

    if (!httpValidateLimits(endpoint, HTTP_VALIDATE_OPEN_CONN, conn)) {
        conn->endpoint = 0;
        httpDestroyConn(conn);
        return 0;
    }
    mprAssert(conn->state == HTTP_STATE_BEGIN);
    httpSetState(conn, HTTP_STATE_CONNECTED);

    if ((level = httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_CONN, NULL)) >= 0) {
        mprLog(level, "### Incoming connection from %s:%d to %s:%d %s", 
            conn->ip, conn->port, sock->acceptIp, sock->acceptPort, conn->secure ? "(secure)" : "");
    }
    e.mask = MPR_READABLE;
    e.timestamp = conn->http->now;
    (conn->ioCallback)(conn, &e);
    return conn;
}


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
    if (conn->rx->traceLevel >= 0) {
        mprLog(conn->rx->traceLevel, "Select host: \"%s\"", host->name);
    }
    conn->host = host;
}


void *httpGetEndpointContext(HttpEndpoint *endpoint)
{
    return endpoint->context;
}


int httpIsEndpointAsync(HttpEndpoint *endpoint) 
{
    return endpoint->async;
}


void httpSetEndpointAddress(HttpEndpoint *endpoint, cchar *ip, int port)
{
    if (ip) {
        endpoint->ip = sclone(ip);
    }
    if (port >= 0) {
        endpoint->port = port;
    }
    if (endpoint->sock) {
        httpStopEndpoint(endpoint);
        httpStartEndpoint(endpoint);
    }
}


void httpSetEndpointAsync(HttpEndpoint *endpoint, int async)
{
    if (endpoint->sock) {
        if (endpoint->async && !async) {
            mprSetSocketBlockingMode(endpoint->sock, 1);
        }
        if (!endpoint->async && async) {
            mprSetSocketBlockingMode(endpoint->sock, 0);
        }
    }
    endpoint->async = async;
}


void httpSetEndpointContext(HttpEndpoint *endpoint, void *context)
{
    mprAssert(endpoint);
    endpoint->context = context;
}


void httpSetEndpointNotifier(HttpEndpoint *endpoint, HttpNotifier notifier)
{
    mprAssert(endpoint);
    endpoint->notifier = notifier;
}


int httpSecureEndpoint(HttpEndpoint *endpoint, struct MprSsl *ssl)
{
#if BIT_FEATURE_SSL
    endpoint->ssl = ssl;
    return 0;
#else
    return MPR_ERR_BAD_STATE;
#endif
}


int httpSecureEndpointByName(cchar *name, struct MprSsl *ssl)
{
    HttpEndpoint    *endpoint;
    Http            *http;
    char            *ip;
    int             port, next, count;

    http = MPR->httpService;
    mprParseSocketAddress(name, &ip, &port, -1);
    if (ip == 0) {
        ip = "";
    }
    for (count = 0, next = 0; (endpoint = mprGetNextItem(http->endpoints, &next)) != 0; ) {
        if (endpoint->port <= 0 || port <= 0 || endpoint->port == port) {
            mprAssert(endpoint->ip);
            if (*endpoint->ip == '\0' || *ip == '\0' || scmp(endpoint->ip, ip) == 0) {
                httpSecureEndpoint(endpoint, ssl);
                count++;
            }
        }
    }
    return (count == 0) ? MPR_ERR_CANT_FIND : 0;
}


void httpAddHostToEndpoint(HttpEndpoint *endpoint, HttpHost *host)
{
    mprAddItem(endpoint->hosts, host);
    if (endpoint->limits == 0) {
        endpoint->limits = host->defaultRoute->limits;
    }
}


bool httpHasNamedVirtualHosts(HttpEndpoint *endpoint)
{
    return endpoint->flags & HTTP_NAMED_VHOST;
}


void httpSetHasNamedVirtualHosts(HttpEndpoint *endpoint, bool on)
{
    if (on) {
        endpoint->flags |= HTTP_NAMED_VHOST;
    } else {
        endpoint->flags &= ~HTTP_NAMED_VHOST;
    }
}


HttpHost *httpLookupHostOnEndpoint(HttpEndpoint *endpoint, cchar *name)
{
    HttpHost    *host;
    int         next;

    if (name == 0 || *name == '\0') {
        return mprGetFirstItem(endpoint->hosts);
    }
    for (next = 0; (host = mprGetNextItem(endpoint->hosts, &next)) != 0; ) {
        if (smatch(host->name, name)) {
            return host;
        }
        if (*host->name == '*') {
            if (host->name[1] == '\0') {
                return host;
            }
            if (scontains(name, &host->name[1], -1)) {
                return host;
            }
        }
    }
    return 0;
}


int httpConfigureNamedVirtualEndpoints(Http *http, cchar *ip, int port)
{
    HttpEndpoint    *endpoint;
    int             next, count;

    if (ip == 0) {
        ip = "";
    }
    for (count = 0, next = 0; (endpoint = mprGetNextItem(http->endpoints, &next)) != 0; ) {
        if (endpoint->port <= 0 || port <= 0 || endpoint->port == port) {
            mprAssert(endpoint->ip);
            if (*endpoint->ip == '\0' || *ip == '\0' || scmp(endpoint->ip, ip) == 0) {
                httpSetHasNamedVirtualHosts(endpoint, 1);
                count++;
            }
        }
    }
    return (count == 0) ? MPR_ERR_CANT_FIND : 0;
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
