/*
    client.c -- Client side specific support.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/*********************************** Code *************************************/

static HttpConn *openConnection(HttpConn *conn, cchar *url, struct MprSsl *ssl)
{
    Http        *http;
    HttpUri     *uri;
    MprSocket   *sp;
    char        *ip;
    int         port, rc, level;

    mprAssert(conn);

    http = conn->http;
    uri = httpCreateUri(url, HTTP_COMPLETE_URI);
    conn->tx->parsedUri = uri;

    if (uri->secure) {
#if BIT_FEATURE_SSL
        if (!http->sslLoaded) {
            if (!mprLoadSsl(0)) {
                mprError("Can't load SSL provider");
                return 0;
            }
            http->sslLoaded = 1;
        }
#else
        httpError(conn, HTTP_CODE_COMMS_ERROR, "SSL communications support not included in build");
        return 0;
#endif
    }
    if (*url == '/') {
        ip = (http->proxyHost) ? http->proxyHost : http->defaultClientHost;
        port = (http->proxyHost) ? http->proxyPort : http->defaultClientPort;
    } else {
        ip = (http->proxyHost) ? http->proxyHost : uri->host;
        port = (http->proxyHost) ? http->proxyPort : uri->port;
    }
    if (port == 0) {
        port = (uri->secure) ? 443 : 80;
    }
    if (conn && conn->sock) {
        if (--conn->keepAliveCount < 0 || port != conn->port || strcmp(ip, conn->ip) != 0) {
            httpCloseConn(conn);
        } else {
            mprLog(4, "Http: reusing keep-alive socket on: %s:%d", ip, port);
        }
    }
    if (conn->sock) {
        return conn;
    }
    if ((sp = mprCreateSocket((uri->secure) ? MPR_SECURE_CLIENT: NULL)) == 0) {
        httpError(conn, HTTP_CODE_COMMS_ERROR, "Can't create socket for %s", url);
        return 0;
    }
#if BIT_FEATURE_SSL
    if (uri->secure && ssl) {
        mprSetSocketSslConfig(sp, ssl);
    }
#endif
    if ((rc = mprConnectSocket(sp, ip, port, 0)) < 0) {
        httpError(conn, HTTP_CODE_COMMS_ERROR, "Can't open socket on %s:%d", ip, port);
        return 0;
    }
    conn->sock = sp;
    conn->ip = sclone(ip);
    conn->port = port;
    conn->secure = uri->secure;
    conn->keepAliveCount = (conn->limits->keepAliveMax) ? conn->limits->keepAliveMax : -1;

    if ((level = httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_CONN, NULL)) >= 0) {
        mprLog(level, "### Outgoing connection from %s:%d to %s:%d", 
            conn->ip, conn->port, conn->sock->ip, conn->sock->port);
    }
    return conn;
}


/*  
    Define headers and create an empty header packet that will be filled later by the pipeline.
 */
static int setClientHeaders(HttpConn *conn)
{
    Http        *http;
    HttpTx      *tx;
    HttpUri     *parsedUri;
    char        *encoded;

    mprAssert(conn);

    http = conn->http;
    tx = conn->tx;
    parsedUri = tx->parsedUri;
    if (conn->authType && strcmp(conn->authType, "basic") == 0) {
        char    abuf[MPR_MAX_STRING];
        mprSprintf(abuf, sizeof(abuf), "%s:%s", conn->authUser, conn->authPassword);
        encoded = mprEncode64(abuf);
        httpAddHeader(conn, "Authorization", "basic %s", encoded);
        conn->sentCredentials = 1;

    } else if (conn->authType && strcmp(conn->authType, "digest") == 0) {
        char    a1Buf[256], a2Buf[256], digestBuf[256];
        char    *ha1, *ha2, *digest, *qop;
        if (http->secret == 0 && httpCreateSecret(http) < 0) {
            mprLog(MPR_ERROR, "Http: Can't create secret for digest authentication");
            return MPR_ERR_CANT_CREATE;
        }
        conn->authCnonce = sfmt("%s:%s:%x", http->secret, conn->authRealm, (int) http->now);

        mprSprintf(a1Buf, sizeof(a1Buf), "%s:%s:%s", conn->authUser, conn->authRealm, conn->authPassword);
        ha1 = mprGetMD5(a1Buf);
        mprSprintf(a2Buf, sizeof(a2Buf), "%s:%s", tx->method, parsedUri->path);
        ha2 = mprGetMD5(a2Buf);
        qop = (conn->authQop) ? conn->authQop : (char*) "";

        conn->authNc++;
        if (scasecmp(conn->authQop, "auth") == 0) {
            mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%08x:%s:%s:%s",
                ha1, conn->authNonce, conn->authNc, conn->authCnonce, conn->authQop, ha2);
        } else if (scasecmp(conn->authQop, "auth-int") == 0) {
            mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%08x:%s:%s:%s",
                ha1, conn->authNonce, conn->authNc, conn->authCnonce, conn->authQop, ha2);
        } else {
            qop = "";
            mprSprintf(digestBuf, sizeof(digestBuf), "%s:%s:%s", ha1, conn->authNonce, ha2);
        }
        digest = mprGetMD5(digestBuf);

        if (*qop == '\0') {
            httpAddHeader(conn, "Authorization", "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", "
                "uri=\"%s\", response=\"%s\"",
                conn->authUser, conn->authRealm, conn->authNonce, parsedUri->path, digest);

        } else if (strcmp(qop, "auth") == 0) {
            httpAddHeader(conn, "Authorization", "Digest username=\"%s\", realm=\"%s\", domain=\"%s\", "
                "algorithm=\"MD5\", qop=\"%s\", cnonce=\"%s\", nc=\"%08x\", nonce=\"%s\", opaque=\"%s\", "
                "stale=\"FALSE\", uri=\"%s\", response=\"%s\"",
                conn->authUser, conn->authRealm, conn->authDomain, conn->authQop, conn->authCnonce, conn->authNc,
                conn->authNonce, conn->authOpaque, parsedUri->path, digest);

        } else if (strcmp(qop, "auth-int") == 0) {
            ;
        }
        conn->sentCredentials = 1;
    }
    if (conn->port != 80) {
        httpAddHeader(conn, "Host", "%s:%d", conn->ip, conn->port);
    } else {
        httpAddHeaderString(conn, "Host", conn->ip);
    }
    if (strcmp(conn->protocol, "HTTP/1.1") == 0) {
        /* If zero, we ask the client to close one request early. This helps with client led closes */
        if (conn->keepAliveCount > 0) {
            httpSetHeaderString(conn, "Connection", "Keep-Alive");
        } else {
            httpSetHeaderString(conn, "Connection", "close");
        }

    } else {
        /* Set to zero to let the client initiate the close */
        conn->keepAliveCount = 0;
        httpSetHeaderString(conn, "Connection", "close");
    }
    return 0;
}


int httpConnect(HttpConn *conn, cchar *method, cchar *url, struct MprSsl *ssl)
{
    mprAssert(conn);
    mprAssert(method && *method);
    mprAssert(url && *url);

    if (conn->endpoint) {
        httpError(conn, HTTP_CODE_BAD_GATEWAY, "Can't call connect in a server");
        return MPR_ERR_BAD_STATE;
    }
    mprLog(4, "Http: client request: %s %s", method, url);

    if (conn->tx == 0 || conn->state != HTTP_STATE_BEGIN) {
        /* WARNING: this will erase headers */
        httpPrepClientConn(conn, 0);
    }
    mprAssert(conn->state == HTTP_STATE_BEGIN);
    httpSetState(conn, HTTP_STATE_CONNECTED);
    conn->sentCredentials = 0;
    conn->tx->method = supper(method);

#if BIT_DEBUG
    conn->startTime = conn->http->now;
    conn->startTicks = mprGetTicks();
#endif
    if (openConnection(conn, url, ssl) == 0) {
        return MPR_ERR_CANT_OPEN;
    }
    httpCreateTxPipeline(conn, conn->http->clientRoute);
    if (setClientHeaders(conn) < 0) {
        return MPR_ERR_CANT_INITIALIZE;
    }
    return 0;
}


/*  
    Check the response for authentication failures and redirections. Return true if a retry is requried.
 */
bool httpNeedRetry(HttpConn *conn, char **url)
{
    HttpRx      *rx;

    mprAssert(conn->rx);

    *url = 0;
    rx = conn->rx;

    if (conn->state < HTTP_STATE_FIRST) {
        return 0;
    }
    if (rx->status == HTTP_CODE_UNAUTHORIZED) {
        if (conn->authUser == 0) {
            httpFormatError(conn, rx->status, "Authentication required");
        } else if (conn->sentCredentials) {
            httpFormatError(conn, rx->status, "Authentication failed");
        } else {
            return 1;
        }
    } else if (HTTP_CODE_MOVED_PERMANENTLY <= rx->status && rx->status <= HTTP_CODE_MOVED_TEMPORARILY && 
            conn->followRedirects) {
        if (rx->redirect) {
            *url = rx->redirect;
            return 1;
        }
        httpFormatError(conn, rx->status, "Missing location header");
        return -1;
    }
    return 0;
}


/*  
    Set the request as being a multipart mime upload. This defines the content type and defines a multipart mime boundary
 */
void httpEnableUpload(HttpConn *conn)
{
    conn->boundary = sfmt("--BOUNDARY--%Ld", conn->http->now);
    httpSetHeader(conn, "Content-Type", "multipart/form-data; boundary=%s", &conn->boundary[2]);
}


static int blockingFileCopy(HttpConn *conn, cchar *path)
{
    MprFile     *file;
    char        buf[MPR_BUFSIZE];
    ssize       bytes, nbytes, offset;
    int         oldMode;

    file = mprOpenFile(path, O_RDONLY | O_BINARY, 0);
    if (file == 0) {
        mprError("Can't open %s", path);
        return MPR_ERR_CANT_OPEN;
    }
    mprAddRoot(file);
    oldMode = mprSetSocketBlockingMode(conn->sock, 1);
    while ((bytes = mprReadFile(file, buf, sizeof(buf))) > 0) {
        offset = 0;
        while (bytes > 0) {
            if ((nbytes = httpWriteBlock(conn->writeq, &buf[offset], bytes)) < 0) {
                mprCloseFile(file);
                mprRemoveRoot(file);
                return MPR_ERR_CANT_WRITE;
            }
            bytes -= nbytes;
            offset += nbytes;
            mprAssert(bytes >= 0);
        }
        mprYield(0);
    }
    httpFlushQueue(conn->writeq, 1);
    mprSetSocketBlockingMode(conn->sock, oldMode);
    mprCloseFile(file);
    mprRemoveRoot(file);
    return 0;
}


/*  
    Write upload data. This routine blocks. If you need non-blocking ... cut and paste.
 */
ssize httpWriteUploadData(HttpConn *conn, MprList *fileData, MprList *formData)
{
    char    *path, *pair, *key, *value, *name;
    ssize   rc;
    int     next;

    rc = 0;
    if (formData) {
        for (rc = next = 0; rc >= 0 && (pair = mprGetNextItem(formData, &next)) != 0; ) {
            key = stok(sclone(pair), "=", &value);
            rc += httpWrite(conn->writeq, "%s\r\nContent-Disposition: form-data; name=\"%s\";\r\n", conn->boundary, key);
            rc += httpWrite(conn->writeq, "Content-Type: application/x-www-form-urlencoded\r\n\r\n%s\r\n", value);
        }
    }
    if (fileData) {
        for (rc = next = 0; rc >= 0 && (path = mprGetNextItem(fileData, &next)) != 0; ) {
            name = mprGetPathBase(path);
            rc += httpWrite(conn->writeq, "%s\r\nContent-Disposition: form-data; name=\"file%d\"; filename=\"%s\"\r\n", 
                conn->boundary, next - 1, name);
            rc += httpWrite(conn->writeq, "Content-Type: %s\r\n\r\n", mprLookupMime(MPR->mimeTypes, path));
            rc += blockingFileCopy(conn, path);
            rc += httpWrite(conn->writeq, "\r\n");
        }
    }
    rc += httpWrite(conn->writeq, "%s--\r\n--", conn->boundary);
    httpFinalize(conn);
    return rc;
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
