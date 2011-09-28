/*
    uri.c - URI manipulation routines
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

static int getPort(HttpUri *uri);
static int getDefaultPort(cchar *scheme);
static void manageUri(HttpUri *uri, int flags);
static void trimPathToDirname(HttpUri *uri);

/************************************ Code ************************************/
/*  
    Create and initialize a URI. This accepts full URIs with schemes (http:) and partial URLs
 */
HttpUri *httpCreateUri(cchar *uri, int complete)
{
    HttpUri     *up;
    char        *tok, *cp;

    mprAssert(uri);

    if ((up = mprAllocObj(HttpUri, manageUri)) == 0) {
        return 0;
    }
    up->uri = sclone(uri);

    tok = up->uri;
    if (schr(tok, ':')) {
        if (sncmp(up->uri, "https://", 8) == 0) {
            up->scheme = sclone("https");
            up->secure = 1;
            if (complete) {
                up->port = 443;
            }
            tok = &up->uri[8];

        } else if (sncmp(up->uri, "http://", 7) == 0) {
            up->scheme = sclone("http");
            tok = &up->uri[7];

        } else {
            up->scheme = 0;
            tok = up->uri;
        }
        if ((cp = spbrk(tok, ":/")) == NULL) {
            up->host = sclone(tok);

        } else if (*cp == ':') {
            up->host = snclone(tok, cp - tok);
            up->port = atoi(++cp);

        } else if (*cp == '/') {
            up->host = snclone(tok, cp - tok);
        }
        if (complete && up->port == 0) {
            up->port = 80;
        }
        tok = schr(cp, '/');

    } else {
        if (complete) {
            up->scheme = "http";
            up->host = "localhost";
            up->port = 80;
        }
        tok = up->uri;
    }

    if ((cp = spbrk(tok, "#?")) == NULL) {
        up->path = sclone(tok);

    } else {
        up->path = snclone(tok, cp - tok);
        tok = cp + 1;
        if (*cp == '#') {
            if ((cp = schr(tok, '?')) != NULL) {
                up->reference = snclone(tok, cp - tok);
                up->query = sclone(++cp);
            } else {
                up->reference = sclone(tok);
            }
        } else {
            up->query = sclone(tok);
        }
    }

    if (up->path && (tok = srchr(up->path, '.')) != NULL) {
        if ((cp = srchr(up->path, '/')) != NULL) {
            if (cp <= tok) {
                up->ext = sclone(++tok);
            }
        } else {
            up->ext = sclone(++tok);
        }
    }
    if (up->path == 0) {
        up->path = sclone("/");
    }
    return up;
}


static void manageUri(HttpUri *uri, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(uri->scheme);
        mprMark(uri->host);
        mprMark(uri->path);
        mprMark(uri->ext);
        mprMark(uri->reference);
        mprMark(uri->query);
        mprMark(uri->uri);
    }
}


/*  
    Create and initialize a URI. This accepts full URIs with schemes (http:) and partial URLs
 */
HttpUri *httpCreateUriFromParts(cchar *scheme, cchar *host, int port, cchar *path, cchar *reference, cchar *query, 
        int complete)
{
    HttpUri     *up;
    char        *cp, *tok;

    if ((up = mprAllocObj(HttpUri, manageUri)) == 0) {
        return 0;
    }
    if (scheme) {
        up->scheme = sclone(scheme);
    } else if (complete) {
        up->scheme = "http";
    }
    if (host) {
        up->host = sclone(host);
        if ((cp = schr(host, ':')) && port == 0) {
            port = (int) stoi(++cp, 10, NULL);
        }
    } else if (complete) {
        up->host = sclone("localhost");
    }
    if (port) {
        up->port = port;
    }
    if (path) {
        while (path[0] == '/' && path[1] == '/') {
            path++;
        }
        up->path = sclone(path);
    }
    if (up->path == 0) {
        up->path = sclone("/");
    }
    if (reference) {
        up->reference = sclone(reference);
    }
    if (query) {
        up->query = sclone(query);
    }
    if ((tok = srchr(up->path, '.')) != NULL) {
        if ((cp = srchr(up->path, '/')) != NULL) {
            if (cp <= tok) {
                up->ext = sclone(&tok[1]);
            }
        } else {
            up->ext = sclone(&tok[1]);
        }
    }
    return up;
}


HttpUri *httpCloneUri(HttpUri *base, int complete)
{
    HttpUri     *up;
    char        *path, *cp, *tok;
    int         port;

    if ((up = mprAllocObj(HttpUri, manageUri)) == 0) {
        return 0;
    }
    port = base->port;
    path = base->path;

    if (base->scheme) {
        up->scheme = sclone(base->scheme);
    } else if (complete) {
        up->scheme = sclone("http");
    }
    if (base->host) {
        up->host = sclone(base->host);
        if ((cp = schr(base->host, ':')) && port == 0) {
            port = (int) stoi(++cp, 10, NULL);
        }
    } else if (complete) {
        base->host = sclone("localhost");
    }
    if (port) {
        up->port = port;
    }
    if (path) {
        while (path[0] == '/' && path[1] == '/') {
            path++;
        }
        up->path = sclone(path);
    }
    if (up->path == 0) {
        up->path = sclone("/");
    }
    if (base->reference) {
        up->reference = sclone(base->reference);
    }
    if (base->query) {
        up->query = sclone(base->query);
    }
    if ((tok = srchr(up->path, '.')) != NULL) {
        if ((cp = srchr(up->path, '/')) != NULL) {
            if (cp <= tok) {
                up->ext = sclone(&tok[1]);
            }
        } else {
            up->ext = sclone(&tok[1]);
        }
    }
    return up;
}


HttpUri *httpCompleteUri(HttpUri *uri, HttpUri *missing)
{
    char        *scheme, *host;
    int         port;

    scheme = (missing) ? missing->scheme : "http";
    host = (missing) ? missing->host : "localhost";
    port = (missing) ? missing->port : 0;

    if (uri->scheme == 0) {
        uri->scheme = sclone(scheme);
    }
    if (uri->port == 0 && port) {
        /* Don't complete port if there is a host */
        if (uri->host == 0) {
            uri->port = port;
        }
    }
    if (uri->host == 0) {
        uri->host = sclone(host);
    }
    return uri;
}


/*  
    Format a fully qualified URI
    If complete is true, missing elements are completed
 */
char *httpFormatUri(cchar *scheme, cchar *host, int port, cchar *path, cchar *reference, cchar *query, int complete)
{
    char    *uri;
    cchar   *portStr, *hostDelim, *portDelim, *pathDelim, *queryDelim, *referenceDelim;

    if (complete || host || scheme) {
        if (scheme == 0 || *scheme == '\0') {
            scheme = "http";
        }
        if (host == 0 || *host == '\0') {
            host = "localhost";
        }
        hostDelim = "://";
    } else {
        host = hostDelim = "";
    }
    /*  
        Hosts with integral port specifiers override
     */
    if (host && schr(host, ':')) {
        portDelim = 0;
    } else {
        if (port != 0 && port != getDefaultPort(scheme)) {
            portStr = itos(port, 10);
            portDelim = ":";
        } else {
            portStr = "";
            portDelim = "";
        }
    }
    if (scheme == 0) {
        scheme = "";
    }
    if (path && *path) {
        if (*hostDelim) {
            pathDelim = (*path == '/') ? "" :  "/";
        } else {
            pathDelim = "";
        }
    } else {
        pathDelim = path = "";
    }
    if (reference && *reference) {
        referenceDelim = "#";
    } else {
        referenceDelim = reference = "";
    }
    if (query && *query) {
        queryDelim = "?";
    } else {
        queryDelim = query = "";
    }
    if (portDelim) {
        uri = sjoin(scheme, hostDelim, host, portDelim, portStr, pathDelim, path, referenceDelim, reference, 
            queryDelim, query, NULL);
    } else {
        uri = sjoin(scheme, hostDelim, host, pathDelim, path, referenceDelim, reference, queryDelim, query, NULL);
    }
    return uri;
}


/*
    This returns a URI relative to the base for the given target

    uri = target.relative(base)
 */
HttpUri *httpGetRelativeUri(HttpUri *base, HttpUri *target, int clone)
{
    HttpUri     *uri;
    char        *basePath, *bp, *cp, *tp, *startDiff;
    int         i, baseSegments, commonSegments;

    if (target == 0) {
        return (clone) ? httpCloneUri(base, 0) : base;
    }
    if (!(target->path && target->path[0] == '/') || !((base->path && base->path[0] == '/'))) {
        /* If target is relative, just use it. If base is relative, can't use it because we don't know where it is */
        return (clone) ? httpCloneUri(target, 0) : target;
    }
    if (base->scheme && target->scheme && scmp(base->scheme, target->scheme) != 0) {
        return (clone) ? httpCloneUri(target, 0) : target;
    }
    if (base->host && target->host && (base->host && scmp(base->host, target->host) != 0)) {
        return (clone) ? httpCloneUri(target, 0) : target;
    }
    if (getPort(base) != getPort(target)) {
        return (clone) ? httpCloneUri(target, 0) : target;
    }
    basePath = httpNormalizeUriPath(base->path);

    /* Count trailing "/" */
    for (baseSegments = 0, bp = basePath; *bp; bp++) {
        if (*bp == '/') {
            baseSegments++;
        }
    }

    /*
        Find portion of path that matches the base, if any.
     */
    commonSegments = 0;
    for (bp = base->path, tp = startDiff = target->path; *bp && *tp; bp++, tp++) {
        if (*bp == '/') {
            if (*tp == '/') {
                commonSegments++;
                startDiff = tp;
            }
        } else {
            if (*bp != *tp) {
                break;
            }
        }
    }

    /*
        Add one more segment if the last segment matches. Handle trailing separators.
     */
#if OLD
    if ((*bp == '/' || *bp == '\0') && (*tp == '/' || *tp == '\0')) {
        commonSegments++;
    }
#endif
    if (*startDiff == '/') {
        startDiff++;
    }
    
    if ((uri = httpCloneUri(target, 0)) == 0) {
        return 0;
    }
    uri->host = 0;
    uri->scheme = 0;
    uri->port = 0;

    uri->path = cp = mprAlloc(baseSegments * 3 + (int) slen(target->path) + 2);
    for (i = commonSegments; i < baseSegments; i++) {
        *cp++ = '.';
        *cp++ = '.';
        *cp++ = '/';
    }
    if (*startDiff) {
        strcpy(cp, startDiff);
    } else if (cp > uri->path) {
        /*
            Cleanup trailing separators ("../" is the end of the new path)
         */
        cp[-1] = '\0';
    } else {
        strcpy(uri->path, ".");
    }
    return uri;
}


/*
    result = base.join(other)
 */
HttpUri *httpJoinUriPath(HttpUri *result, HttpUri *base, HttpUri *other)
{
    char    *sep;

    if (other->path[0] == '/') {
        result->path = sclone(other->path);
    } else {
        sep = ((base->path[0] == '\0' || base->path[slen(base->path) - 1] == '/') || 
               (other->path[0] == '\0' || other->path[0] == '/'))  ? "" : "/";
        result->path = sjoin(base->path, sep, other->path, NULL);
    }
    return result;
}


HttpUri *httpJoinUri(HttpUri *uri, int argc, HttpUri **others)
{
    HttpUri     *other;
    int         i;

    uri = httpCloneUri(uri, 0);

    for (i = 0; i < argc; i++) {
        other = others[i];
        if (other->scheme) {
            uri->scheme = sclone(other->scheme);
        }
        if (other->host) {
            uri->host = sclone(other->host);
        }
        if (other->port) {
            uri->port = other->port;
        }
        if (other->path) {
            httpJoinUriPath(uri, uri, other);
        }
        if (other->reference) {
            uri->reference = sclone(other->reference);
        }
        if (other->query) {
            uri->query = sclone(other->query);
        }
    }
    uri->ext = mprGetPathExt(uri->path);
    return uri;
}


HttpUri *httpMakeUriLocal(HttpUri *uri)
{
    if (uri) {
        uri->host = 0;
        uri->scheme = 0;
        uri->port = 0;
    }
    return uri;
}


void httpNormalizeUri(HttpUri *uri)
{
    uri->path = httpNormalizeUriPath(uri->path);
}


/*
    Normalize a URI path to remove redundant "./" and cleanup "../" and make separator uniform. Does not make an abs path.
    It does not map separators nor change case. 
 */
char *httpNormalizeUriPath(cchar *pathArg)
{
    char    *dupPath, *path, *sp, *dp, *mark, **segments;
    int     firstc, j, i, nseg, len;

    if (pathArg == 0 || *pathArg == '\0') {
        return mprEmptyString();
    }
    len = (int) slen(pathArg);
    if ((dupPath = mprAlloc(len + 2)) == 0) {
        return NULL;
    }
    strcpy(dupPath, pathArg);

    if ((segments = mprAlloc(sizeof(char*) * (len + 1))) == 0) {
        return NULL;
    }
    nseg = len = 0;
    firstc = *dupPath;
    for (mark = sp = dupPath; *sp; sp++) {
        if (*sp == '/') {
            *sp = '\0';
            while (sp[1] == '/') {
                sp++;
            }
            segments[nseg++] = mark;
            len += (int) (sp - mark);
            mark = sp + 1;
        }
    }
    segments[nseg++] = mark;
    len += (int) (sp - mark);
    for (j = i = 0; i < nseg; i++, j++) {
        sp = segments[i];
        if (sp[0] == '.') {
            if (sp[1] == '\0')  {
                if ((i+1) == nseg) {
                    segments[j] = "";
                } else {
                    j--;
                }
            } else if (sp[1] == '.' && sp[2] == '\0')  {
                if (i == 1 && *segments[0] == '\0') {
                    j = 0;
                } else if ((i+1) == nseg) {
                    if (--j >= 0) {
                        segments[j] = "";
                    }
                } else {
                    j = max(j - 2, -1);
                }
            }
        } else {
            segments[j] = segments[i];
        }
    }
    nseg = j;
    mprAssert(nseg >= 0);
    if ((path = mprAlloc(len + nseg + 1)) != 0) {
        for (i = 0, dp = path; i < nseg; ) {
            strcpy(dp, segments[i]);
            len = (int) slen(segments[i]);
            dp += len;
            if (++i < nseg || (nseg == 1 && *segments[0] == '\0' && firstc == '/')) {
                *dp++ = '/';
            }
        }
        *dp = '\0';
    }
    return path;
}


HttpUri *httpResolveUri(HttpUri *base, int argc, HttpUri **others, bool local)
{
    HttpUri     *current, *other;
    int         i;

    if ((current = httpCloneUri(base, 0)) == 0) {
        return 0;
    }
    if (local) {
        current->host = 0;
        current->scheme = 0;
        current->port = 0;
    }
    /*
        Must not inherit the query or reference
     */
    current->query = NULL;
    current->reference = NULL;

    for (i = 0; i < argc; i++) {
        other = others[i];
        if (other->scheme) {
            current->scheme = sclone(other->scheme);
        }
        if (other->host) {
            current->host = sclone(other->host);
        }
        if (other->port) {
            current->port = other->port;
        }
        if (other->path) {
            trimPathToDirname(current);
            httpJoinUriPath(current, current, other);
        }
        if (other->reference) {
            current->reference = sclone(other->reference);
        }
        if (other->query) {
            current->query = sclone(other->query);
        }
    }
    current->ext = mprGetPathExt(current->path);
    return current;
}


char *httpUriToString(HttpUri *uri, int complete)
{
    return httpFormatUri(uri->scheme, uri->host, uri->port, uri->path, uri->reference, uri->query, complete);
}


static int getPort(HttpUri *uri)
{
    if (uri->port) {
        return uri->port;
    }
    return (uri->scheme && scmp(uri->scheme, "https") == 0) ? 443 : 80;
}


static int getDefaultPort(cchar *scheme)
{
    return (scheme && scmp(scheme, "https") == 0) ? 443 : 80;
}


static void trimPathToDirname(HttpUri *uri) 
{
    char        *path, *cp;
    int         len;

    path = uri->path;
    len = (int) slen(path);
    if (path[len - 1] == '/') {
        if (len > 1) {
            path[len - 1] = '\0';
        }
    } else {
        if ((cp = srchr(path, '/')) != 0) {
            if (cp > path) {
                *cp = '\0';
            } else {
                cp[1] = '\0';
            }
        } else if (*path) {
            path[0] = '\0';
        }
    }
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
