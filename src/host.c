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
        mprMark(host->parent);
        mprMark(host->aliases);
        mprMark(host->dirs);
        mprMark(host->locations);
        mprMark(host->loc);
        mprMark(host->mimeTypes);
        mprMark(host->documentRoot);
        mprMark(host->serverRoot);
        mprMark(host->ip);
        mprMark(host->traceInclude);
        mprMark(host->traceExclude);
        mprMark(host->protocol);
        mprMark(host->mutex);
        mprMark(host->log);
        mprMark(host->logFormat);
        mprMark(host->logPath);
        mprMark(host->limits);
    }
}


HttpHost *httpCreateHost(cchar *ip, int port, HttpLoc *loc)
{
    HttpHost    *host;
    Http        *http;

    http = MPR->httpService;

    if ((host = mprAllocObj(HttpHost, manageHost)) == 0) {
        return 0;
    }
    host->name = mprAsprintf("%s:%d", ip ? ip : "*", port);
    host->mutex = mprCreateLock();
    host->aliases = mprCreateList(-1, 0);
    host->dirs = mprCreateList(-1, 0);
    host->locations = mprCreateList(-1, 0);
    host->limits = mprMemdup(http->serverLimits, sizeof(HttpLimits));
    host->ip = sclone(ip);
    host->port = port;
    host->flags = HTTP_HOST_NO_TRACE;
    host->protocol = sclone("HTTP/1.1");
    host->mimeTypes = MPR->mimeTypes;
    host->documentRoot = host->serverRoot = sclone(".");

    //  MOB -- not right
    host->traceMask = HTTP_TRACE_TX | HTTP_TRACE_RX | HTTP_TRACE_FIRST | HTTP_TRACE_HEADER;
    host->traceLevel = 3;
    host->traceMaxLength = INT_MAX;

    host->loc = (loc) ? loc : httpCreateLocation(http);
    httpAddLocation(host, host->loc);
    host->loc->auth = httpCreateAuth(host->loc->auth);
    httpAddHost(http, host);
    return host;
}


/*  
    Create a new virtual host and inherit settings from another host
 */
HttpHost *httpCreateVirtualHost(cchar *ip, int port, HttpHost *parent)
{
    HttpHost    *host;
    Http        *http;

    http = MPR->httpService;

    if ((host = mprAllocObj(HttpHost, manageHost)) == 0) {
        return 0;
    }
    host->mutex = mprCreateLock();
    host->parent = parent;

    /*  
        The aliases, dirs and locations are all copy-on-write
     */
    host->aliases = parent->aliases;
    host->dirs = parent->dirs;
    host->locations = parent->locations;
    host->flags = parent->flags | HTTP_HOST_VHOST;
    host->protocol = parent->protocol;
    host->mimeTypes = parent->mimeTypes;
    host->ip = parent->ip;
    host->port = parent->port;
    host->limits = mprMemdup(parent->limits, sizeof(HttpLimits));
    if (ip) {
        host->ip = sclone(ip);
    }
    if (port) {
        host->port = port;
    }
    host->loc = httpCreateInheritedLocation(parent->loc);
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
    int     len;

    doc = host->documentRoot = httpMakePath(host, dir);
    len = (int) strlen(doc);
    if (doc[len - 1] == '/') {
        doc[len - 1] = '\0';
    }
    /*  Create a catch-all alias */
    httpAddAlias(host, httpCreateAlias("", doc, 0));
}


void httpSetHostServerRoot(HttpHost *host, cchar *serverRoot)
{
    host->serverRoot = sclone(serverRoot);
}


void httpSetHostName(HttpHost *host, cchar *name)
{
    host->name = sclone(name);
}


void httpSetHostAddress(HttpHost *host, cchar *ip, int port)
{
    host->ip = sclone(ip);
    if (port > 0) {
        host->port = port;
    }
}


void httpSetHostProtocol(HttpHost *host, cchar *protocol)
{
    host->protocol = sclone(protocol);
}


int httpAddAlias(HttpHost *host, HttpAlias *newAlias)
{
    HttpAlias     *alias;
    int         rc, next;

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
    int         rc, next;

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


int httpAddLocation(HttpHost *host, HttpLoc *newLocation)
{
    HttpLoc     *loc;
    int         next, rc;

    mprAssert(newLocation);
    mprAssert(newLocation->prefix);
    
    if (host->parent && host->locations == host->parent->locations) {
        host->locations = mprCloneList(host->parent->locations);
    }

    /*
        Sort in reverse collating sequence. Must make sure that /abc/def sorts before /abc
     */
    for (next = 0; (loc = mprGetNextItem(host->locations, &next)) != 0; ) {
        rc = strcmp(newLocation->prefix, loc->prefix);
        if (rc == 0) {
            mprRemoveItem(host->locations, loc);
            mprInsertItemAtPos(host->locations, next - 1, newLocation);
            return 0;
        }
        if (strcmp(newLocation->prefix, loc->prefix) > 0) {
            mprInsertItemAtPos(host->locations, next - 1, newLocation);
            return 0;
        }
    }
    mprAddItem(host->locations, newLocation);
    return 0;
}


HttpAlias *httpGetAlias(HttpHost *host, cchar *uri)
{
    HttpAlias     *alias;
    int         next;

    if (uri) {
        for (next = 0; (alias = mprGetNextItem(host->aliases, &next)) != 0; ) {
            if (strncmp(alias->prefix, uri, alias->prefixLen) == 0) {
                if (uri[alias->prefixLen] == '\0' || uri[alias->prefixLen] == '/') {
                    return alias;
                }
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
    HttpDir       *dir;
    char        *path, *tmpPath;
    int         next, len;

    if (!mprIsAbsPath(pathArg)) {
        path = tmpPath = mprGetAbsPath(pathArg);
    } else {
        path = (char*) pathArg;
        tmpPath = 0;
    }
    len = (int) strlen(path);

    for (next = 0; (dir = mprGetNextItem(host->dirs, &next)) != 0; ) {
        mprAssert(strlen(dir->path) == 0 || dir->path[strlen(dir->path) - 1] != '/');
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
    HttpDir *dir;
    int     next, len, dlen;

    len = (int) strlen(path);

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
        for (next = 0; (loc = mprGetNextItem(host->locations, &next)) != 0; ) {
            rc = sncmp(loc->prefix, uri, loc->prefixLen);
            if (rc == 0 && uri[loc->prefixLen] == '/') {
                return loc;
            }
        }
    }
    return mprGetLastItem(host->locations);
}


//  MOB -- order this file

void httpSetHostTrace(HttpHost *host, int level, int mask)
{
    host->traceMask = mask;
    host->traceLevel = level;
}


void httpSetHostTraceFilter(HttpHost *host, int len, cchar *include, cchar *exclude)
{
    char    *word, *tok, *line;

    host->traceMaxLength = len;

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
        if (host->traceInclude && !mprLookupHash(host->traceInclude, ext)) {
            return 0;
        }
        if (host->traceExclude && mprLookupHash(host->traceExclude, ext)) {
            return 0;
        }
    }
    return host->traceMask;
}


/*
    Make a path name. This replaces $references, converts to an absolute path name, cleans the path and maps delimiters.
 */
char *httpMakePath(HttpHost *host, cchar *file)
{
    char    *result, *path;

    mprAssert(file);

    if ((path = httpReplaceReferences(host, file)) == 0) {
        /*  Overflow */
        return 0;
    }
    if (*path == '\0' || strcmp(path, ".") == 0) {
        result = sclone(host->serverRoot);

#if BLD_WIN_LIKE
    } else if (*path != '/' && path[1] != ':' && path[2] != '/') {
        result = mprJoinPath(host->serverRoot, path);
#elif VXWORKS
    } else if (strchr((path), ':') == 0 && *path != '/') {
        result = mprJoinPath(host->serverRoot, path);
#else
    } else if (*path != '/') {
        result = mprJoinPath(host->serverRoot, path);
#endif
    } else {
        result = mprGetAbsPath(path);
    }
    return result;
}


static int matchRef(cchar *key, char **src)
{
    int     len;

    mprAssert(src);
    mprAssert(key && *key);

    len = strlen(key);
    if (strncmp(*src, key, len) == 0) {
        *src += len;
        return 1;
    }
    return 0;
}


/*
    Replace a limited set of $VAR references. Currently support DOCUMENT_ROOT, SERVER_ROOT and PRODUCT
    TODO - Expand and formalize this. Should support many more variables.
 */
//  MOB - rename
char *httpReplaceReferences(HttpHost *host, cchar *str)
{
    MprBuf  *buf;
    char    *src;
    char    *result;

    buf = mprCreateBuf(0, 0);
    if (str) {
        for (src = (char*) str; *src; ) {
            if (*src == '$') {
                ++src;
                if (matchRef("DOCUMENT_ROOT", &src)) {
                    mprPutStringToBuf(buf, host->documentRoot);
                    continue;

                } else if (matchRef("SERVER_ROOT", &src)) {
                    mprPutStringToBuf(buf, host->serverRoot);
                    continue;

                } else if (matchRef("PRODUCT", &src)) {
                    mprPutStringToBuf(buf, BLD_PRODUCT);
                    continue;
                }
            }
            mprPutCharToBuf(buf, *src++);
        }
    }
    mprAddNullToBuf(buf);
    result = sclone(mprGetBufStart(buf));
    return result;
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
