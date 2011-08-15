/*
    match.c -- HTTP pipeline matching.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static HttpStage *processDirectories(HttpConn *conn, HttpStage *handler);
static char *getExtension(HttpConn *conn, cchar *path);
static HttpStage *checkHandler(HttpConn *conn, HttpStage *stage);
static HttpStage *findHandler(HttpConn *conn);

/*********************************** Code *************************************/

void httpMatchHost(HttpConn *conn)
{ 
    MprSocket       *listenSock;
    HttpServer      *server;
    HttpHost        *host;
    Http            *http;

    http = conn->http;
    listenSock = conn->sock->listenSock;

    if ((server = httpLookupServer(http, listenSock->ip, listenSock->port)) == 0) {
        mprError("No server for request from %s:%d", listenSock->ip, listenSock->port);
        mprCloseSocket(conn->sock, 0);
        return;
    }
    if (httpIsNamedVirtualServer(server)) {
        host = httpLookupHost(server, conn->rx->hostHeader);
    } else {
        host = mprGetFirstItem(server->hosts);
    }
    if (host == 0) {
        httpSetConnHost(conn, 0);
        httpError(conn, HTTP_CODE_NOT_FOUND, "No host to serve request. Searching for %s", conn->rx->hostHeader);
        conn->host = mprGetFirstItem(server->hosts);
        return;
    }
    mprLog(4, "Select host: \"%s\"", host->name);
    conn->host = host;
}


/*
    Find the matching handler for a request. If any errors occur, the pass handler is used to pass errors onto the 
    net/sendfile connectors to send to the client. This routine may rewrite the request URI and may redirect the request.
 */
void httpRouteRequest(HttpConn *conn)
{
    Http        *http;
    HttpRx      *rx;
    HttpStage   *handler;
    HttpRoute   *route;
    int         next, rewrites, match;

    http = conn->http;
    rx = conn->rx;
    handler = 0;

    if (rx->alias->redirectCode) {
        httpRedirect(conn, rx->alias->redirectCode, rx->alias->uri);
        return;
    }
    for (next = rewrites = 0; !handler && rewrites < HTTP_MAX_REWRITE; ) {
        if ((route = mprGetNextItem(rx->loc->routes, &next)) == 0) {
            break;
        }
        match = httpMatchRoute(conn, route);
        if (match == HTTP_ROUTE_COMPLETE) {
            rx->route = route;
            handler = findHandler(conn);
            mprAssert(handler);
            if (!(handler->flags & HTTP_STAGE_VIRTUAL)) {
                if ((handler = processDirectories(conn, handler)) == 0) {
                    next = 0;
                    rewrites++;
                }
            }
        } else if (match == HTTP_ROUTE_REROUTE) {
            next = 0;
            rewrites++;
        }
    }
    if (!handler || conn->error) {
        handler = http->passHandler;
        if (rewrites >= HTTP_MAX_REWRITE) {
            httpError(conn, HTTP_CODE_INTERNAL_SERVER_ERROR, "Too many request rewrites");
        }
    }
    conn->tx->handler = handler;
}


/*
    Search for a handler by request extension. If that fails, use handler custom matching.
 */
static HttpStage *findHandler(HttpConn *conn)
{
    Http        *http;
    HttpHost    *host;
    HttpRx      *rx;
    HttpTx      *tx;
    HttpStage   *handler, *h;
    HttpLoc     *loc;
    int         next;

    http = conn->http;
    rx = conn->rx;
    tx = conn->tx;
    loc = rx->loc;
    host = httpGetConnHost(conn);

    mprAssert(rx->uri);
    mprAssert(rx->pathInfo);
    mprAssert(host);

    /*
        Check for any explicitly defined handlers (SetHandler directive)
     */
    if ((handler = checkHandler(conn, loc->handler)) == 0) {
        /* 
            Perform custom handler matching first on all defined handlers. Accept the handler if it has a match() 
            routine and it accepts the request.
         */
        for (next = 0; (h = mprGetNextItem(loc->handlers, &next)) != 0; ) {
//  MOB - is this really necessary
            if (h->match && checkHandler(conn, h)) {
                handler = h;
                break;
            }
        }
        if (handler == 0) {
            if (tx->extension) {
                handler = checkHandler(conn, httpGetHandlerByExtension(loc, tx->extension));
#if UNUSED
            } else if (!tx->fileInfo.valid) {
//  MOB -- fix this. Should not be doing this for virtual URIs
                /*
                    URI has no extension, check if the addition of configured  extensions results in a valid filename.
                 */
                for (path = 0, hp = 0; (hp = mprGetNextKey(loc->extensions, hp)) != 0; ) {
                    if (*hp->key && (((HttpStage*)hp->data)->flags & HTTP_STAGE_MISSING_EXT)) {
                        path = sjoin(tx->filename, ".", hp->key, NULL);
                        if (mprGetPathInfo(path, &tx->fileInfo) == 0) {
                            mprLog(5, "findHandler: Adding extension, new path %s\n", path);
                            httpSetUri(conn, sjoin(rx->uri, ".", hp->key, NULL), NULL);
                            handler = (HttpStage*) hp->data;
                            break;
                        }
                    }
                }
#endif
            }
            if (handler == 0) {
                handler = checkHandler(conn, httpGetHandlerByExtension(loc, ""));
                if (handler == 0) {
                    if (!(rx->flags & (HTTP_OPTIONS | HTTP_TRACE))) {
                        httpError(conn, HTTP_CODE_NOT_IMPLEMENTED, "No handler for \"%s\" \"%s\"", rx->method, rx->pathInfo);
                    }
                    handler = http->passHandler;
                }
            }
        }
    }
    mprAssert(handler);
    mprAssert(!tx->fileInfo.checked);
    if (!(handler->flags & HTTP_STAGE_VIRTUAL)) {
        mprGetPathInfo(tx->filename, &tx->fileInfo);
    }
    return handler;
}


/*
    Manage requests to directories. This will either do an external redirect back to the browser or do an internal 
    (transparent) redirection and serve different content back to the browser. This routine may modify the requested 
    URI and/or the request handler.
 */
static HttpStage *processDirectories(HttpConn *conn, HttpStage *handler)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpUri     *prior;
    char        *path, *pathInfo, *uri;

    rx = conn->rx;
    tx = conn->tx;
    prior = rx->parsedUri;

    mprAssert(rx->dir);
    mprAssert(rx->dir->indexName);
    mprAssert(rx->pathInfo);

    if (!tx->fileInfo.checked) {
        mprGetPathInfo(tx->filename, &tx->fileInfo);
    }
    if (!tx->fileInfo.isDir) {
        return handler;
    }
    if (sends(rx->pathInfo, "/")) {
        /*  
            Internal directory redirections
         */
        path = mprJoinPath(tx->filename, rx->dir->indexName);
        if (mprPathExists(path, R_OK)) {
            /*  
                Index file exists, so do an internal redirect to it. Client will not be aware of this happening.
                Return zero so the request will be rematched on return.
             */
            pathInfo = mprJoinPath(rx->pathInfo, rx->dir->indexName);
            uri = httpFormatUri(prior->scheme, prior->host, prior->port, pathInfo, prior->reference, prior->query, 0);
            httpSetUri(conn, uri, 0);
            /* Force a rematch */
            return 0;
        }
    } else {
        /* Must not append the index for the external redirect */
        pathInfo = sjoin(rx->pathInfo, "/", NULL);
        uri = httpFormatUri(prior->scheme, prior->host, prior->port, pathInfo, prior->reference, prior->query, 0);
        httpRedirect(conn, HTTP_CODE_MOVED_PERMANENTLY, uri);
        handler = conn->http->passHandler;
    }
    return handler;
}


static HttpStage *checkHandler(HttpConn *conn, HttpStage *stage)
{
    HttpRx   *rx;

    rx = conn->rx;
    if (stage == 0) {
        return 0;
    }
    if ((stage->flags & HTTP_STAGE_ALL & rx->flags) == 0) {
        return 0;
    }
    if (stage->match && !(stage->flags & HTTP_STAGE_UNLOADED)) {
        /* Can't have match routines on unloaded modules */
        //  MOB - rethink this
        if (!stage->match(conn, stage, HTTP_STAGE_RX | HTTP_STAGE_TX)) {
            return 0;
        }
    }
    return stage;
}


/*
    Get the request extension. Look first at the URI (pathInfo). If no extension, look at the filename.
    Return NULL if no extension.
 */
char *httpGetExtension(HttpConn *conn)
{
    HttpRx  *rx;
    char    *ext;

    rx = conn->rx;
    if ((ext = getExtension(conn, &rx->pathInfo[rx->alias->prefixLen])) == 0) {
        if (conn->tx->filename) {
            ext = getExtension(conn, conn->tx->filename);
        }
    }
    return ext;
}


static char *getExtension(HttpConn *conn, cchar *path)
{
    HttpRx      *rx;
    char        *cp, *ep, *ext;

    rx = conn->rx;

    if (rx && rx->pathInfo) {
        if ((cp = strrchr(path, '.')) != 0) {
            ext = sclone(++cp);
            for (ep = ext; *ep && isalnum((int)*ep); ep++) {
                ;
            }
            *ep = '\0';
            return ext;
        }
    }
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
