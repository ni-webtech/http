/*
    authCheck.c - Authorization checking for basic and digest authentication.
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Defines ***********************************/
/*
    Per-request authorization data
 */
typedef struct AuthData 
{
    char            *password;          /* User password or digest */
    char            *userName;
    char            *cnonce;
    char            *nc;
    char            *nonce;
    char            *opaque;
    char            *qop;
    char            *realm;
    char            *uri;
} AuthData;

/********************************** Forwards **********************************/

static int calcDigest(char **digest, cchar *userName, cchar *password, cchar *realm, cchar *uri, 
    cchar *nonce, cchar *qop, cchar *nc, cchar *cnonce, cchar *method);
static char *createDigestNonce(HttpConn *conn, cchar *secret, cchar *realm);
static void decodeBasicAuth(HttpConn *conn, AuthData *ad);
static int  decodeDigestDetails(HttpConn *conn, AuthData *ad);
static void formatAuthResponse(HttpConn *conn, HttpAuth *auth, int code, char *msg, char *logMsg);
static int parseDigestNonce(char *nonce, cchar **secret, cchar **realm, MprTime *when);

/*********************************** Code *************************************/

int httpCheckAuth(HttpConn *conn)
{
    Http        *http;
    HttpRx      *rx;
    HttpAuth    *auth;
    HttpRoute   *route;
    AuthData    *ad;
    MprTime     when;
    cchar       *requiredPassword;
    char        *msg, *requiredDigest;
    cchar       *secret, *realm;
    int         actualAuthType;

    rx = conn->rx;
    http = conn->http;
    route = rx->route;
    auth = route->auth;

    if (!conn->endpoint || auth == 0 || auth->type == 0) {
        return 0;
    }
    if ((ad = mprAllocStruct(AuthData)) == 0) {
        return 0;
    }
    if (rx->authDetails == 0) {
        formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access Denied, Missing authorization details.", 0);
        return HTTP_ROUTE_OK;
    }
    if (scasecmp(rx->authType, "basic") == 0) {
        decodeBasicAuth(conn, ad);
        actualAuthType = HTTP_AUTH_BASIC;

    } else if (scasecmp(rx->authType, "digest") == 0) {
        if (decodeDigestDetails(conn, ad) < 0) {
            httpError(conn, 400, "Bad authorization header");
            return HTTP_ROUTE_OK;
        }
        actualAuthType = HTTP_AUTH_DIGEST;
    } else {
        actualAuthType = HTTP_AUTH_UNKNOWN;
    }
    mprLog(4, "matchAuth: type %d, url %s\nDetails %s\n", auth->type, rx->pathInfo, rx->authDetails);

    if (ad->userName == 0) {
        formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access Denied, Missing user name", 0);
        return HTTP_ROUTE_OK;
    }
    if (auth->type != actualAuthType) {
        formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access Denied, Wrong authentication protocol", 0);
        return HTTP_ROUTE_OK;
    }
    /*  
        Some backend methods can't return the password and will simply do everything in validateCred. 
        In this case, they and will return "". That is okay.
     */
    if ((requiredPassword = (http->getPassword)(auth, auth->requiredRealm, ad->userName)) == 0) {
        formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access Denied, authentication error", "User not defined");
        return 1;
    }
    if (auth->type == HTTP_AUTH_DIGEST) {
        if (scmp(ad->qop, auth->qop) != 0) {
            formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access Denied. Protection quality does not match", 0);
            return HTTP_ROUTE_OK;
        }
        calcDigest(&requiredDigest, 0, requiredPassword, ad->realm, ad->uri, ad->nonce, ad->qop, ad->nc, 
            ad->cnonce, rx->method);
        requiredPassword = requiredDigest;

        /*
            Validate the nonce value - prevents replay attacks
         */
        when = 0; secret = 0; realm = 0;
        parseDigestNonce(ad->nonce, &secret, &realm, &when);
        if (scmp(secret, http->secret) != 0 || scmp(realm, auth->requiredRealm) != 0) {
            formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access denied, authentication error", "Nonce mismatch");
        } else if ((when + (5 * 60 * MPR_TICKS_PER_SEC)) < http->now) {
            formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access denied, authentication error", "Nonce is stale");
        }
    }
    if (!(http->validateCred)(auth, auth->requiredRealm, ad->userName, ad->password, requiredPassword, &msg)) {
        formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access denied, incorrect username/password", msg);
    }
    rx->authenticated = 1;
    return HTTP_ROUTE_OK;
}


/*  
    Decode basic authorization details
 */
static void decodeBasicAuth(HttpConn *conn, AuthData *ad)
{
    HttpRx  *rx;
    char    *decoded, *cp;

    rx = conn->rx;
    if ((decoded = mprDecode64(rx->authDetails)) == 0) {
        return;
    }
    if ((cp = strchr(decoded, ':')) != 0) {
        *cp++ = '\0';
    }
    if (cp) {
        ad->userName = sclone(decoded);
        ad->password = sclone(cp);

    } else {
        ad->userName = mprEmptyString();
        ad->password = mprEmptyString();
    }
    httpSetAuthUser(conn, ad->userName);
}


/*  
    Decode the digest authentication details.
 */
static int decodeDigestDetails(HttpConn *conn, AuthData *ad)
{
    HttpRx      *rx;
    char        *value, *tok, *key, *dp, *sp;
    int         seenComma;

    rx = conn->rx;
    key = sclone(rx->authDetails);

    while (*key) {
        while (*key && isspace((uchar) *key)) {
            key++;
        }
        tok = key;
        while (*tok && !isspace((uchar) *tok) && *tok != ',' && *tok != '=') {
            tok++;
        }
        *tok++ = '\0';

        while (isspace((uchar) *tok)) {
            tok++;
        }
        seenComma = 0;
        if (*tok == '\"') {
            value = ++tok;
            while (*tok != '\"' && *tok != '\0') {
                tok++;
            }
        } else {
            value = tok;
            while (*tok != ',' && *tok != '\0') {
                tok++;
            }
            seenComma++;
        }
        *tok++ = '\0';

        /*
            Handle back-quoting
         */
        if (strchr(value, '\\')) {
            for (dp = sp = value; *sp; sp++) {
                if (*sp == '\\') {
                    sp++;
                }
                *dp++ = *sp++;
            }
            *dp = '\0';
        }

        /*
            user, response, oqaque, uri, realm, nonce, nc, cnonce, qop
         */
        switch (tolower((uchar) *key)) {
        case 'a':
            if (scasecmp(key, "algorithm") == 0) {
                break;
            } else if (scasecmp(key, "auth-param") == 0) {
                break;
            }
            break;

        case 'c':
            if (scasecmp(key, "cnonce") == 0) {
                ad->cnonce = sclone(value);
            }
            break;

        case 'd':
            if (scasecmp(key, "domain") == 0) {
                break;
            }
            break;

        case 'n':
            if (scasecmp(key, "nc") == 0) {
                ad->nc = sclone(value);
            } else if (scasecmp(key, "nonce") == 0) {
                ad->nonce = sclone(value);
            }
            break;

        case 'o':
            if (scasecmp(key, "opaque") == 0) {
                ad->opaque = sclone(value);
            }
            break;

        case 'q':
            if (scasecmp(key, "qop") == 0) {
                ad->qop = sclone(value);
            }
            break;

        case 'r':
            if (scasecmp(key, "realm") == 0) {
                ad->realm = sclone(value);
            } else if (scasecmp(key, "response") == 0) {
                /* Store the response digest in the password field */
                ad->password = sclone(value);
            }
            break;

        case 's':
            if (scasecmp(key, "stale") == 0) {
                break;
            }
        
        case 'u':
            if (scasecmp(key, "uri") == 0) {
                ad->uri = sclone(value);
            } else if (scasecmp(key, "username") == 0 || scasecmp(key, "user") == 0) {
                ad->userName = sclone(value);
            }
            break;

        default:
            /*  Just ignore keywords we don't understand */
            ;
        }
        key = tok;
        if (!seenComma) {
            while (*key && *key != ',') {
                key++;
            }
            if (*key) {
                key++;
            }
        }
    }
    if (ad->userName == 0 || ad->realm == 0 || ad->nonce == 0 || ad->uri == 0 || ad->password == 0) {
        return MPR_ERR_BAD_ARGS;
    }
    if (ad->qop && (ad->cnonce == 0 || ad->nc == 0)) {
        return MPR_ERR_BAD_ARGS;
    }
    if (ad->qop == 0) {
        ad->qop = mprEmptyString();
    }
    httpSetAuthUser(conn, ad->userName);
    return 0;
}


/*  
    Format an authentication response. This is typically a 401 response code.
 */
static void formatAuthResponse(HttpConn *conn, HttpAuth *auth, int code, char *msg, char *logMsg)
{
    char    *qopClass, *nonce;

    if (logMsg == 0) {
        logMsg = msg;
    }
    mprLog(3, "Auth response: code %d, %s", code, logMsg);

    if (auth->type == HTTP_AUTH_BASIC) {
        httpSetHeader(conn, "WWW-Authenticate", "Basic realm=\"%s\"", auth->requiredRealm);

    } else if (auth->type == HTTP_AUTH_DIGEST) {
        qopClass = auth->qop;
        nonce = createDigestNonce(conn, conn->http->secret, auth->requiredRealm);
        mprAssert(conn->host);

        if (scmp(qopClass, "auth") == 0) {
            httpSetHeader(conn, "WWW-Authenticate", "Digest realm=\"%s\", domain=\"%s\", "
                "qop=\"auth\", nonce=\"%s\", opaque=\"%s\", algorithm=\"MD5\", stale=\"FALSE\"", 
                auth->requiredRealm, conn->host->name, nonce, "");

        } else if (scmp(qopClass, "auth-int") == 0) {
            httpSetHeader(conn, "WWW-Authenticate", "Digest realm=\"%s\", domain=\"%s\", "
                "qop=\"auth\", nonce=\"%s\", opaque=\"%s\", algorithm=\"MD5\", stale=\"FALSE\"", 
                auth->requiredRealm, conn->host->name, nonce, "");

        } else {
            httpSetHeader(conn, "WWW-Authenticate", "Digest realm=\"%s\", nonce=\"%s\"", auth->requiredRealm, nonce);
        }
    }
    httpSetContentType(conn, "text/plain");
    httpError(conn, code, "Authentication Error: %s", msg);
    httpSetPipelineHandler(conn, conn->http->passHandler);
}


/*
    Create a nonce value for digest authentication (RFC 2617)
 */ 
static char *createDigestNonce(HttpConn *conn, cchar *secret, cchar *realm)
{
    MprTime      now;
    char         nonce[256];
    static int64 next = 0;

    mprAssert(realm && *realm);

    now = conn->http->now;
    mprSprintf(nonce, sizeof(nonce), "%s:%s:%Lx:%Lx", secret, realm, now, next++);
    return mprEncode64(nonce);
}


static int parseDigestNonce(char *nonce, cchar **secret, cchar **realm, MprTime *when)
{
    char    *tok, *decoded, *whenStr;

    if ((decoded = mprDecode64(nonce)) == 0) {
        return MPR_ERR_CANT_READ;
    }
    *secret = stok(decoded, ":", &tok);
    *realm = stok(NULL, ":", &tok);
    whenStr = stok(NULL, ":", &tok);
    *when = (MprTime) stoiradix(whenStr, 16, NULL); 
    return 0;
}


/*
    Get a Digest value using the MD5 algorithm -- See RFC 2617 to understand this code.
 */ 
static int calcDigest(char **digest, cchar *userName, cchar *password, cchar *realm, cchar *uri, 
    cchar *nonce, cchar *qop, cchar *nc, cchar *cnonce, cchar *method)
{
    char    a1Buf[256], a2Buf[256], digestBuf[256];
    char    *ha1, *ha2;

    mprAssert(qop);

    /*
        Compute HA1. If userName == 0, then the password is already expected to be in the HA1 format 
        (MD5(userName:realm:password).
     */
    if (userName == 0) {
        ha1 = sclone(password);
    } else {
        mprSprintf(a1Buf, sizeof(a1Buf), "%s:%s:%s", userName, realm, password);
        ha1 = mprGetMD5(a1Buf);
    }

    /*
        HA2
     */ 
    mprSprintf(a2Buf, sizeof(a2Buf), "%s:%s", method, uri);
    ha2 = mprGetMD5(a2Buf);

    /*
        H(HA1:nonce:HA2)
     */
    if (scmp(qop, "auth") == 0) {
        mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%s:%s:%s:%s", ha1, nonce, nc, cnonce, qop, ha2);

    } else if (scmp(qop, "auth-int") == 0) {
        mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%s:%s:%s:%s", ha1, nonce, nc, cnonce, qop, ha2);

    } else {
        mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%s", ha1, nonce, ha2);
    }
    *digest = mprGetMD5(digestBuf);
    return 0;
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
