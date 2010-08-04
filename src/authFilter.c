/*
    authFilter.c - Authorization filter for basic and digest authentication.
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

static int calcDigest(MprCtx ctx, char **digest, cchar *userName, cchar *password, cchar *realm, cchar *uri, 
    cchar *nonce, cchar *qop, cchar *nc, cchar *cnonce, cchar *method);
static char *createDigestNonce(MprCtx ctx, cchar *secret, cchar *etag, cchar *realm);
static void decodeBasicAuth(HttpConn *conn, AuthData *ad);
static int  decodeDigestDetails(HttpConn *conn, AuthData *ad);
static void formatAuthResponse(HttpConn *conn, HttpAuth *auth, int code, char *msg, char *logMsg);
static bool matchAuth(HttpConn *conn, HttpStage *handler);
static int parseDigestNonce(MprCtx ctx, char *nonce, cchar **secret, cchar **etag, cchar **realm, MprTime *when);

/*********************************** Code *************************************/

int httpOpenAuthFilter(Http *http)
{
    HttpStage     *filter;

    filter = httpCreateFilter(http, "authFilter", HTTP_STAGE_ALL);
    if (filter == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    http->authFilter = filter;
    filter->match = matchAuth; 
    return 0;
}


static bool matchAuth(HttpConn *conn, HttpStage *handler)
{
    Http        *http;
    HttpRx      *rec;
    HttpTx      *trans;
    HttpAuth    *auth;
    AuthData    *ad;
    MprTime     when;
    cchar       *requiredPassword;
    char        *msg, *requiredDigest;
    cchar       *secret, *etag, *realm;
    int         actualAuthType;

    rec = conn->rx;
    trans = conn->tx;
    http = conn->http;
    auth = rec->auth;

    if (!conn->server || auth == 0 || auth->type == 0) {
        return 0;
    }
    if ((ad = mprAllocObjZeroed(rec, AuthData)) == 0) {
        return 1;
    }
#if UNUSED
    if (auth == 0) {
        httpError(conn, HTTP_CODE_UNAUTHORIZED, "Access Denied, Authorization not enabled");
        return 1;
    }
    if (auth->type == 0) {
        formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access Denied, Authorization required.", 0);
        return 1;
    }
#endif
    if (rec->authDetails == 0) {
        formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access Denied, Missing authorization details.", 0);
        return 1;
    }
    if (mprStrcmpAnyCase(rec->authType, "basic") == 0) {
        decodeBasicAuth(conn, ad);
        actualAuthType = HTTP_AUTH_BASIC;

    } else if (mprStrcmpAnyCase(rec->authType, "digest") == 0) {
        if (decodeDigestDetails(conn, ad) < 0) {
            httpError(conn, 400, "Bad authorization header");
            return 1;
        }
        actualAuthType = HTTP_AUTH_DIGEST;
    } else {
        actualAuthType = HTTP_AUTH_UNKNOWN;
    }
    mprLog(conn, 4, "run: type %d, url %s\nDetails %s\n", auth->type, rec->pathInfo, rec->authDetails);

    if (ad->userName == 0) {
        formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access Denied, Missing user name", 0);
        return 1;
    }
    if (auth->type != actualAuthType) {
        formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access Denied, Wrong authentication protocol", 0);
        return 1;
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
        if (strcmp(ad->qop, auth->qop) != 0) {
            formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access Denied. Protection quality does not match", 0);
            return 1;
        }
        calcDigest(rec, &requiredDigest, 0, requiredPassword, ad->realm, rec->pathInfo, ad->nonce, ad->qop, ad->nc, 
            ad->cnonce, rec->method);
        requiredPassword = requiredDigest;

        /*
            Validate the nonce value - prevents replay attacks
         */
        when = 0; secret = 0; etag = 0; realm = 0;
        parseDigestNonce(conn, ad->nonce, &secret, &etag, &realm, &when);
        if (strcmp(secret, http->secret) != 0 || strcmp(etag, trans->etag) != 0 || strcmp(realm, auth->requiredRealm) != 0) {
            formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access denied, authentication error", "Nonce mismatch");
        } else if ((when + (5 * 60 * MPR_TICKS_PER_SEC)) < http->now) {
            formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access denied, authentication error", "Nonce is stale");
        }
    }
    if (!(http->validateCred)(auth, auth->requiredRealm, ad->userName, ad->password, requiredPassword, &msg)) {
        formatAuthResponse(conn, auth, HTTP_CODE_UNAUTHORIZED, "Access denied, authentication error", msg);
    }
    return 1;
}


/*  
    Decode basic authorization details
 */
static void decodeBasicAuth(HttpConn *conn, AuthData *ad)
{
    HttpRx    *rec;
    char            *decoded, *cp;

    rec = conn->rx;
    if ((decoded = mprDecode64(conn, rec->authDetails)) == 0) {
        return;
    }
    if ((cp = strchr(decoded, ':')) != 0) {
        *cp++ = '\0';
    }
    if (cp) {
        ad->userName = mprStrdup(rec, decoded);
        ad->password = mprStrdup(rec, cp);

    } else {
        ad->userName = mprStrdup(rec, "");
        ad->password = mprStrdup(rec, "");
    }
    httpSetAuthUser(conn, ad->userName);
    mprFree(decoded);
}


/*  
    Decode the digest authentication details.
 */
static int decodeDigestDetails(HttpConn *conn, AuthData *ad)
{
    HttpRx    *rec;
    char            *authDetails, *value, *tok, *key, *dp, *sp;
    int             seenComma;

    rec = conn->rx;
    key = authDetails = mprStrdup(rec, rec->authDetails);

    while (*key) {
        while (*key && isspace((int) *key)) {
            key++;
        }
        tok = key;
        while (*tok && !isspace((int) *tok) && *tok != ',' && *tok != '=') {
            tok++;
        }
        *tok++ = '\0';

        while (isspace((int) *tok)) {
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
        switch (tolower((int) *key)) {
        case 'a':
            if (mprStrcmpAnyCase(key, "algorithm") == 0) {
                break;
            } else if (mprStrcmpAnyCase(key, "auth-param") == 0) {
                break;
            }
            break;

        case 'c':
            if (mprStrcmpAnyCase(key, "cnonce") == 0) {
                ad->cnonce = mprStrdup(rec, value);
            }
            break;

        case 'd':
            if (mprStrcmpAnyCase(key, "domain") == 0) {
                break;
            }
            break;

        case 'n':
            if (mprStrcmpAnyCase(key, "nc") == 0) {
                ad->nc = mprStrdup(rec, value);
            } else if (mprStrcmpAnyCase(key, "nonce") == 0) {
                ad->nonce = mprStrdup(rec, value);
            }
            break;

        case 'o':
            if (mprStrcmpAnyCase(key, "opaque") == 0) {
                ad->opaque = mprStrdup(rec, value);
            }
            break;

        case 'q':
            if (mprStrcmpAnyCase(key, "qop") == 0) {
                ad->qop = mprStrdup(rec, value);
            }
            break;

        case 'r':
            if (mprStrcmpAnyCase(key, "realm") == 0) {
                ad->realm = mprStrdup(rec, value);
            } else if (mprStrcmpAnyCase(key, "response") == 0) {
                /* Store the response digest in the password field */
                ad->password = mprStrdup(rec, value);
            }
            break;

        case 's':
            if (mprStrcmpAnyCase(key, "stale") == 0) {
                break;
            }
        
        case 'u':
            if (mprStrcmpAnyCase(key, "uri") == 0) {
                ad->uri = mprStrdup(rec, value);
            } else if (mprStrcmpAnyCase(key, "user") == 0) {
                ad->userName = mprStrdup(rec, value);
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
    mprFree(authDetails);
    if (ad->userName == 0 || ad->realm == 0 || ad->nonce == 0 || ad->uri == 0 || ad->password == 0) {
        return MPR_ERR_BAD_ARGS;
    }
    if (ad->qop && (ad->cnonce == 0 || ad->nc == 0)) {
        return MPR_ERR_BAD_ARGS;
    }
    if (ad->qop == 0) {
        ad->qop = mprStrdup(rec, "");
    }
    httpSetAuthUser(conn, ad->userName);
    return 0;
}


/*  
    Format an authentication response. This is typically a 401 response code.
 */
static void formatAuthResponse(HttpConn *conn, HttpAuth *auth, int code, char *msg, char *logMsg)
{
    HttpRx  *rec;
    HttpTx  *trans;
    char    *qopClass, *nonce, *etag;

    rec = conn->rx;
    trans = conn->tx;
    if (logMsg == 0) {
        logMsg = msg;
    }
    mprLog(conn, 3, "Auth response: code %d, %s", code, logMsg);

    if (auth->type == HTTP_AUTH_BASIC) {
        httpSetHeader(conn, "WWW-Authenticate", "Basic realm=\"%s\"", auth->requiredRealm);

    } else if (auth->type == HTTP_AUTH_DIGEST) {
        qopClass = auth->qop;
        etag = trans->etag ? trans->etag : "";
        nonce = createDigestNonce(conn, conn->http->secret, etag, auth->requiredRealm);

        if (strcmp(qopClass, "auth") == 0) {
            httpSetHeader(conn, "WWW-Authenticate", "Digest realm=\"%s\", domain=\"%s\", "
                "qop=\"auth\", nonce=\"%s\", opaque=\"%s\", algorithm=\"MD5\", stale=\"FALSE\"", 
                auth->requiredRealm, conn->server->name, nonce, etag);

        } else if (strcmp(qopClass, "auth-int") == 0) {
            httpSetHeader(conn, "WWW-Authenticate", "Digest realm=\"%s\", domain=\"%s\", "
                "qop=\"auth\", nonce=\"%s\", opaque=\"%s\", algorithm=\"MD5\", stale=\"FALSE\"", 
                auth->requiredRealm, conn->server->name, nonce, etag);

        } else {
            httpSetHeader(conn, "WWW-Authenticate", "Digest realm=\"%s\", nonce=\"%s\"", auth->requiredRealm, nonce);
        }
        mprFree(nonce);
    }
    httpError(conn, code, "Authentication Error: %s", msg);
    httpSetPipeHandler(conn, conn->http->passHandler);
}


/*
    Create a nonce value for digest authentication (RFC 2617)
 */ 
static char *createDigestNonce(MprCtx ctx, cchar *secret, cchar *etag, cchar *realm)
{
    MprTime     now;
    char        nonce[256];

    mprAssert(realm && *realm);

    now = mprGetTime(ctx);
    mprSprintf(ctx, nonce, sizeof(nonce), "%s:%s:%s:%Lx", secret, etag, realm, now);
    return mprEncode64(ctx, nonce);
}


static int parseDigestNonce(MprCtx ctx, char *nonce, cchar **secret, cchar **etag, cchar **realm, MprTime *when)
{
    char    *tok, *decoded, *whenStr;

    if ((decoded = mprDecode64(ctx, nonce)) == 0) {
        return MPR_ERR_CANT_READ;
    }
    *secret = mprStrTok(decoded, ":", &tok);
    *etag = mprStrTok(NULL, ":", &tok);
    *realm = mprStrTok(NULL, ":", &tok);
    whenStr = mprStrTok(NULL, ":", &tok);
    *when = (MprTime) mprAtoi(whenStr, 16);
    return 0;
}


static char *md5(MprCtx ctx, cchar *string)
{
    return mprGetMD5Hash(ctx, string, (int) strlen(string), NULL);
}


/*
    Get a Digest value using the MD5 algorithm -- See RFC 2617 to understand this code.
 */ 
static int calcDigest(MprCtx ctx, char **digest, cchar *userName, cchar *password, cchar *realm, cchar *uri, 
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
        ha1 = mprStrdup(ctx, password);
    } else {
        mprSprintf(ctx, a1Buf, sizeof(a1Buf), "%s:%s:%s", userName, realm, password);
        ha1 = md5(ctx, a1Buf);
    }

    /*
        HA2
     */ 
    mprSprintf(ctx, a2Buf, sizeof(a2Buf), "%s:%s", method, uri);
    ha2 = md5(ctx, a2Buf);

    /*
        H(HA1:nonce:HA2)
     */
    if (strcmp(qop, "auth") == 0) {
        mprSprintf(ctx, digestBuf, sizeof(digestBuf), "%s:%s:%s:%s:%s:%s", ha1, nonce, nc, cnonce, qop, ha2);

    } else if (strcmp(qop, "auth-int") == 0) {
        mprSprintf(ctx, digestBuf, sizeof(digestBuf), "%s:%s:%s:%s:%s:%s", ha1, nonce, nc, cnonce, qop, ha2);

    } else {
        mprSprintf(ctx, digestBuf, sizeof(digestBuf), "%s:%s:%s", ha1, nonce, ha2);
    }
    *digest = md5(ctx, digestBuf);
    mprFree(ha1);
    mprFree(ha2);
    return 0;
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
