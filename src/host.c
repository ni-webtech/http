/*
    host.c -- Host class for all HTTP hosts

    The Host class is used for the default HTTP server and for all virtual hosts (including SSL hosts).
    Many objects are controlled at the host level. Eg. URL handlers.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

static void manageHost(HttpHost *host, int flags);

/*********************************** Code *************************************/

static void manageHost(HttpHost *host, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(host->name);
        mprMark(host->ip);
        mprMark(host->parent);
        mprMark(host->aliases);
        mprMark(host->dirs);
        mprMark(host->locations);
        mprMark(host->loc);
        mprMark(host->mimeTypes);
        mprMark(host->documentRoot);
        mprMark(host->serverRoot);
        mprMark(host->traceInclude);
        mprMark(host->traceExclude);
        mprMark(host->protocol);
        mprMark(host->mutex);
        mprMark(host->log);
        mprMark(host->logFormat);
        mprMark(host->logPath);
        mprMark(host->limits);

    } else if (flags & MPR_MANAGE_FREE) {
        httpRemoveHost(MPR->httpService, host);
    }
}


HttpHost *httpCreateHost(HttpLoc *loc)
{
    HttpHost    *host;
    Http        *http;

    http = MPR->httpService;
    if ((host = mprAllocObj(HttpHost, manageHost)) == 0) {
        return 0;
    }
    host->mutex = mprCreateLock();
    host->aliases = mprCreateList(-1, 0);
    host->dirs = mprCreateList(-1, 0);
    host->locations = mprCreateList(-1, 0);
    host->limits = mprMemdup(http->serverLimits, sizeof(HttpLimits));
    host->flags = HTTP_HOST_NO_TRACE;
    host->protocol = sclone("HTTP/1.1");
    host->mimeTypes = MPR->mimeTypes;
    host->documentRoot = host->serverRoot = sclone(".");

    host->traceMask = HTTP_TRACE_TX | HTTP_TRACE_RX | HTTP_TRACE_FIRST | HTTP_TRACE_HEADER;
    host->traceLevel = 3;
    host->traceMaxLength = MAXINT;

    host->loc = (loc) ? loc : httpCreateLocation();
    httpAddLocation(host, host->loc);
    host->loc->auth = httpCreateAuth(host->loc->auth);
    httpAddDir(host, httpCreateBareDir("."));
    httpAddHost(http, host);
    return host;
}


HttpHost *httpCloneHost(HttpHost *parent)
{
    HttpHost    *host;
    Http        *http;

    http = MPR->httpService;

    if ((host = mprAllocObj(HttpHost, manageHost)) == 0) {
        return 0;
    }
    host->mutex = mprCreateLock();

    /*  
        The aliases, dirs and locations are all copy-on-write
     */
    host->parent = parent;
    host->aliases = parent->aliases;
    host->dirs = parent->dirs;
    host->locations = parent->locations;
    host->flags = parent->flags | HTTP_HOST_VHOST;
    host->protocol = parent->protocol;
    host->mimeTypes = parent->mimeTypes;
    host->limits = mprMemdup(parent->limits, sizeof(HttpLimits));
    host->documentRoot = parent->documentRoot;
    host->serverRoot = parent->serverRoot;
    host->loc = httpCreateInheritedLocation(parent->loc);
    httpSetLocationHost(host->loc, host);
    host->traceMask = parent->traceMask;
    host->traceLevel = parent->traceLevel;
    host->traceMaxLength = parent->traceMaxLength;
    if (parent->traceInclude) {
        host->traceInclude = mprCloneHash(parent->traceInclude);
    }
    if (parent->traceExclude) {
        host->traceExclude = mprCloneHash(parent->traceExclude);
    }
    httpAddLocation(host, host->loc);
    httpAddHost(http, host);
    return host;
}


void httpSetHostDocumentRoot(HttpHost *host, cchar *dir)
{
    char    *doc;
    ssize   len;

    doc = host->documentRoot = httpMakePath(host->loc, dir);
    len = slen(doc);
    if (doc[len - 1] == '/') {
        doc[len - 1] = '\0';
    }
    /*  Create a catch-all alias */
    httpAddAlias(host, httpCreateAlias("", doc, 0));
}


void httpSetHostLogRotation(HttpHost *host, int logCount, int logSize)
{
    host->logCount = logCount;
    host->logSize = logSize;
}


void httpSetHostServerRoot(HttpHost *host, cchar *serverRoot)
{
    host->serverRoot = mprGetAbsPath(serverRoot);
}


/*
    Set the host name intelligently from the name specified by ip:port. Port may be set to -1 and ip may contain a port
    specifier, ie. "address:port". This routines sets host->name and host->ip, host->port which is used for vhost matching.
 */
void httpSetHostName(HttpHost *host, cchar *ip, int port)
{
    if (port < 0 && schr(ip, ':')) {
        char *pip;
        mprParseIp(ip, &pip, &port, -1);
        ip = pip;
    }
    if (ip) {
        if (port > 0) {
            host->name = mprAsprintf("%s:%d", ip, port);
        } else {
            host->name = sclone(ip);
        }
    } else {
        mprAssert(port > 0);
        host->name = mprAsprintf("*:%d", port);
    }
    if (scmp(ip, "default") != 0) {
        host->ip = sclone(ip);
        host->port = port;
    }
}


void httpSetHostProtocol(HttpHost *host, cchar *protocol)
{
    host->protocol = sclone(protocol);
}


int httpAddAlias(HttpHost *host, HttpAlias *newAlias)
{
    HttpAlias   *alias;
    int         next, rc;

    if (host->parent && host->aliases == host->parent->aliases) {
        host->aliases = mprCloneList(host->parent->aliases);
    }

    /*  
        Sort in reverse order. Make sure that /abc/def sorts before /abc. But we sort redirects with status codes first.
     */
    for (next = 0; (alias = mprGetNextItem(host->aliases, &next)) != 0; ) {
        rc = strcmp(newAlias->prefix, alias->prefix);
        if (rc == 0) {
            if (newAlias->redirectCode > alias->redirectCode) {
                mprInsertItemAtPos(host->aliases, next - 1, newAlias);

            } else if (newAlias->redirectCode == alias->redirectCode) {
                mprRemoveItem(host->aliases, alias);
                mprInsertItemAtPos(host->aliases, next - 1, newAlias);
            }
            return 0;
            
        } else if (rc > 0) {
            mprInsertItemAtPos(host->aliases, next - 1, newAlias);
            return 0;
        }
    }
    mprAddItem(host->aliases, newAlias);
    return 0;
}


int httpAddDir(HttpHost *host, HttpDir *dir)
{
    HttpDir     *dp;
    int         next, rc;

    mprAssert(dir);
    mprAssert(dir->path);
    
    if (host->parent && host->dirs == host->parent->dirs) {
        host->dirs = mprCloneList(host->parent->dirs);
    }

    /*
        Sort in reverse collating sequence. Must make sure that /abc/def sorts before /abc
     */
    for (next = 0; (dp = mprGetNextItem(host->dirs, &next)) != 0; ) {
        mprAssert(dp->path);
        rc = strcmp(dir->path, dp->path);
        if (rc == 0) {
            mprRemoveItem(host->dirs, dir);
            mprInsertItemAtPos(host->dirs, next - 1, dir);
            return 0;

        } else if (rc > 0) {
            mprInsertItemAtPos(host->dirs, next - 1, dir);
            return 0;
        }
    }
    mprAddItem(host->dirs, dir);
    return 0;
}


int httpAddLocation(HttpHost *host, HttpLoc *loc)
{
    HttpLoc     *lp;
    int         next, rc;

    mprAssert(loc);
    mprAssert(loc->prefix);
    
    if (host->parent && host->locations == host->parent->locations) {
        host->locations = mprCloneList(host->parent->locations);
    }

    /*
        Sort in reverse collating sequence. Must make sure that /abc/def sorts before /abc
     */
    for (next = 0; (lp = mprGetNextItem(host->locations, &next)) != 0; ) {
        rc = strcmp(loc->prefix, lp->prefix);
        if (rc == 0) {
            mprRemoveItem(host->locations, lp);
            mprInsertItemAtPos(host->locations, next - 1, loc);
            return 0;
        }
        if (strcmp(loc->prefix, lp->prefix) > 0) {
            mprInsertItemAtPos(host->locations, next - 1, loc);
            return 0;
        }
    }
    mprAddItem(host->locations, loc);
    httpSetLocationHost(loc, host);
    return 0;
}


//  MOB - should be named GetBestAlias
HttpAlias *httpGetAlias(HttpHost *host, cchar *uri)
{
    HttpAlias     *alias;
    int           next;

    if (uri) {
        for (next = 0; (alias = mprGetNextItem(host->aliases, &next)) != 0; ) {
            if (strncmp(alias->prefix, uri, alias->prefixLen) == 0) {
                return alias;
            }
        }
    }
    /*
        Must always return an alias. The last is the catch-all.
     */
    return mprGetLastItem(host->aliases);
}


HttpAlias *httpLookupAlias(HttpHost *host, cchar *prefix)
{
    HttpAlias   *alias;
    int         next;

    for (next = 0; (alias = mprGetNextItem(host->aliases, &next)) != 0; ) {
        if (strcmp(alias->prefix, prefix) == 0) {
            return alias;
        }
    }
    return 0;
}


HttpDir *httpLookupDir(HttpHost *host, cchar *pathArg)
{
    HttpDir     *dir;
    char        *path, *tmpPath;
    int         next;

    if (!mprIsAbsPath(pathArg)) {
        path = tmpPath = mprGetAbsPath(pathArg);
    } else {
        path = (char*) pathArg;
        tmpPath = 0;
    }
    for (next = 0; (dir = mprGetNextItem(host->dirs, &next)) != 0; ) {
        mprAssert(slen(dir->path) == 0 || dir->path[slen(dir->path) - 1] != '/');
        if (dir->path != 0) {
            if (mprSamePath(dir->path, path)) {
                return dir;
            }
        }
    }
    return 0;
}


/*  
    Find the directory entry that this file (path) resides in. path is a physical file path. We find the most specific
    (longest) directory that matches. The directory must match or be a parent of path. Not called with raw files names.
    They will be lower case and only have forward slashes. For windows, the will be in cannonical format with drive
    specifiers.
 */
HttpDir *httpLookupBestDir(HttpHost *host, cchar *path)
{
    HttpDir     *dir;
    ssize       dlen;
    int         next;

    for (next = 0; (dir = mprGetNextItem(host->dirs, &next)) != 0; ) {
        dlen = dir->pathLen;
        mprAssert(dlen == 0 || dir->path[dlen - 1] != '/');
        if (mprSamePathCount(dir->path, path, dlen)) {
            if (dlen >= 0) {
                return dir;
            }
        }
    }
    return 0;
}


HttpLoc *httpLookupLocation(HttpHost *host, cchar *prefix)
{
    HttpLoc     *loc;
    int         next;

    mprAssert(host);

    for (next = 0; (loc = mprGetNextItem(host->locations, &next)) != 0; ) {
        if (strcmp(prefix, loc->prefix) == 0) {
            return loc;
        }
    }
    return 0;
}


HttpLoc *httpLookupBestLocation(HttpHost *host, cchar *uri)
{
    HttpLoc     *loc;
    int         next, rc;

    if (uri) {
        mprAssert(host);
        for (next = 0; (loc = mprGetNextItem(host->locations, &next)) != 0; ) {
            rc = sncmp(loc->prefix, uri, loc->prefixLen);
            if (rc == 0) {
                return loc;
            }
        }
    }
    return mprGetLastItem(host->locations);
}


void httpSetHostTrace(HttpHost *host, int level, int mask)
{
    host->traceMask = mask;
    host->traceLevel = level;
}


void httpSetHostTraceFilter(HttpHost *host, ssize len, cchar *include, cchar *exclude)
{
    char    *word, *tok, *line;

    host->traceMaxLength = (int) len;

    if (include && strcmp(include, "*") != 0) {
        host->traceInclude = mprCreateHash(0, 0);
        line = sclone(include);
        word = stok(line, ", \t\r\n", &tok);
        while (word) {
            if (word[0] == '*' && word[1] == '.') {
                word += 2;
            }
            mprAddKey(host->traceInclude, word, host);
            word = stok(NULL, ", \t\r\n", &tok);
        }
    }
    if (exclude) {
        host->traceExclude = mprCreateHash(0, 0);
        line = sclone(exclude);
        word = stok(line, ", \t\r\n", &tok);
        while (word) {
            if (word[0] == '*' && word[1] == '.') {
                word += 2;
            }
            mprAddKey(host->traceExclude, word, host);
            word = stok(NULL, ", \t\r\n", &tok);
        }
    }
}


int httpSetupTrace(HttpHost *host, cchar *ext)
{
    if (ext) {
        if (host->traceInclude && !mprLookupKey(host->traceInclude, ext)) {
            return 0;
        }
        if (host->traceExclude && mprLookupKey(host->traceExclude, ext)) {
            return 0;
        }
    }
    return host->traceMask;
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
