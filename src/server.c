/*
    server.c -- Create Http servers.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

static int acceptConnEvent(HttpServer *server, MprEvent *event);

/*********************************** Code *************************************/
/*
    Create a server listening on ipAddress:port. NOTE: ipAddr may be empty which means bind to all addresses.
 */
HttpServer *httpCreateServer(Http *http, cchar *ip, int port, MprDispatcher *dispatcher)
{
    HttpServer    *server;

    mprAssert(ip);
    mprAssert(port > 0);

    server = mprAllocObjZeroed(http, HttpServer);
    if (server == 0) {
        return 0;
    }
    server->async = 1;
    server->http = http;
    server->port = port;
    server->ip = mprStrdup(server, ip);
    server->waitHandler.fd = -1;
    server->dispatcher = (dispatcher) ? dispatcher : mprGetDispatcher(http);
    server->name = HTTP_NAME;
    return server;
}


int httpStartServer(HttpServer *server)
{
    cchar       *proto;
    char        *ip;

    server->sock = mprCreateSocket(server, server->ssl);

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
            (MprEventProc) acceptConnEvent, server);
    } else {
        mprSetSocketBlockingMode(server->sock, 1);
    }
    LOG(server, MPR_CONFIG, "Started %s server on %s:%d", proto, ip, server->port);
    return 0;
}


void httpStopServer(HttpServer *server)
{
    mprFree(server->sock);
    server->sock = 0;
}


/*  Accept a new client connection on a new socket. If multithreaded, this will come in on a worker thread 
    dedicated to this connection. This is called from the listen wait handler.
 */
static HttpConn *acceptConn(HttpServer *server)
{
    HttpConn        *conn;
    MprSocket       *sock;

    mprAssert(server);

    sock = mprAcceptSocket(server->sock);
    if (server->waitHandler.fd >= 0) {
        mprEnableWaitEvents(&server->waitHandler, MPR_READABLE);
    }
    if (sock == 0) {
        return 0;
    }
    LOG(server, 4, "New connection from %s:%d for %s:%d %s",
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

    httpSetState(conn, HTTP_STATE_STARTED);

    if (!conn->async) {
        if (httpWait(conn, HTTP_STATE_PARSED, -1) < 0) {
            mprError(server, "Timeout while accepting.");
            return 0;
        }
    }
    return conn;
}


//  TODO - refactor. Should call directly into acceptConn which should invoke the callback
//  TODO - should this be void?

static int acceptConnEvent(HttpServer *server, MprEvent *event)
{
    HttpConn    *conn;
    MprEvent    e;

    if ((conn = acceptConn(server)) == 0) {
        return 0;
    }
    e.mask = MPR_READABLE;
    e.timestamp = mprGetTime(server);
    (conn->callback)(conn->callbackArg, &e);
    return 0;
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


void httpSetMetaServer(HttpServer *server, void *meta)
{
    server->meta = meta;
}

void httpSetServerContext(HttpServer *server, void *context)
{
    server->context = context;
}


void httpSetServerNotifier(HttpServer *server, HttpNotifier notifier)
{
    server->notifier = notifier;
}


void httpSetServerAsync(HttpServer *server, int async)
{
    server->async = async;
}


void httpSetDocumentRoot(HttpServer *server, cchar *documentRoot)
{
    mprFree(server->documentRoot);
    server->documentRoot = mprStrdup(server, documentRoot);
}


void httpSetServerRoot(HttpServer *server, cchar *serverRoot)
{
    mprFree(server->serverRoot);
    server->serverRoot = mprStrdup(server, serverRoot);
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
