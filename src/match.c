/*
    match.c -- HTTP pipeline matching.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static char *addIndexToUrl(HttpConn *conn, cchar *index);
static char *getExtension(HttpConn *conn, cchar *path);
static HttpStage *checkHandler(HttpConn *conn, HttpStage *stage);
static HttpStage *findHandler(HttpConn *conn);
static HttpStage *findLocationHandler(HttpConn *conn);
static HttpStage *mapToFile(HttpConn *conn, HttpStage *handler);
static HttpStage *processDirectory(HttpConn *conn, HttpStage *handler);
static bool rewriteRequest(HttpConn *conn);
static void setScriptName(HttpConn *conn);

/*********************************** Code *************************************/
/*
    Match a request to a Host. This is an initial match which may be revised if the request includes a Host header.
    This is typically invoked from the server state change notifier on transition to the PARSED state.
 */
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
        httpProtocolError(conn, HTTP_CODE_INTERNAL_SERVER_ERROR, 
            "No host to serve request from %s:%d", listenSock->ip, listenSock->port);
        return;
    }
    mprAssert(host);

    if (httpIsNamedVirtualServer(server)) {
        rx = conn->rx;
        if ((host = httpLookupHostByName(server, rx->hostName)) == 0) {
            httpSetConnHost(conn, host);
            httpError(conn, HTTP_CODE_NOT_FOUND, "No host to serve request. Searching for %s", rx->hostName);
            return;
        }
    }
    mprAssert(host);
    mprLog(3, "Select host: \"%s\"", host->name);
    conn->host = host;
#if UNUSED
    //  MOB - who does this?
    if (mprGetLogLevel(conn) >= host->traceLevel) {
        conn->traceLevel = host->traceLevel;
        conn->traceMaxLength = host->traceMaxLength;
        conn->traceMask = host->traceMask;
        conn->traceInclude = host->traceInclude;
        conn->traceExclude = host->traceExclude;
    }            
#endif
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

    mprAssert(rx->uri);
    mprAssert(rx->loc);
    mprAssert(rx->alias);
    mprAssert(tx->filename);
    mprAssert(tx->fileInfo.checked);

    /*
        Get the best (innermost) location block and see if a handler is explicitly set for that location block.
        Possibly rewrite the url and retry.
     */
    while (!handler && !conn->error && rx->rewrites++ < HTTP_MAX_REWRITE) {
        /*
            Give stages a cance to rewrite the request, then match the location handler. If that doesn't match,
            try to match by extension and/or handler match() routines. This may invoke processDirectory which
            may redirect and thus require reprocessing -- hence the loop.
        */
        if (!rewriteRequest(conn)) {
            if ((handler = findLocationHandler(conn)) == 0) {
                handler = findHandler(conn);
            }
            handler = mapToFile(conn, handler);
        }
    }
    if (handler == 0 || conn->error || ((tx->flags & HTTP_TX_NO_BODY) && !(rx->flags & HTTP_HEAD))) {
        handler = http->passHandler;
        if (!conn->error && rx->rewrites >= HTTP_MAX_REWRITE) {
            httpError(conn, HTTP_CODE_INTERNAL_SERVER_ERROR, "Too many request rewrites");
        }
    }
    if (handler->flags & HTTP_STAGE_PATH_INFO) {
        setScriptName(conn);
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
    for (he = 0; (he = mprGetNextHash(loc->extensions, he)) != 0; ) {
        handler = (HttpStage*) he->data;
        if (handler->rewrite && handler->rewrite(conn, handler)) {
            return 1;
        }
    }
    return 0;
}


static HttpStage *findLocationHandler(HttpConn *conn)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpLoc     *loc;
    HttpHost    *host;

    rx = conn->rx;
    tx = conn->tx;
    host = httpGetConnHost(conn);
    loc = rx->loc = httpLookupBestLocation(host, rx->pathInfo);
    rx->auth = loc->auth;
    mprAssert(loc);
    return checkHandler(conn, loc->handler);
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

    mprAssert(rx->uri);
    mprAssert(tx->filename);
    mprAssert(tx->fileInfo.checked);
    mprAssert(rx->pathInfo);

    if (rx->pathInfo == 0) {
        handler = http->passHandler;

    } else if (tx->extension) {
        handler = checkHandler(conn, httpGetHandlerByExtension(loc, tx->extension));

    } else if (!tx->fileInfo.valid) {
        /*
            URI has no extension, check if the addition of configured  extensions results in a valid filename.
         */
        for (path = 0, hp = 0; (hp = mprGetNextHash(loc->extensions, hp)) != 0; ) {
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
        if (hp == 0) {
            handler = 0;
        }
    }
    if (handler == 0) {
        /* Failed to match by extension, so perform custom handler matching on all defined handlers */
        for (next = 0; (handler = mprGetNextItem(loc->handlers, &next)) != 0; ) {
            if (checkHandler(conn, handler)) {
                break;
            }
        }
    }
    if (handler == 0) {
        handler = checkHandler(conn, httpGetHandlerByExtension(loc, ""));
        if (handler == 0) {
            handler = http->passHandler;
            if (!(rx->flags & (HTTP_OPTIONS | HTTP_TRACE))) {
                httpProtocolError(conn, HTTP_CODE_NOT_IMPLEMENTED, "No handler to service method \"%s\" for request \"%s\"", 
                    rx->method, rx->pathInfo);
            }
        }
    }
    return handler;
}


static HttpStage *mapToFile(HttpConn *conn, HttpStage *handler)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpHost    *host;
    MprPath     *info, ginfo;
    char        *gfile;

    rx = conn->rx;
    tx = conn->tx;
    host = conn->host;
    info = &tx->fileInfo;

    mprAssert(handler);
    mprAssert(tx->filename);
    mprAssert(info->checked);

    if (!handler || (handler->flags & HTTP_STAGE_VIRTUAL)) {
        return handler;
    }
    if ((rx->dir = httpLookupBestDir(host, tx->filename)) == 0) {
        httpError(conn, HTTP_CODE_NOT_FOUND, "Missing directory block for %s", tx->filename);
    } else {
        rx->auth = rx->dir->auth;
        if (info->isDir) {
            handler = processDirectory(conn, handler);

        } else if (!info->valid) {
            /*
                File not found. See if a compressed variant exists.
             */
            if (rx->acceptEncoding && strstr(rx->acceptEncoding, "gzip") != 0) {
                gfile = mprAsprintf("%s.gz", tx->filename);
                if (mprGetPathInfo(gfile, &ginfo) == 0) {
                    tx->filename = gfile;
                    tx->fileInfo = ginfo;
                    tx->etag = mprAsprintf("\"%x-%Lx-%Lx\"", ginfo.inode, ginfo.size, ginfo.mtime);
                    httpSetHeader(conn, "Content-Encoding", "gzip");
                    return handler;
                }
            }
            if (!(rx->flags & HTTP_PUT) && (handler->flags & HTTP_STAGE_VERIFY_ENTITY) && 
                    (rx->auth == 0 || rx->auth->type == 0)) {
                httpError(conn, HTTP_CODE_NOT_FOUND, "Can't open document: %s", tx->filename);
            }
        }
    }
    return handler;
}


/*
    Manage requests to directories. This will either do an external redirect back to the browser or do an internal 
    (transparent) redirection and serve different content back to the browser. This routine may modify the requested 
    URI and/or the request handler.
 */
static HttpStage *processDirectory(HttpConn *conn, HttpStage *handler)
{
    HttpRx      *rx;
    HttpTx      *tx;
    MprPath     *info;
    HttpHost    *host;
    char        *path, *index;

    rx = conn->rx;
    tx = conn->tx;
    host = conn->host;
    info = &tx->fileInfo;

    mprAssert(rx->dir);
    mprAssert(rx->pathInfo);
    mprAssert(info->isDir);

    index = rx->dir->indexName;
    if (rx->pathInfo[slen(rx->pathInfo) - 1] == '/') {
        /*  
            Internal directory redirections
         */
        path = mprJoinPath(tx->filename, index);
        if (mprPathExists(path, R_OK)) {
            /*  
                Index file exists, so do an internal redirect to it. Client will not be aware of this happening.
                Return zero so the request will be rematched on return.
             */
            httpSetUri(conn, addIndexToUrl(conn, index), NULL);
            return 0;
        }
    } else {
        /*  
            External redirect. Ask the client to re-issue a request for a new location. See if an index exists and if so, 
            construct a new location for the index. If the index can't be accessed, append a "/" to the URI and redirect.
         */
        if (rx->parsedUri->query && rx->parsedUri->query[0]) {
            path = mprAsprintf("%s/%s?%s", rx->pathInfo, index, rx->parsedUri->query);
        } else {
            path = mprJoinPath(rx->pathInfo, index);
        }
        if (!mprPathExists(path, R_OK)) {
            path = sjoin(rx->pathInfo, "/", NULL);
        }
        httpRedirect(conn, HTTP_CODE_MOVED_PERMANENTLY, path);
        handler = conn->http->passHandler;
    }
    return handler;
}


#if UNUSED
static bool fileExists(cchar *path) {
    if (mprPathExists(path, R_OK)) {
        return 1;
    }
#if BLD_WIN_LIKE
{
    char    *file;
    file = sjoin(path, ".exe", NULL);
    if (mprPathExists(file, R_OK)) {
        return 1;
    }
    file = sjoin(path, ".bat", NULL);
    if (mprPathExists(file, R_OK)) {
        return 1;
    }
}
#endif
    return 0;
}
#endif


static void setScriptName(HttpConn *conn)
{
    HttpAlias   *alias;
    HttpRx      *rx;
    HttpTx      *tx;
    char        *cp, *extraPath, *start, *pathInfo;
    ssize       scriptLen;

    rx = conn->rx;
    tx = conn->tx;
    alias = rx->alias;

    /*
        Find the script name in the filename. This is assumed to be either:
        - The original filename up to and including first portion containing a "."
        - The entire original filename
        Once found, set the scriptName and trim the extraPath from both the filename and the pathInfo
     */
    start = &tx->filename[strlen(alias->filename)];
    if ((cp = strchr(start, '.')) != 0 && (extraPath = strchr(cp, '/')) != 0) {
        rx->scriptName = sclone(rx->pathInfo);
        scriptLen = alias->prefixLen + extraPath - start;
        rx->pathInfo = sclone(&rx->scriptName[scriptLen]);
        rx->scriptName[0] = '\0';
        *extraPath = '\0';
        httpMapToStorage(conn);
        if (rx->pathInfo[0]) {
            rx->pathTranslated = httpMakeFilename(conn, alias, rx->pathInfo, 0);
        }
    } else {
        pathInfo = rx->pathInfo;
        rx->scriptName = rx->pathInfo;
        rx->pathInfo = 0;
    }
}


static char *addIndexToUrl(HttpConn *conn, cchar *index)
{
    HttpRx      *rx;
    char        *path;

    rx = conn->rx;
    path = mprJoinPath(rx->pathInfo, index);
    if (rx->parsedUri->query && rx->parsedUri->query[0]) {
        return srejoin(path, "?", rx->parsedUri->query, NULL);
    }
    return path;
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


char *httpMakeFilename(HttpConn *conn, HttpAlias *alias, cchar *url, bool skipAliasPrefix)
{
    cchar   *seps;
    char    *path;
    int     len;

    mprAssert(alias);
    mprAssert(url);

    if (skipAliasPrefix) {
        url += alias->prefixLen;
    }
    while (*url == '/') {
        url++;
    }
    len = (int) strlen(alias->filename);
    if ((path = mprAlloc(len + strlen(url) + 2)) == 0) {
        return 0;
    }
    strcpy(path, alias->filename);
    if (*url) {
        seps = mprGetPathSeparators(path);
        path[len++] = seps[0];
        strcpy(&path[len], url);
    }
    return mprGetNativePath(path);
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
