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

/*  Create and initialize a URI. This accepts full URIs with schemes (http:) and partial URLs
 */
HttpUri *httpCreateUri(cchar *uri, int complete)
{
    HttpUri     *up;
    char        *tok, *cp, *last_delim, *hostbuf;
    int         c, len, ulen;

    mprAssert(uri);

    if ((up = mprAllocObj(HttpUri, manageUri)) == 0) {
        return 0;
    }

    /*  Allocate a single buffer to hold all the cracked fields.  */
    ulen = (int) strlen(uri);
    len = ulen *  2 + 3;
    up->uri = sclone(uri);
    up->parsedUriBuf = mprAlloc(len *  sizeof(char));

    hostbuf = &up->parsedUriBuf[ulen+1];
    strcpy(up->parsedUriBuf, uri);
    tok = 0;

    if (strchr(up->parsedUriBuf, ':')) {
        if (strncmp(up->parsedUriBuf, "https://", 8) == 0) {
            up->scheme = up->parsedUriBuf;
            up->secure = 1;
            if (complete) {
                up->port = 443;
            }
            tok = &up->scheme[8];
            tok[-3] = '\0';
        } else if (strncmp(up->parsedUriBuf, "http://", 7) == 0) {
            up->scheme = up->parsedUriBuf;
            tok = &up->scheme[7];
            tok[-3] = '\0';
        } else {
            tok = up->parsedUriBuf;
            up->scheme = 0;
        }
        up->host = tok;
        for (cp = tok; *cp; cp++) {
            if (*cp == '/') {
                break;
            }
            if (*cp == ':') {
                *cp++ = '\0';
                up->port = atoi(cp);
                tok = cp;
            }
        }
        if ((cp = strchr(tok, '/')) != NULL) {
            c = *cp;
            *cp = '\0';
            scopy(hostbuf, ulen + 1, up->host);
            *cp = c;
            up->host = hostbuf;
            up->path = cp;
            while (cp[0] == '/' && cp[1] == '/')
                cp++;
            tok = cp;
        }

    } else {
        if (complete) {
            up->scheme = "http";
            up->host = "localhost";
            up->port = 80;
        }
        tok = up->path = up->parsedUriBuf;
    }

    /*  
        Split off the reference fragment
     */
    if ((cp = strchr(tok, '#')) != NULL) {
        *cp++ = '\0';
        up->reference = cp;
        tok = cp;
    }

    /*  
        Split off the query string.
     */
    if ((cp = strchr(tok, '?')) != NULL) {
        *cp++ = '\0';
        up->query = cp;
        tok = up->query;
    }

    if (up->path && (cp = strrchr(up->path, '.')) != NULL) {
        if ((last_delim = strrchr(up->path, '/')) != NULL) {
            if (last_delim <= cp) {
                up->ext = cp + 1;
#if BLD_WIN_LIKE
                for (cp = up->ext; *cp; cp++) {
                    *cp = (char) tolower((int) *cp);
                }
#endif
            }
        } else {
            up->ext = cp + 1;
#if BLD_WIN_LIKE
            for (cp = up->ext; *cp; cp++) {
                *cp = (char) tolower((int) *cp);
            }
#endif
        }
    }
    if (up->path == 0) {
        up->path = "/";
    }
    return up;
}


static void manageUri(HttpUri *uri, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
#if UNUSED
        mprMark(uri->scheme);
        mprMark(uri->host);
        mprMark(uri->path);
        mprMark(uri->ext);
        mprMark(uri->reference);
        mprMark(uri->query);
#endif
        mprMark(uri->uri);
        mprMark(uri->parsedUriBuf);

    } else if (flags & MPR_MANAGE_FREE) {
    }
}


/*  
    Create and initialize a URI. This accepts full URIs with schemes (http:) and partial URLs
 */
HttpUri *httpCreateUriFromParts(cchar *scheme, cchar *host, int port, cchar *path, cchar *reference, cchar *query, 
        int complete)
{
    HttpUri     *up;
    char        *cp, *last_delim;

    if ((up = mprAllocObj(HttpUri, NULL)) == 0) {
        return 0;
    }
    if (scheme) {
        up->scheme = sclone(scheme);
    } else if (complete) {
        up->scheme = "http";
    }
    if (host) {
        up->host = sclone(host);
        if ((cp = strchr(host, ':')) && port == 0) {
            port = (int) stoi(++cp, 10, NULL);
        }
    } else if (complete) {
        host = "localhost";
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
        up->path = "/";
    }
    if (reference) {
        up->reference = sclone(reference);
    }
    if (query) {
        up->query = sclone(query);
    }
    if ((cp = strrchr(up->path, '.')) != NULL) {
        if ((last_delim = strrchr(up->path, '/')) != NULL) {
            if (last_delim <= cp) {
                up->ext = cp + 1;
#if BLD_WIN_LIKE
                for (cp = up->ext; *cp; cp++) {
                    *cp = (char) tolower((int) *cp);
                }
#endif
            }
        } else {
            up->ext = cp + 1;
#if BLD_WIN_LIKE
            for (cp = up->ext; *cp; cp++) {
                *cp = (char) tolower((int) *cp);
            }
#endif
        }
    }
    return up;
}


HttpUri *httpCloneUri(HttpUri *base, int complete)
{
    HttpUri     *up;
    char        *path, *cp, *last_delim;
    int         port;

    if ((up = mprAllocObj(HttpUri, NULL)) == 0) {
        return 0;
    }
    port = base->port;
    path = base->path;

    if (base->scheme) {
        up->scheme = sclone(base->scheme);
    } else if (complete) {
        up->scheme = "http";
    }
    if (base->host) {
        up->host = sclone(base->host);
        if ((cp = strchr(base->host, ':')) && port == 0) {
            port = (int) stoi(++cp, 10, NULL);
        }
    } else if (complete) {
        base->host = "localhost";
    }
    if (port) {
        up->port = port;
    }
    if (path) {
        while (path[0] == '/' && path[1] == '/')
            path++;
        up->path = sclone(path);
    }
    if (up->path == 0) {
        up->path = "/";
    }
    if (base->reference) {
        up->reference = sclone(base->reference);
    }
    if (base->query) {
        up->query = sclone(base->query);
    }
    if ((cp = strrchr(up->path, '.')) != NULL) {
        if ((last_delim = strrchr(up->path, '/')) != NULL) {
            if (last_delim <= cp) {
                up->ext = cp + 1;
#if BLD_WIN_LIKE
                for (cp = up->ext; *cp; cp++) {
                    *cp = (char) tolower((int) *cp);
                }
#endif
            }
        } else {
            up->ext = cp + 1;
#if BLD_WIN_LIKE
            for (cp = up->ext; *cp; cp++) {
                *cp = (char) tolower((int) *cp);
            }
#endif
        }
    }
    return up;
}


HttpUri *httpCompleteUri(HttpUri *uri, HttpUri *missing)
{
    char        *scheme, *host;
    int         port;

    scheme = (missing) ? sclone(missing->scheme) : "http";
    host = (missing) ? sclone(missing->host) : "localhost";
    port = (missing) ? missing->port : 0;

    if (uri->scheme == 0) {
        uri->scheme = scheme;
    }
    if (uri->port == 0 && port) {
        /* Don't complete port if there is a host */
        if (uri->host == 0) {
            uri->port = port;
        }
    }
    if (uri->host == 0) {
        uri->host = host;
    }
    return uri;
}


/*  
    Format a fully qualified URI
    If complete is true, missing elements are completed
 */
char *httpFormatUri(cchar *scheme, cchar *host, int port, cchar *path, cchar *reference, cchar *query, int complete)
{
    char    portBuf[16], *uri;
    cchar   *hostDelim, *portDelim, *pathDelim, *queryDelim, *referenceDelim;

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

    /*  Hosts with integral port specifiers override
     */
    if (host && strchr(host, ':')) {
        portDelim = 0;
    } else {
        if (port != 0 && port != getDefaultPort(scheme)) {
            itos(portBuf, sizeof(portBuf), port, 10);
            portDelim = ":";
        } else {
            portBuf[0] = '\0';
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
        uri = sjoin(scheme, hostDelim, host, portDelim, portBuf, pathDelim, path, referenceDelim, reference, 
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
HttpUri *httpGetRelativeUri(HttpUri *base, HttpUri *target, int dup)
{
    HttpUri     *uri;
    char        *targetPath, *basePath, *bp, *cp, *tp, *startDiff;
    int         i, baseSegments, commonSegments;

    if (target == 0) {
        return (dup) ? httpCloneUri(base, 0) : base;
    }
    if (!(target->path && target->path[0] == '/') || !((base->path && base->path[0] == '/'))) {
        /* If target is relative, just use it. If base is relative, can't use it because we don't know where it is */
        return (dup) ? httpCloneUri(target, 0) : target;
    }
    if (base->scheme && target->scheme && strcmp(base->scheme, target->scheme) != 0) {
        return (dup) ? httpCloneUri(target, 0) : target;
    }
    if (base->host && target->host && (base->host && strcmp(base->host, target->host) != 0)) {
        return (dup) ? httpCloneUri(target, 0) : target;
    }
    if (getPort(base) != getPort(target)) {
        return (dup) ? httpCloneUri(target, 0) : target;
    }

    //  OPT -- Could avoid free if already normalized
    targetPath = httpNormalizeUriPath(target->path);
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

    uri->path = cp = mprAlloc(baseSegments * 3 + (int) strlen(target->path) + 2);
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
            MOB -- do we want to do this?
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
        sep = ((base->path[0] == '\0' || base->path[strlen(base->path) - 1] == '/') || 
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
    uri->ext = (char*) mprGetPathExtension(uri->path);
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
        return sclone("");
    }
    len = (int) strlen(pathArg);
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
            len = (int) strlen(segments[i]);
            dp += len;
            if (++i < nseg || (nseg == 1 && *segments[0] == '\0' && firstc == '/')) {
                *dp++ = '/';
            }
        }
        *dp = '\0';
    }
    return path;
}


HttpUri *httpResolveUri(HttpUri *base, int argc, HttpUri **others, int local)
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
    current->ext = (char*) mprGetPathExtension(current->path);
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
    return (uri->scheme && strcmp(uri->scheme, "https") == 0) ? 443 : 80;
}


static int getDefaultPort(cchar *scheme)
{
    return (scheme && strcmp(scheme, "https") == 0) ? 443 : 80;
}


static void trimPathToDirname(HttpUri *uri) 
{
    char        *path, *cp;
    int         len;

    path = uri->path;
    len = (int) strlen(path);
    if (path[len - 1] == '/') {
        if (len > 1) {
            path[len - 1] = '\0';
        }
    } else {
        if ((cp = strrchr(path, '/')) != 0) {
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

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=8 ts=8 expandtab

    @end
 */
