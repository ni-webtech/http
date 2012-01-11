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

HttpHost *httpCreateHost(cchar *home)
{
    HttpHost    *host;
    Http        *http;

    http = MPR->httpService;
    if ((host = mprAllocObj(HttpHost, manageHost)) == 0) {
        return 0;
    }
    if ((host->responseCache = mprCreateCache(MPR_CACHE_SHARED)) == 0) {
        return 0;
    }
    mprSetCacheLimits(host->responseCache, 0, HTTP_CACHE_LIFESPAN, 0, 0);

    host->mutex = mprCreateLock();
    host->routes = mprCreateList(-1, 0);
    host->flags = HTTP_HOST_NO_TRACE;
    host->protocol = sclone("HTTP/1.1");
    host->home = sclone(home ? home : ".");
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
    host->responseCache = parent->responseCache;
    host->home = parent->home;
    host->routes = parent->routes;
    host->flags = parent->flags | HTTP_HOST_VHOST;
    host->protocol = parent->protocol;
    httpAddHost(http, host);
    return host;
}


static void manageHost(HttpHost *host, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(host->name);
        mprMark(host->ip);
        mprMark(host->parent);
        mprMark(host->responseCache);
        mprMark(host->routes);
        mprMark(host->defaultRoute);
        mprMark(host->protocol);
        mprMark(host->mutex);
        mprMark(host->home);

    } else if (flags & MPR_MANAGE_FREE) {
        /* The http->hosts list is static. ie. The hosts won't be marked via http->hosts */
        httpRemoveHost(MPR->httpService, host);
    }
}


int httpStartHost(HttpHost *host)
{
    HttpRoute   *route;
    int         next;

    for (ITERATE_ITEMS(host->routes, route, next)) {
        httpStartRoute(route);
    }
    for (ITERATE_ITEMS(host->routes, route, next)) {
        if (!route->log && route->parent && route->parent->log) {
            route->log = route->parent->log;
        }
    }
    return 0;
}


void httpStopHost(HttpHost *host)
{
    HttpRoute   *route;
    int         next;

    for (ITERATE_ITEMS(host->routes, route, next)) {
        httpStopRoute(route);
    }
}


HttpRoute *httpGetHostDefaultRoute(HttpHost *host)
{
    return host->defaultRoute;
}


static void printRoute(HttpRoute *route, int next, bool full)
{
    cchar   *methods, *pattern, *target, *index;
    int     nextIndex;

    methods = httpGetRouteMethods(route);
    methods = methods ? methods : "*";
    pattern = (route->pattern && *route->pattern) ? route->pattern : "^/";
    target = (route->target && *route->target) ? route->target : "$&";
    if (full) {
        mprRawLog(0, "\n%d. %s\n", next, route->name);
        mprRawLog(0, "    Pattern:      %s\n", pattern);
        mprRawLog(0, "    StartSegment: %s\n", route->startSegment);
        mprRawLog(0, "    StartsWith:   %s\n", route->startWith);
        mprRawLog(0, "    RegExp:       %s\n", route->optimizedPattern);
        mprRawLog(0, "    Methods:      %s\n", methods);
        mprRawLog(0, "    Prefix:       %s\n", route->prefix);
        mprRawLog(0, "    Target:       %s\n", target);
        mprRawLog(0, "    Directory:    %s\n", route->dir);
        if (route->indicies) {
            mprRawLog(0, "    Indicies      ");
            for (ITERATE_ITEMS(route->indicies, index, nextIndex)) {
                mprRawLog(0, "%s ", index);
            }
        }
        mprRawLog(0, "\n    Next Group    %d\n", route->nextGroup);
        if (route->handler) {
            mprRawLog(0, "    Handler:      %s\n", route->handler->name);
        }
        mprRawLog(0, "\n");
    } else {
        mprRawLog(0, "%-20s %-12s %-40s %-14s\n", route->name, methods ? methods : "*", pattern, target);
    }
}


void httpLogRoutes(HttpHost *host, bool full)
{
    HttpRoute   *route;
    int         next, foundDefault;

    if (!full) {
        mprRawLog(0, "%-20s %-12s %-40s %-14s\n", "Name", "Methods", "Pattern", "Target");
    }
    for (foundDefault = next = 0; (route = mprGetNextItem(host->routes, &next)) != 0; ) {
        printRoute(route, next - 1, full);
        if (route == host->defaultRoute) {
            foundDefault++;
        }
    }
    /*
        Add the default so LogRoutes can print the default route which has yet been added to host->routes
     */
    if (!foundDefault && host->defaultRoute) {
        printRoute(host->defaultRoute, next - 1, full);
    }
    mprRawLog(0, "\n");
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
        mprParseSocketAddress(ip, &pip, &port, -1);
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
    HttpRoute   *prev, *item;
    int         i, thisRoute;

    mprAssert(route);
    
    if (host->parent && host->routes == host->parent->routes) {
        host->routes = mprCloneList(host->parent->routes);
    }
    if (mprLookupItem(host->routes, route) < 0) {
        thisRoute = mprAddItem(host->routes, route);
        if (thisRoute > 0) {
            prev = mprGetItem(host->routes, thisRoute - 1);
            if (!smatch(prev->startSegment, route->startSegment)) {
                prev->nextGroup = thisRoute;
                for (i = thisRoute - 2; i >= 0; i--) {
                    item = mprGetItem(host->routes, i);
                    if (smatch(item->startSegment, prev->startSegment)) {
                        item->nextGroup = thisRoute;
                    } else {
                        break;
                    }
                }
            }
        }
    }
    httpSetRouteHost(route, host);
    return 0;
}


HttpRoute *httpLookupRoute(HttpHost *host, cchar *name)
{
    HttpRoute   *route;
    int         next;

    if (name == 0 || *name == '\0') {
        name = "default";
    }
    for (next = 0; (route = mprGetNextItem(host->routes, &next)) != 0; ) {
        mprAssert(route->name);
        if (smatch(route->name, name)) {
            return route;
        }
    }
    return 0;
}


void httpResetRoutes(HttpHost *host)
{
    host->routes = mprCreateList(-1, 0);
}


void httpSetHostDefaultRoute(HttpHost *host, HttpRoute *route)
{
    host->defaultRoute = route;
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
