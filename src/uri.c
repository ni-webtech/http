/*
    uri.c - URI manipulation routines
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/************************************ Code ************************************/

/*  Create and initialize a URI. This accepts full URIs with schemes (http:) and partial URLs
 */
HttpUri *httpCreateUri(MprCtx ctx, cchar *uri, int complete)
{
    HttpUri     *up;
    char        *tok, *cp, *last_delim, *hostbuf;
    int         c, len, ulen;

    mprAssert(uri);

    up = mprAllocObjZeroed(ctx, HttpUri);
    if (up == 0) {
        return 0;
    }

    /*  
        Allocate a single buffer to hold all the cracked fields.
     */
    ulen = (int) strlen(uri);
    len = ulen *  2 + 3;
    up->uri = mprStrdup(up, uri);
    up->parsedUriBuf = (char*) mprAlloc(up, len *  sizeof(char));

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
            mprStrcpy(hostbuf, ulen + 1, up->host);
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
                mprStrLower(up->ext);
#endif
            }
        } else {
            up->ext = cp + 1;
#if BLD_WIN_LIKE
            mprStrLower(up->ext);
#endif
        }
    }
    if (up->path == 0) {
        up->path = "/";
    }
    return up;
}


/*  
    Create and initialize a URI. This accepts full URIs with schemes (http:) and partial URLs
 */
HttpUri *httpCreateUriFromParts(MprCtx ctx, cchar *scheme, cchar *host, int port, cchar *path, cchar *reference, 
        cchar *query)
{
    HttpUri     *up;
    char        *cp, *last_delim;

    up = mprAllocObjZeroed(ctx, HttpUri);
    if (up == 0) {
        return 0;
    }
    if (scheme) {
        up->scheme = mprStrdup(up, scheme);
    }
    if (host) {
        up->host = mprStrdup(up, host);
        if ((cp = strchr(host, ':')) && port == 0) {
            port = (int) mprAtoi(++cp, 10);
        }
    }
    if (port) {
        up->port = port;
    }
    if (path) {
        while (path[0] == '/' && path[1] == '/')
            path++;
        up->path = mprStrdup(up, path);
    }
    if (up->path == 0) {
        up->path = "/";
    }
    if (reference) {
        up->reference = mprStrdup(up, reference);
    }
    if (query) {
        up->query = mprStrdup(up, query);
    }
    if ((cp = strrchr(up->path, '.')) != NULL) {
        if ((last_delim = strrchr(up->path, '/')) != NULL) {
            if (last_delim <= cp) {
                up->ext = cp + 1;
#if BLD_WIN_LIKE
                mprStrLower(up->ext);
#endif
            }
        } else {
            up->ext = cp + 1;
#if BLD_WIN_LIKE
            mprStrLower(up->ext);
#endif
        }
    }
    return up;
}


char *httpUriToString(MprCtx ctx, HttpUri *uri, int complete)
{
    return httpFormatUri(ctx, uri->scheme, uri->host, uri->port, uri->path, uri->reference, uri->query, complete);
}


/*  
    Format a fully qualified URI
    If complete is true, missing elements are completed
 */
char *httpFormatUri(MprCtx ctx, cchar *scheme, cchar *host, int port, cchar *path, cchar *reference, cchar *query, 
        int complete)
{
    char    portBuf[16], *uri;
    cchar   *hostDelim, *portDelim, *pathDelim, *queryDelim, *referenceDelim;

    if (complete || host) {
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
        if (port != 0) {
            mprItoa(portBuf, sizeof(portBuf), port, 10);
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
        uri = mprStrcat(ctx, -1, scheme, hostDelim, host, portDelim, portBuf, pathDelim, path, referenceDelim, 
            reference, queryDelim, query, NULL);
    } else {
        uri = mprStrcat(ctx, -1, scheme, hostDelim, host, pathDelim, path, referenceDelim, reference, queryDelim, 
            query, NULL);
    }
    return uri;
}


#if UNUSED
//  MOB -- DEPRECATE and remove this
/*  
    Validate a Uri
    WARNING: this code will not fully validate against certain Windows 95/98/Me bugs. Don't use this code in these
    operating systems without modifying this code to remove "con", "nul", "aux", "clock$" and "config$" in either
    case from the URI. The MprFileSystem::stat() will perform these checks to determine if a file is a device file.
 */
char *httpValidateUri(MprCtx ctx, cchar *uriArg)
{
    char    *uri, *sp, *dp, *xp, *dot;

    if ((uri = mprStrdup(ctx, uriArg)) == 0) {
        return 0;
    }

    /*  Remove multiple path separators and map '\\' to '/' for windows
     */
    sp = dp = uri;
    while (*sp) {
#if BLD_WIN_LIKE
        if (*sp == '\\') {
            *sp = '/';
        }
#endif
        if (sp[0] == '/' && sp[1] == '/') {
            sp++;
        } else {
            *dp++ = *sp++;
        }
    }
    *dp = '\0';

    dot = strchr(uri, '.');
    if (dot == 0) {
        return uri;
    }

    /*  
        Per RFC 1808, remove "./" segments
     */
    dp = dot;
    for (sp = dot; *sp; ) {
        if (*sp == '.' && sp[1] == '/' && (sp == uri || sp[-1] == '/')) {
            sp += 2;
        } else {
            *dp++ = *sp++;
        }
    }
    *dp = '\0';

    /*  
        Remove trailing "."
     */
    if ((dp == &uri[1] && uri[0] == '.') ||
        (dp > &uri[1] && dp[-1] == '.' && dp[-2] == '/')) {
        *--dp = '\0';
    }

    /*  
        Remove "../"
     */
    for (sp = dot; *sp; ) {
        if (*sp == '.' && sp[1] == '.' && sp[2] == '/' && (sp == uri || sp[-1] == '/')) {
            xp = sp + 3;
            sp -= 2;
            if (sp < uri) {
                sp = uri;
            } else {
                while (sp >= uri && *sp != '/') {
                    sp--;
                }
                sp++;
            }
            dp = sp;
            while ((*dp++ = *xp) != 0) {
                xp++;
            }
        } else {
            sp++;
        }
    }
    *dp = '\0';

    /*  
        Remove trailing "/.."
     */
    if (sp == &uri[2] && *uri == '.' && uri[1] == '.') {
        *uri = '\0';
    } else {
        if (sp > &uri[2] && sp[-1] == '.' && sp[-2] == '.' && sp[-3] == '/') {
            sp -= 4;
            if (sp < uri) {
                sp = uri;
            } else {
                while (sp >= uri && *sp != '/') {
                    sp--;
                }
                sp++;
            }
            *sp = '\0';
        }
    }
#if BLD_WIN_LIKE
    if (*uri != '\0') {
        char    *cp;
        /*
            There was some extra URI past the matching alias prefix portion.  Windows will ignore trailing "."
            and " ". We must reject here as the URL probably won't match due to the trailing character and the
            copyHandler will return the unprocessed content to the user. Bad.
         */
        cp = &uri[strlen(uri) - 1];
        while (cp >= uri) {
            if (*cp == '.' || *cp == ' ') {
                *cp-- = '\0';
            } else {
                break;
            }
        }
    }
#endif
    return uri;
}
#endif


#if FUTURE
char *httpJoinUriPath(Ejs *ejs, HttpUri *uri, int argc, EjsObj **argv)
{
    char    *other, *cp, *result, *prior;
    int     i, abs;

    args = (EjsArray*) argv[0];
    result = mprStrdup(np, uri->path);

    for (i = 0; i < argc; i++) {
        other = argv[i];
        prior = result;
        if (*prior == '\0') {
            result = mprStrdup(uri, other);
        } else {
            if (prior[strlen(prior) - 1] == '/') {
                result = mprStrcat(uri, -1, prior, other, NULL);
            } else {
                if ((cp = strrchr(prior, '/')) != NULL) {
                    cp[1] = '\0';
                }
                result = mprStrcat(uri, -1, prior, other, NULL);
            }
        }
        if (prior != uri->path) {
            mprFree(prior);
        }
    }
    uri->path = httpNormalizeUriPath(np, result);
    mprFree(result);
    uri->ext = (char*) mprGetPathExtension(uri, uri->path);
    return uri;
}
#endif


/*
    Normalize a URI path to remove redundant "./" and cleanup "../" and make separator uniform. Does not make an abs path.
    It does not map separators nor change case. 
 */
char *httpNormalizeUriPath(MprCtx ctx, cchar *uriArg)
{
    char    *dupPath, *path, *sp, *dp, *mark, **segments;
    int     j, i, nseg, len;

    if (uriArg == 0 || *uriArg == '\0') {
        return mprStrdup(ctx, "");
    }
    len = strlen(uriArg);
    if ((dupPath = mprAlloc(ctx, len + 2)) == 0) {
        return NULL;
    }
    strcpy(dupPath, uriArg);

    if ((segments = mprAlloc(ctx, sizeof(char*) * (len + 1))) == 0) {
        mprFree(dupPath);
        return NULL;
    }
    nseg = len = 0;
    for (mark = sp = dupPath; *sp; sp++) {
        if (*sp == '/') {
            *sp = '\0';
            while (sp[1] == '/') {
                sp++;
            }
            segments[nseg++] = mark;
            len += sp - mark;
            mark = sp + 1;
        }
    }
    segments[nseg++] = mark;
    len += sp - mark;
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
                if ((i+1) == nseg) {
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

    if ((path = mprAlloc(ctx, len + nseg + 1)) != 0) {
        for (i = 0, dp = path; i < nseg; ) {
            strcpy(dp, segments[i]);
            len = strlen(segments[i]);
            dp += len;
            if (++i < nseg) {
                *dp++ = '/';
            }
        }
        *dp = '\0';
    }
    mprFree(dupPath);
    mprFree(segments);
    return path;
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

    @end
 */
