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

HttpHost *httpCreateHost()
{
    HttpHost    *host;
    Http        *http;

    http = MPR->httpService;
    if ((host = mprAllocObj(HttpHost, manageHost)) == 0) {
        return 0;
    }
    host->mutex = mprCreateLock();
    host->dirs = mprCreateList(-1, 0);
    host->routes = mprCreateList(-1, 0);
    host->limits = mprMemdup(http->serverLimits, sizeof(HttpLimits));
    host->flags = HTTP_HOST_NO_TRACE;
    host->protocol = sclone("HTTP/1.1");
    host->mimeTypes = MPR->mimeTypes;
    host->home = sclone(".");

    host->traceMask = HTTP_TRACE_TX | HTTP_TRACE_RX | HTTP_TRACE_FIRST | HTTP_TRACE_HEADER;
    host->traceLevel = 3;
    host->traceMaxLength = MAXINT;
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
        The dirs and routes are all copy-on-write.
        Don't clone ip, port and name
     */
    host->parent = parent;
    host->dirs = parent->dirs;
    host->routes = parent->routes;
    host->flags = parent->flags | HTTP_HOST_VHOST;
    host->protocol = parent->protocol;
    host->mimeTypes = parent->mimeTypes;
    host->limits = mprMemdup(parent->limits, sizeof(HttpLimits));
    host->home = parent->home;
    host->traceMask = parent->traceMask;
    host->traceLevel = parent->traceLevel;
    host->traceMaxLength = parent->traceMaxLength;
    if (parent->traceInclude) {
        host->traceInclude = mprCloneHash(parent->traceInclude);
    }
    if (parent->traceExclude) {
        host->traceExclude = mprCloneHash(parent->traceExclude);
    }
    httpAddHost(http, host);
    return host;
}


static void manageHost(HttpHost *host, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(host->name);
        mprMark(host->ip);
        mprMark(host->parent);
        mprMark(host->dirs);
        mprMark(host->routes);
        mprMark(host->mimeTypes);
        mprMark(host->home);
        mprMark(host->traceInclude);
        mprMark(host->traceExclude);
        mprMark(host->protocol);
        mprMark(host->mutex);
        mprMark(host->log);
        mprMark(host->logFormat);
        mprMark(host->logPath);
        mprMark(host->limits);

    } else if (flags & MPR_MANAGE_FREE) {
        /* The http->hosts list is static. ie. The hosts won't be marked via http->hosts */
        httpRemoveHost(MPR->httpService, host);
    }
}


void httpSetHostLogRotation(HttpHost *host, int logCount, int logSize)
{
    host->logCount = logCount;
    host->logSize = logSize;
}


void httpSetHostHome(HttpHost *host, cchar *home)
{
    host->home = mprGetAbsPath(home);
}


/*
    IP may be null in which case the host is listening on all interfaces. Port may be set to -1 and ip may contain a port
    specifier, ie. "address:port".
 */
void httpSetHostIpAddr(HttpHost *host, cchar *ip, int port)
{
    char    *pip;

    if (port < 0 && schr(ip, ':')) {
        mprParseIp(ip, &pip, &port, -1);
        ip = pip;
    }
    host->ip = sclone(ip);
    host->port = port;

    if (!host->name) {
        if (ip) {
            if (port > 0) {
                host->name = sfmt("%s:%d", ip, port);
            } else {
                host->name = sclone(ip);
            }
        } else {
            mprAssert(port > 0);
            host->name = sfmt("*:%d", port);
        }
    }
}


void httpSetHostName(HttpHost *host, cchar *name)
{
    host->name = sclone(name);
}


void httpSetHostProtocol(HttpHost *host, cchar *protocol)
{
    host->protocol = sclone(protocol);
}


int httpAddRoute(HttpHost *host, HttpRoute *route)
{
    mprAssert(route);
    
    if (host->parent && host->routes == host->parent->routes) {
        host->routes = mprCloneList(host->parent->routes);
    }
    if (mprLookupItem(host->routes, route) < 0) {
        mprAddItem(host->routes, route);
    }
    httpSetRouteHost(route, host);
    return 0;
}


void httpResetRoutes(HttpHost *host)
{
    host->routes = mprCreateList(-1, 0);
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


#if UNUSED && KEEP
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
