/*
    server.c -- Create Http servers.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

static int manageServer(HttpServer *server, int flags);
static int destroyServerConnections(HttpServer *server);

/************************************ Code ************************************/
/*
    Create a server listening on ip:port. NOTE: ip may be empty which means bind to all addresses.
 */
HttpServer *httpCreateServer(cchar *ip, int port, MprDispatcher *dispatcher, int flags)
{
    HttpServer  *server;
    Http        *http;

    if ((server = mprAllocObj(HttpServer, manageServer)) == 0) {
        return 0;
    }
    http = MPR->httpService;
    server->http = http;
    server->clientLoad = mprCreateHash(HTTP_CLIENTS_HASH, MPR_HASH_STATIC_VALUES);
    server->async = 1;
    server->http = MPR->httpService;
    server->port = port;
    server->ip = sclone(ip);
    server->dispatcher = dispatcher;
    if (server->ip && *server->ip) {
        server->name = server->ip;
    }
    server->loc = httpInitLocation(http, 1);
    server->hosts = mprCreateList(-1, 0);
    httpAddServer(http, server);
    if (flags & HTTP_CREATE_HOST) {
        httpAddHostToServer(server, httpCreateHost(ip, port, server->loc));
    }
    return server;
}


void httpDestroyServer(HttpServer *server)
{
    mprLog(4, "Destroy server %s", server->name);
    if (server->waitHandler) {
        mprRemoveWaitHandler(server->waitHandler);
        server->waitHandler = 0;
    }
    destroyServerConnections(server);
    if (server->sock) {
        mprCloseSocket(server->sock, 0);
        server->sock = 0;
    }
    httpRemoveServer(MPR->httpService, server);
}


static int manageServer(HttpServer *server, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(server->http);
        mprMark(server->loc);
        mprMark(server->limits);
        mprMark(server->waitHandler);
        mprMark(server->clientLoad);
        mprMark(server->hosts);
        mprMark(server->name);
        mprMark(server->ip);
        mprMark(server->context);
        mprMark(server->meta);
        mprMark(server->sock);
        mprMark(server->dispatcher);
        mprMark(server->ssl);

    } else if (flags & MPR_MANAGE_FREE) {
        httpDestroyServer(server);
    }
    return 0;
}


/*  
    Convenience function to create and configure a new server without using a config file.
 */
HttpServer *httpCreateConfiguredServer(cchar *docRoot, cchar *ip, int port)
{
    Http            *http;
    HttpHost        *host;
    HttpServer      *server;

    http = MPR->httpService;

    if (ip == 0) {
        /*  
            If no IP:PORT specified, find the first server
         */
        if ((server = mprGetFirstItem(http->servers)) != 0) {
            ip = server->ip;
            port = server->port;
        } else {
            ip = "localhost";
            if (port <= 0) {
                port = HTTP_DEFAULT_PORT;
            }
            server = httpCreateServer(ip, port, NULL, HTTP_CREATE_HOST);
        }
    } else {
        server = httpCreateServer(ip, port, NULL, HTTP_CREATE_HOST);
    }
    if ((host->mimeTypes = mprCreateMimeTypes("mime.types")) == 0) {
        host->mimeTypes = MPR->mimeTypes;
    }
    httpAddDir(host, httpCreateBareDir(docRoot));
    httpSetHostDocumentRoot(host, docRoot);
    return server;
}


static int destroyServerConnections(HttpServer *server)
{
    HttpConn    *conn;
    Http        *http;
    int         next;

    http = server->http;
    lock(http);

    for (next = 0; (conn = mprGetNextItem(http->connections, &next)) != 0; ) {
        if (conn->server == server) {
            conn->server = 0;
            httpDestroyConn(conn);
            next--;
        }
    }
    unlock(http);
    return 0;
}


static bool validateServer(HttpServer *server)
{
    HttpHost    *host;

    if ((host = mprGetFirstItem(server->hosts)) == 0) {
        mprError("Missing host object on server");
        return 0;
    }
    if (mprGetListLength(host->aliases) == 0) {
        httpAddAlias(host, httpCreateAlias("", host->documentRoot, 0));
    }
#if UNUSED
    if (!host->documentRoot) {
        host->documentRoot = server->documentRoot;
    }
    if (!host->serverRoot) {
        host->serverRoot = server->serverRoot;
    }
#endif
    return 1;
}


int httpStartServer(HttpServer *server)
{
    cchar   *proto, *ip;

    if (!validateServer(server)) {
        return MPR_ERR_BAD_ARGS;
    }
    if ((server->sock = mprCreateSocket(server->ssl)) == 0) {
        return MPR_ERR_MEMORY;
    }
    if (mprListenOnSocket(server->sock, server->ip, server->port, MPR_SOCKET_NODELAY | MPR_SOCKET_THREAD) < 0) {
        mprError("Can't open a socket on %s, port %d", server->ip, server->port);
        return MPR_ERR_CANT_OPEN;
    }
    if (server->http->listenCallback && (server->http->listenCallback)(server) < 0) {
        return MPR_ERR_CANT_OPEN;
    }
    if (server->async && server->waitHandler ==  0) {
        server->waitHandler = mprCreateWaitHandler(server->sock->fd, MPR_SOCKET_READABLE, server->dispatcher,
            (MprEventProc) httpAcceptConn, server, (server->dispatcher) ? 0 : MPR_WAIT_NEW_DISPATCHER);
    } else {
        mprSetSocketBlockingMode(server->sock, 1);
    }
    proto = mprIsSocketSecure(server->sock) ? "HTTPS" : "HTTP ";
    ip = *server->ip ? server->ip : "*";
    mprLog(MPR_CONFIG, "Started %s server on \"%s:%d\"", proto, ip, server->port);
    return 0;
}


void httpStopServer(HttpServer *server)
{
    mprCloseSocket(server->sock, 0);
    server->sock = 0;
}


int httpValidateLimits(HttpServer *server, int event, HttpConn *conn)
{
    HttpLimits      *limits;
    int             count;

    limits = server->limits;
    mprAssert(conn->server == server);
    lock(server->http);

    switch (event) {
    case HTTP_VALIDATE_OPEN_CONN:
        if (server->clientCount >= limits->clientCount) {
            unlock(server->http);
            httpConnError(conn, HTTP_CODE_SERVICE_UNAVAILABLE, 
                "Too many concurrent clients %d/%d", server->clientCount, limits->clientCount);
            return 0;
        }
        count = (int) PTOL(mprLookupHash(server->clientLoad, conn->ip));
        mprAddKey(server->clientLoad, conn->ip, ITOP(count + 1));
        server->clientCount = mprGetHashLength(server->clientLoad);
        break;

    case HTTP_VALIDATE_CLOSE_CONN:
        count = (int) PTOL(mprLookupHash(server->clientLoad, conn->ip));
        if (count > 1) {
            mprAddKey(server->clientLoad, conn->ip, ITOP(count - 1));
        } else {
            mprRemoveHash(server->clientLoad, conn->ip);
        }
        server->clientCount = mprGetHashLength(server->clientLoad);
        mprLog(4, "Close connection %d. Active requests %d, active clients %d", 
            conn->seqno, server->requestCount, server->clientCount);
        break;
    
    case HTTP_VALIDATE_OPEN_REQUEST:
        if (server->requestCount >= limits->requestCount) {
            //  MOB -- will CLOSE_REQUEST get called and thus set the limit to negative?
            unlock(server->http);
            httpConnError(conn, HTTP_CODE_SERVICE_UNAVAILABLE, 
                "Too many concurrent requests %d/%d", server->requestCount, limits->requestCount);
            return 0;
        }
        server->requestCount++;
        break;

    case HTTP_VALIDATE_CLOSE_REQUEST:
        server->requestCount--;
        mprAssert(server->requestCount >= 0);
        mprLog(4, "Close request. Active requests %d, active clients %d", server->requestCount, server->clientCount);
        break;
    }
    mprLog(6, "Validate request. Counts: requests: %d/%d, clients %d/%d", 
        server->requestCount, limits->requestCount, server->clientCount, limits->clientCount);
    unlock(server->http);
    return 1;
}


/*  
    Accept a new client connection on a new socket. If multithreaded, this will come in on a worker thread 
    dedicated to this connection. This is called from the listen wait handler.
 */
HttpConn *httpAcceptConn(HttpServer *server, MprEvent *event)
{
    HttpConn        *conn;
    MprSocket       *sock;
    MprDispatcher   *dispatcher;
    MprEvent        e;
    int             level;

    mprAssert(server);

    /*
        This will block in sync mode until a connection arrives
     */
    sock = mprAcceptSocket(server->sock);
    if (server->waitHandler) {
        mprEnableWaitEvents(server->waitHandler, MPR_READABLE);
    }
    if (sock == 0) {
        return 0;
    }
    dispatcher = (event && server->dispatcher == 0) ? event->dispatcher: server->dispatcher;
    mprLog(4, "New connection from %s:%d to %s:%d %s",
        sock->ip, sock->port, sock->acceptIp, sock->acceptPort, server->sock->sslSocket ? "(secure)" : "");

    if ((conn = httpCreateConn(server->http, server, dispatcher)) == 0) {
        mprError("Can't create connect object. Insufficient memory.");
        mprCloseSocket(sock, 0);
        return 0;
    }
    conn->notifier = server->notifier;
    conn->async = server->async;
    conn->server = server;
    conn->sock = sock;
    conn->port = sock->port;
    conn->ip = sclone(sock->ip);
    conn->secure = mprIsSocketSecure(sock);

    if (!httpValidateLimits(server, HTTP_VALIDATE_OPEN_CONN, conn)) {
        /* Prevent validate limits from */
        conn->server = 0;
        httpDestroyConn(conn);
        return 0;
    }
    mprAssert(conn->state == HTTP_STATE_BEGIN);
    httpSetState(conn, HTTP_STATE_CONNECTED);

    if ((level = httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_CONN, NULL)) >= 0) {
        mprLog(level, "### Incoming connection from %s:%d to %s:%d", 
            conn->ip, conn->port, conn->sock->ip, conn->sock->port);
    }
    e.mask = MPR_READABLE;
    e.timestamp = mprGetTime();
    (conn->ioCallback)(conn, &e);
    return conn;
}


//  MOB Is this used / needed
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


void httpSetServerAddress(HttpServer *server, cchar *ip, int port)
{
    HttpHost    *host;
    int         next;

    if (ip) {
        server->ip = sclone(ip);
    }
    if (port >= 0) {
        server->port = port;
    }
    for (next = 0; (host = mprGetNextItem(server->hosts, &next)) != 0; ) {
        httpSetHostAddress(host, ip, port);
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
    if (server->sock) {
        if (server->async && !async) {
            mprSetSocketBlockingMode(server->sock, 1);
        }
        if (!server->async && async) {
            mprSetSocketBlockingMode(server->sock, 0);
        }
    }
    server->async = async;
}


void httpSetServerContext(HttpServer *server, void *context)
{
    mprAssert(server);
    server->context = context;
}


void httpSetServerLocation(HttpServer *server, HttpLoc *loc)
{
    mprAssert(server);
    mprAssert(loc);
    server->loc = loc;
}


void httpSetServerName(HttpServer *server, cchar *name)
{
    mprAssert(server);
    server->name = sclone(name);
}


void httpSetServerNotifier(HttpServer *server, HttpNotifier notifier)
{
    mprAssert(server);
    server->notifier = notifier;
}


int httpSecureServer(cchar *ip, int port, struct MprSsl *ssl)
{
#if BLD_FEATURE_SSL
    HttpServer  *server;
    Http        *http;
    int         next, count;

    http = MPR->httpService;
    if (ip == 0) {
        ip = "";
    }
    for (count = next = 0; (server = mprGetNextItem(http->servers, &next)) != 0; ) {
        if (server->port <= 0 || port <= 0 || server->port == port) {
            mprAssert(server->ip);
            if (*server->ip == '\0' || *ip == '\0' || scmp(server->ip, ip) == 0) {
                server->ssl = ssl;
                count++;
            }
        }
    }
    return (count == 0) ? MPR_ERR_CANT_FIND : 0;
#else
    return MPR_ERR_BAD_STATE;
#endif
}


void httpAddHostToServer(HttpServer *server, HttpHost *host)
{
    mprAddItem(server->hosts, host);
    if (server->limits == 0) {
        server->limits = host->limits;
    }
}


#if UNUSED
HttpServer *httpRemoveHostFromServer(Http *http, cchar *ip, int port, HttpHost *host)
{
    HttpServer  *server;
    int         next;

    for (next = 0; (server = mprGetNextItem(http->servers, &next)) != 0; ) {
        if (server->port < 0 || port < 0 || server->port == port) {
            if (ip == 0 || server->ip == 0 || strcmp(server->ip, ip) == 0) {
                mprRemoveItem(server->hosts, host);
            }
        }
    }
    return 0;
}
#endif


bool httpIsNamedVirtualServer(HttpServer *server)
{
    return server->flags & HTTP_IPADDR_VHOST;
}


void httpSetNamedVirtualServer(HttpServer *server)
{
    server->flags |= HTTP_IPADDR_VHOST;
}


HttpHost *httpLookupHostByName(HttpServer *server, cchar *name)
{
    HttpHost    *host;
    int         next;

    for (next = 0; (host = mprGetNextItem(server->hosts, &next)) != 0; ) {
        if (name == 0 || strcmp(name, host->name) == 0) {
            return host;
        }
    }
    return 0;
}


#if UNUSED
void httpSetServerLimits(HttpServer *server, HttpLimits *limits)
{
    server->limits = limits;
}


/*  
    Create the host addresses for a host. Called for hosts or for NameVirtualHost directives (host == 0).
 */
int httpCreateEndpoints(HttpHost *host, cchar *configValue)
{
    Http            *http;
    HttpServer      *server;
    HttpServer      *endpoint;
    char            *ipAddrPort, *ip, *value, *tok;
    int             next, port;

    http = MPR->httpService;
    endpoint = 0;
    value = sclone(configValue);
    ipAddrPort = stok(value, " \t", &tok);

    while (ipAddrPort) {
        if (scasecmp(ipAddrPort, "_default_") == 0) {
            //  TODO is this used?
            mprAssert(0);
            ipAddrPort = "*:*";
        }
        if (mprParseIp(ipAddrPort, &ip, &port, -1) < 0) {
            mprError("Can't parse ipAddr %s", ipAddrPort);
            continue;
        }
        mprAssert(ip && *ip);
        if (ip[0] == '*') {
            ip = sclone("");
        }

        /*  
            For each listening endpiont
         */
        for (next = 0; (server = mprGetNextItem(http->servers, &next)) != 0; ) {
            if (port > 0 && port != server->port) {
                continue;
            }
            if (server->ip[0] != '\0' && ip[0] != '\0' && strcmp(ip, server->ip) != 0) {
                continue;
            }

            /*
                Find the matching endpoint or create a new one
             */
            if ((endpoint = httpLookupEndpoint(http, server->ip, server->port)) == 0) {
                endpoint = httpCreateEndpoint(server->ip, server->port);
                mprAddItem(http->servers, endpoint);
            }
            if (host == 0) {
                httpSetNamedVirtualEndpoint(endpoint);

            } else {
                httpAddHostToEndpoint(endpoint, host);
                if (server->ip[0] != '\0') {
                    httpSetHostName(host, mprAsprintf("%s:%d", server->ip, server->port));
                } else {
                    httpSetHostName(host, mprAsprintf("%s:%d", ip, server->port));
                }
            }
        }
        ipAddrPort = stok(0, " \t", &tok);
    }
    if (host) {
        if (endpoint == 0) {
            mprError("No valid IP address for host %s", host->name);
            return MPR_ERR_CANT_INITIALIZE;
        }
        if (httpIsNamedVirtualEndpoint(endpoint)) {
            httpSetNamedVirtualHost(host);
        }
    }
    return 0;
}
#endif


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
