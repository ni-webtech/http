/*
    digest.c - Digest Authorization

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Defines ***********************************/
/*
    Per-request digest authorization data
 */
typedef struct DigestData 
{
    char    *algorithm;
    char    *cnonce;
    char    *domain;
    char    *nc;
    char    *nonce;
    char    *opaque;
    char    *qop;
    char    *realm;
    char    *stale;
    char    *uri;
} DigestData;

/********************************** Forwards **********************************/

static char *calcDigest(HttpConn *conn, DigestData *dp);
static char *createDigestNonce(HttpConn *conn, cchar *secret, cchar *realm);
static void manageDigestData(DigestData *dp, int flags);
static int parseDigestNonce(char *nonce, cchar **secret, cchar **realm, MprTime *when);

/*********************************** Code *************************************/
/*
    Parse the client 'Authorization' header and the server 'Www-Authenticate' header
 */
int httpDigestParse(HttpConn *conn)
{
    HttpRx      *rx;
    DigestData  *dp;
    MprTime     when;
    char        *value, *tok, *key, *cp, *sp;
    cchar       *secret, *realm;
    int         seenComma;

    dp = conn->authData = mprAllocObj(DigestData, manageDigestData);
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
            for (cp = sp = value; *sp; sp++) {
                if (*sp == '\\') {
                    sp++;
                }
                *cp++ = *sp++;
            }
            *cp = '\0';
        }

        /*
            user, response, oqaque, uri, realm, nonce, nc, cnonce, qop
         */
        switch (tolower((uchar) *key)) {
        case 'a':
            if (scaselesscmp(key, "algorithm") == 0) {
                dp->algorithm = sclone(value);
                break;
            } else if (scaselesscmp(key, "auth-param") == 0) {
                break;
            }
            break;

        case 'c':
            if (scaselesscmp(key, "cnonce") == 0) {
                dp->cnonce = sclone(value);
            }
            break;

        case 'd':
            if (scaselesscmp(key, "domain") == 0) {
                dp->domain = sclone(value);
                break;
            }
            break;

        case 'n':
            if (scaselesscmp(key, "nc") == 0) {
                dp->nc = sclone(value);
            } else if (scaselesscmp(key, "nonce") == 0) {
                dp->nonce = sclone(value);
            }
            break;

        case 'o':
            if (scaselesscmp(key, "opaque") == 0) {
                dp->opaque = sclone(value);
            }
            break;

        case 'q':
            if (scaselesscmp(key, "qop") == 0) {
                dp->qop = sclone(value);
            }
            break;

        case 'r':
            if (scaselesscmp(key, "realm") == 0) {
                dp->realm = sclone(value);
            } else if (scaselesscmp(key, "response") == 0) {
                /* Store the response digest in the password field */
                conn->password = sclone(value);
            }
            break;

        case 's':
            if (scaselesscmp(key, "stale") == 0) {
                break;
            }
        
        case 'u':
            if (scaselesscmp(key, "uri") == 0) {
                dp->uri = sclone(value);
            } else if (scaselesscmp(key, "username") == 0 || scaselesscmp(key, "user") == 0) {
                conn->username = sclone(value);
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
    if (conn->username == 0 || conn->password == 0) {
        return MPR_ERR_BAD_FORMAT;
    }
    if (dp->realm == 0 || dp->nonce == 0 || dp->uri == 0) {
        return MPR_ERR_BAD_FORMAT;
    }
    if (dp->qop && (dp->cnonce == 0 || dp->nc == 0)) {
        return MPR_ERR_BAD_FORMAT;
    }
    if (conn->endpoint) {
        parseDigestNonce(dp->nonce, &secret, &realm, &when);
        if (!smatch(secret, secret)) {
            //  How should this be reported
            mprLog(2, "Access denied: Nonce mismatch\n");
            return MPR_ERR_BAD_STATE;

        } else if (!smatch(realm, rx->route->auth->realm)) {
            mprLog(2, "Access denied: Realm mismatch\n");
            return MPR_ERR_BAD_STATE;

        } else if (dp->qop && !smatch(dp->qop, "auth")) {
            mprLog(2, "Access denied: Bad qop\n");
            return MPR_ERR_BAD_STATE;

        } else if ((when + (5 * 60)) < time(0)) {
            mprLog(2, "Access denied: Nonce is stale\n");
            return MPR_ERR_BAD_STATE;
        }
        rx->passDigest = calcDigest(conn, dp);
    } else {
        if (dp->domain == 0 || dp->opaque == 0 || dp->algorithm == 0 || dp->stale == 0) {
            return MPR_ERR_BAD_FORMAT;
        }
    }
    return 0;
}


static void manageDigestData(DigestData *dp, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(dp->algorithm);
        mprMark(dp->cnonce);
        mprMark(dp->domain);
        mprMark(dp->nc);
        mprMark(dp->nonce);
        mprMark(dp->opaque);
        mprMark(dp->qop);
        mprMark(dp->realm);
        mprMark(dp->stale);
        mprMark(dp->uri);
    }
}


/*
    Respond to the request by asking for a client login
 */
void httpDigestLogin(HttpConn *conn)
{
    HttpAuth    *auth;
    char        *nonce, *opaque;

    auth = conn->rx->route->auth;
    nonce = createDigestNonce(conn, conn->http->secret, auth->realm);
    /* Opaque is unused, set to anything */
    opaque = "799d5";

    if (smatch(auth->qop, "none")) {
        httpSetHeader(conn, "WWW-Authenticate", "Digest realm=\"%s\", nonce=\"%s\"", auth->realm, nonce);
    } else {
        /* Value of null defaults to "auth" */
        httpSetHeader(conn, "WWW-Authenticate", "Digest realm=\"%s\", domain=\"%s\", "
            "qop=\"auth\", nonce=\"%s\", opaque=\"%s\", algorithm=\"MD5\", stale=\"FALSE\"", 
            auth->realm, conn->host->name, nonce, opaque);
    }
    httpSetContentType(conn, "text/plain");
    httpError(conn, HTTP_CODE_UNAUTHORIZED, "Access Denied. Login required");
}


/*
    Add the client 'Authorization' header for authenticated requests
 */
void httpDigestSetHeaders(HttpConn *conn)
{ 
    Http        *http;
    HttpTx      *tx;
    DigestData  *dp;
    char        a1Buf[256], a2Buf[256], digestBuf[256];
    char        *ha1, *ha2, *digest, *cnonce;

    http = conn->http;
    tx = conn->tx;
    dp = conn->authData;

    cnonce = sfmt("%s:%s:%x", http->secret, dp->realm, (int) http->now);
    mprSprintf(a1Buf, sizeof(a1Buf), "%s:%s:%s", conn->username, dp->realm, conn->password);
    ha1 = mprGetMD5(a1Buf);
    mprSprintf(a2Buf, sizeof(a2Buf), "%s:%s", tx->method, tx->parsedUri->path);
    ha2 = mprGetMD5(a2Buf);
#if UNUSED
    //  MOB - why incremented?
    dp->nc++;
#endif
    if (smatch(dp->qop, "auth")) {
        mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%08x:%s:%s:%s", ha1, dp->nonce, dp->nc, cnonce, dp->qop, ha2);
        digest = mprGetMD5(digestBuf);
        httpAddHeader(conn, "Authorization", "Digest username=\"%s\", realm=\"%s\", domain=\"%s\", "
            "algorithm=\"MD5\", qop=\"%s\", cnonce=\"%s\", nc=\"%08x\", nonce=\"%s\", opaque=\"%s\", "
            "stale=\"FALSE\", uri=\"%s\", response=\"%s\"", conn->username, dp->realm, dp->domain, dp->qop, 
            cnonce, dp->nc, dp->nonce, dp->opaque, tx->parsedUri->path, digest);
    } else {
        mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%s", ha1, dp->nonce, ha2);
        digest = mprGetMD5(digestBuf);
        httpAddHeader(conn, "Authorization", "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", "
            "uri=\"%s\", response=\"%s\"", conn->username, dp->realm, dp->nonce, tx->parsedUri->path, digest);
    }
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
static char *calcDigest(HttpConn *conn, DigestData *dp)
{
    HttpAuth    *auth;
    char        abuf[256], digestBuf[256], *ha1, *ha2;

    auth = conn->rx->route->auth;
    if (!conn->user) {
        conn->user = mprLookupKey(auth->users, conn->username);
    }
    mprAssert(conn->user && conn->user->password);
    if (conn->user == 0 || conn->user->password == 0) {
        return 0;
    }

    /*
        Compute HA1. Password is already expected to be in the HA1 format MD5(username:realm:password).
     */
    ha1 = sclone(conn->user->password);

    /*
        HA2
     */ 
    mprSprintf(abuf, sizeof(abuf), "%s:%s", conn->rx->method, dp->uri);
    ha2 = mprGetMD5(abuf);

    /*
        H(HA1:nonce:HA2)
     */
    if (scmp(dp->qop, "auth") == 0) {
        mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%s:%s:%s:%s", ha1, dp->nonce, dp->nc, dp->cnonce, dp->qop, ha2);
    } else {
        mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%s", ha1, dp->nonce, ha2);
    }
    return mprGetMD5(digestBuf);
}

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
