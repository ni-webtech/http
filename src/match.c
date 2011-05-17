/*
    match.c -- HTTP pipeline matching.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static HttpStage *checkDirectory(HttpConn *conn, HttpStage *handler);
static char *getExtension(HttpConn *conn, cchar *path);
static HttpStage *checkHandler(HttpConn *conn, HttpStage *stage);
static HttpStage *findHandler(HttpConn *conn);
static bool rewriteRequest(HttpConn *conn);

/*********************************** Code *************************************/

void httpMatchHost(HttpConn *conn)
{ 
    MprSocket       *listenSock;
    HttpServer      *server;
    HttpHost        *host;
    HttpRx          *rx;
    Http            *http;

    http = conn->http;
    listenSock = conn->sock->listenSock;

    server = httpLookupServer(http, listenSock->ip, listenSock->port);
    if (server == 0 || (host = mprGetFirstItem(server->hosts)) == 0) {
        mprError("No host to serve request from %s:%d", listenSock->ip, listenSock->port);
        mprCloseSocket(conn->sock, 0);
        return;
    }
    mprAssert(host);

    if (httpIsNamedVirtualServer(server)) {
        rx = conn->rx;
        if ((host = httpLookupHostByName(server, rx->hostHeader)) == 0) {
            httpSetConnHost(conn, host);
            httpError(conn, HTTP_CODE_NOT_FOUND, "No host to serve request. Searching for %s", rx->hostHeader);
            conn->host = mprGetFirstItem(server->hosts);
            return;
        }
    }
    mprAssert(host);
    mprLog(4, "Select host: \"%s\"", host->name);
    conn->host = host;
}


/*
    Find the matching handler for a request. If any errors occur, the pass handler is used to pass errors onto the 
    net/sendfile connectors to send to the client. This routine may rewrite the request URI and may redirect the request.
 */
void httpMatchHandler(HttpConn *conn)
{
    Http        *http;
    HttpRx      *rx;
    HttpTx      *tx;
    HttpStage   *handler;
    HttpHost    *host;

    http = conn->http;
    rx = conn->rx;
    tx = conn->tx;
    host = conn->host;
    handler = 0;

    mprAssert(rx->pathInfo);
    mprAssert(rx->uri);
    mprAssert(rx->loc);
    mprAssert(rx->alias);
    mprAssert(tx->filename);
    mprAssert(tx->fileInfo.checked);

    for (handler = 0; !handler && !conn->error && (rx->rewrites < HTTP_MAX_REWRITE); rx->rewrites++) {
        if (!rewriteRequest(conn)) {
            handler = findHandler(conn);
        }
    }
#if UNUSED
    if (!handler || conn->error || ((tx->flags & HTTP_TX_NO_BODY) && !(rx->flags & HTTP_HEAD)))
#else
    if (!handler || conn->error) {
#endif
        handler = http->passHandler;
        if (!conn->error && rx->rewrites >= HTTP_MAX_REWRITE) {
            httpError(conn, HTTP_CODE_INTERNAL_SERVER_ERROR, "Too many request rewrites");
        }
    }
    tx->handler = handler;
}


static bool rewriteRequest(HttpConn *conn)
{
    HttpStage       *handler;
    HttpAlias       *alias;
    HttpLoc         *loc;
    HttpRx          *rx;
    HttpTx          *tx;
    MprHash         *he;
    int             next;

    rx = conn->rx;
    tx = conn->tx;
    loc = rx->loc;
    alias = rx->alias;

    mprAssert(rx->alias);
    mprAssert(tx->filename);
    mprAssert(tx->fileInfo.checked);

    if (alias->redirectCode) {
        httpRedirect(conn, alias->redirectCode, alias->uri);
        return 1;
    }
    for (next = 0; (handler = mprGetNextItem(loc->handlers, &next)) != 0; ) {
        if (handler->rewrite && handler->rewrite(conn, handler)) {
            return 1;
        }
    }
    for (he = 0; (he = mprGetNextKey(loc->extensions, he)) != 0; ) {
        handler = (HttpStage*) he->data;
        if (handler->rewrite && handler->rewrite(conn, handler)) {
            return 1;
        }
    }
    return 0;
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
    Get the request extension. Look first at the URI (pathInfo). If no extension, look at the filename.
    Return NULL if no extension.
 */
char *httpGetExtension(HttpConn *conn)
{
    HttpRx  *rx;
    char    *ext;

    rx = conn->rx;
    if ((ext = getExtension(conn, &rx->pathInfo[rx->alias->prefixLen])) == 0) {
        ext = getExtension(conn, conn->tx->filename);
    }
    return ext;
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
    HttpStage   *handler;
    HttpLoc     *loc;
    MprHash     *hp;
    char        *path;
    int         next;

    http = conn->http;
    rx = conn->rx;
    tx = conn->tx;
    loc = rx->loc;
    handler = 0;
    host = httpGetConnHost(conn);

    mprAssert(rx->uri);
    mprAssert(tx->filename);
    mprAssert(tx->fileInfo.checked);
    mprAssert(rx->pathInfo);
    mprAssert(host);

    /*
        Check for any explicitly defined handlers (SetHandler directive)
     */
    if ((handler = checkHandler(conn, loc->handler)) == 0) {
        /* 
            Perform custom handler matching first on all defined handlers 
         */
        for (next = 0; (handler = mprGetNextItem(loc->handlers, &next)) != 0; ) {
            if (checkHandler(conn, handler)) {
                break;
            }
        }
        if (handler == 0) {
            if (tx->extension) {
                handler = checkHandler(conn, httpGetHandlerByExtension(loc, tx->extension));

            } else if (!tx->fileInfo.valid) {
                /*
                    URI has no extension, check if the addition of configured  extensions results in a valid filename.
                 */
                for (path = 0, hp = 0; (hp = mprGetNextKey(loc->extensions, hp)) != 0; ) {
                    handler = (HttpStage*) hp->data;
                    if (*hp->key && (handler->flags & HTTP_STAGE_MISSING_EXT)) {
                        path = sjoin(tx->filename, ".", hp->key, NULL);
                        if (mprGetPathInfo(path, &tx->fileInfo) == 0) {
                            mprLog(5, "findHandler: Adding extension, new path %s\n", path);
                            httpSetUri(conn, sjoin(rx->uri, ".", hp->key, NULL), NULL);
                            break;
                        }
                    }
                }
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
    return checkDirectory(conn, handler);
}


/*
    Manage requests to directories. This will either do an external redirect back to the browser or do an internal 
    (transparent) redirection and serve different content back to the browser. This routine may modify the requested 
    URI and/or the request handler.
 */
static HttpStage *checkDirectory(HttpConn *conn, HttpStage *handler)
{
    HttpRx      *rx;
    HttpTx      *tx;
    MprPath     *info;
    HttpHost    *host;
    HttpUri     *prior;
    char        *path, *pathInfo, *uri;

    rx = conn->rx;
    tx = conn->tx;
    host = conn->host;
    info = &tx->fileInfo;
    prior = rx->parsedUri;

    mprAssert(rx->dir);
    mprAssert(rx->dir->indexName);
    mprAssert(rx->pathInfo);

    if (!tx->fileInfo.isDir || handler == 0 || handler->flags & HTTP_STAGE_VIRTUAL) {
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
        if (!stage->match(conn, stage)) {
            return 0;
        }
    }
    return stage;
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
