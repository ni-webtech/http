/*
    server.c -- Create Http servers.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

static int destroyServer(HttpServer *server);

/************************************ Code ************************************/
/*
    Create a server listening on ip:port. NOTE: ip may be empty which means bind to all addresses.
 */
HttpServer *httpCreateServer(Http *http, cchar *ip, int port, MprDispatcher *dispatcher)
{
    HttpServer    *server;

    mprAssert(ip);
    mprAssert(port > 0);

    server = mprAllocObjWithDestructorZeroed(http, HttpServer, destroyServer);
    if (server == 0) {
        return 0;
    }
    server->clients = mprCreateHash(server, HTTP_CLIENTS_HASH);
    server->async = 1;
    server->http = http;
    server->port = port;
    server->ip = mprStrdup(server, ip);
    server->waitHandler.fd = -1;
    server->dispatcher = (dispatcher) ? dispatcher : mprGetDispatcher(http);
    if (server->ip && server->ip) {
        server->name = server->ip;
    }
    server->software = HTTP_NAME;
    server->limits = httpCreateLimits(server, 1);
    server->loc = httpInitLocation(http, server, 1);
    return server;
}


static int destroyServer(HttpServer *server)
{
    if (server->waitHandler.fd >= 0) {
        mprRemoveWaitHandler(&server->waitHandler);
    }
    mprFree(server->sock);
    return 0;
}


int httpStartServer(HttpServer *server)
{
    cchar       *proto;
    char        *ip;

    if ((server->sock = mprCreateSocket(server, server->ssl)) == 0) {
        return MPR_ERR_NO_MEMORY;
    }
    if (mprOpenServerSocket(server->sock, server->ip, server->port, MPR_SOCKET_NODELAY | MPR_SOCKET_THREAD) < 0) {
        mprError(server, "Can't open a socket on %s, port %d", server->ip, server->port);
        return MPR_ERR_CANT_OPEN;
    }
    if (mprListenOnSocket(server->sock) < 0) {
        mprError(server, "Can't listen on %s, port %d", server->ip, server->port);
        return MPR_ERR_CANT_OPEN;
    }
    proto = "HTTP";
    if (mprIsSocketSecure(server->sock)) {
        proto = "HTTPS";
    }
    ip = server->ip;
    if (ip == 0 || *ip == '\0') {
        ip = "*";
    }
    if (server->async) {
        mprInitWaitHandler(server, &server->waitHandler, server->sock->fd, MPR_SOCKET_READABLE, server->dispatcher,
            (MprEventProc) httpAcceptConn, server);
    } else {
        mprSetSocketBlockingMode(server->sock, 1);
    }
    mprLog(server, MPR_CONFIG, "Started %s server on %s:%d", proto, ip, server->port);
    return 0;
}


void httpStopServer(HttpServer *server)
{
    mprFree(server->sock);
    server->sock = 0;
}


int httpValidateLimits(HttpServer *server, int event, HttpConn *conn)
{
    HttpLimits      *limits;
    int             count;

    limits = server->limits;
    mprAssert(conn->server == server);

    switch (event) {
    case HTTP_VALIDATE_OPEN_CONN:
        if (server->clientCount >= limits->clientCount) {
            httpConnError(conn, HTTP_CODE_SERVICE_UNAVAILABLE, 
                "Too many concurrent clients %d/%d", server->clientCount, limits->clientCount);
            return 0;
        }
        count = (int) PTOL(mprLookupHash(server->clients, conn->ip));
        mprAddHash(server->clients, conn->ip, ITOP(count + 1));
        server->clientCount = mprGetHashCount(server->clients);
        break;

    case HTTP_VALIDATE_CLOSE_CONN:
        count = (int) PTOL(mprLookupHash(server->clients, conn->ip));
        if (count > 1) {
            mprAddHash(server->clients, conn->ip, ITOP(count - 1));
        } else {
            mprRemoveHash(server->clients, conn->ip);
        }
        server->clientCount = mprGetHashCount(server->clients);
        mprLog(server, 4, "Close connection. Active requests %d, active clients %d", 
            server->requestCount, server->clientCount);
        break;
    
    case HTTP_VALIDATE_OPEN_REQUEST:
        if (server->requestCount >= limits->requestCount) {
            httpConnError(conn, HTTP_CODE_SERVICE_UNAVAILABLE, 
                "Too many concurrent requests %d/%d", server->requestCount, limits->requestCount);
            return 0;
        }
        server->requestCount++;
        break;

    case HTTP_VALIDATE_CLOSE_REQUEST:
        server->requestCount--;
        mprAssert(server->requestCount >= 0);
        mprLog(server, 4, "Close request. Active requests %d, active clients %d", server->requestCount, server->clientCount);
        break;
    }
    mprLog(server, 6, "Validate request. Counts: requests: %d/%d, clients %d/%d", 
        server->requestCount, limits->requestCount, server->clientCount, limits->clientCount);
    return 1;
}


/*  
    Accept a new client connection on a new socket. If multithreaded, this will come in on a worker thread 
    dedicated to this connection. This is called from the listen wait handler.
 */
HttpConn *httpAcceptConn(HttpServer *server)
{
    HttpConn        *conn;
    MprSocket       *sock;
    MprEvent        e;
    int             level;

    mprAssert(server);

    /*
        This will block in sync mode until a connection arrives
     */
    sock = mprAcceptSocket(server->sock);
    if (server->waitHandler.fd >= 0) {
        mprEnableWaitEvents(&server->waitHandler, MPR_READABLE);
    }
    if (sock == 0) {
        return 0;
    }
    mprLog(server, 4, "New connection from %s:%d to %s:%d %s",
        sock->ip, sock->port, sock->acceptIp, sock->acceptPort, server->sock->sslSocket ? "(secure)" : "");

    if ((conn = httpCreateConn(server->http, server)) == 0) {
        mprError(server, "Can't create connect object. Insufficient memory.");
        mprFree(sock);
        return 0;
    }
    mprStealBlock(conn, sock);
    conn->async = server->async;
    conn->server = server;
    conn->sock = sock;
    conn->port = sock->port;
    conn->ip = mprStrdup(conn, sock->ip);
    conn->secure = mprIsSocketSecure(sock);

    if (!httpValidateLimits(server, HTTP_VALIDATE_OPEN_CONN, conn)) {
        mprFree(conn);
        return 0;
    }
    mprAssert(conn->state == HTTP_STATE_BEGIN);
    httpSetState(conn, HTTP_STATE_CONNECTED);

    if ((level = httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_CONN, NULL)) >= 0) {
        mprLog(conn, level, "### New Connection from %s:%d to %s:%d", 
            conn->ip, conn->port, conn->sock->ip, conn->sock->port);
    }
    e.mask = MPR_READABLE;
    e.timestamp = mprGetTime(server);
    (conn->callback)(conn->callbackArg, &e);
    return conn;
}


void *httpGetMetaServer(HttpServer *server)
{
    return server->meta;
}


void *httpGetServerContext(HttpServer *server)
{
    return server->context;
}


int httpGetServerAsync(HttpServer *server) 
{
    return server->async;
}


void httpSetDocumentRoot(HttpServer *server, cchar *documentRoot)
{
    mprFree(server->documentRoot);
    server->documentRoot = mprStrdup(server, documentRoot);
}


void httpSetIpAddr(HttpServer *server, cchar *ip, int port)
{
    if (ip) {
        mprFree(server->ip);
        server->ip = mprStrdup(server, ip);
    }
    if (port >= 0) {
        server->port = port;
    }
    if (server->sock) {
        httpStopServer(server);
        httpStartServer(server);
    }
}

void httpSetMetaServer(HttpServer *server, void *meta)
{
    server->meta = meta;
}


void httpSetServerAsync(HttpServer *server, int async)
{
    server->async = async;
}


void httpSetServerContext(HttpServer *server, void *context)
{
    server->context = context;
}


void httpSetServerLocation(HttpServer *server, HttpLoc *loc)
{
    mprFree(loc);
    server->loc = loc;
}


void httpSetServerName(HttpServer *server, cchar *name)
{
    server->name = mprStrdup(server, name);
}


void httpSetServerNotifier(HttpServer *server, HttpNotifier notifier)
{
    server->notifier = notifier;
}


void httpSetServerRoot(HttpServer *server, cchar *serverRoot)
{
    mprFree(server->serverRoot);
    server->serverRoot = mprStrdup(server, serverRoot);
}


void httpSetServerSoftware(HttpServer *server, cchar *software)
{
    server->software = mprStrdup(server, software);
}


void httpSetServerSsl(HttpServer *server, struct MprSsl *ssl)
{
    server->ssl = ssl;
}


/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
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
    
    @end
 */
