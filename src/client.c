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
        if (--conn->keepAliveCount < 0 || port != conn->port || strcmp(ip, conn->ip) != 0 || 
                uri->secure != (conn->sock->ssl != 0) || conn->sock->ssl != ssl) {
            httpCloseConn(conn);
        } else {
            mprLog(4, "Http: reusing keep-alive socket on: %s:%d", ip, port);
        }
    }
    if (conn->sock) {
        return conn;
    }
    if ((sp = mprCreateSocket()) == 0) {
        httpError(conn, HTTP_CODE_COMMS_ERROR, "Can't create socket for %s", url);
        return 0;
    }
    if ((rc = mprConnectSocket(sp, ip, port, 0)) < 0) {
        httpError(conn, HTTP_CODE_COMMS_ERROR, "Can't open socket on %s:%d", ip, port);
        return 0;
    }
#if BIT_PACK_SSL
    /* Must be done even if using keep alive for repeat SSL requests */
    if (uri->secure) {
        if (ssl == 0) {
            ssl = mprCreateSsl();
        }
        if (mprUpgradeSocket(sp, ssl, 0) < 0) {
            conn->errorMsg = sp->errorMsg;
            return 0;
        }
    }
#endif
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
    HttpAuthType    *authType;

    mprAssert(conn);

    if (smatch(conn->protocol, "HTTP/1.0")) {
        conn->http10 = 1;
    }
    if (conn->authType && (authType = httpLookupAuthType(conn->authType)) != 0) {
        (authType->setAuth)(conn);
        conn->setCredentials = 1;
    }
    if (conn->port != 80) {
        httpAddHeader(conn, "Host", "%s:%d", conn->ip, conn->port);
    } else {
        httpAddHeaderString(conn, "Host", conn->ip);
    }
#if UNUSED
    if (conn->http10 && !smatch(conn->tx->method, "GET")) {
        conn->keepAliveCount = 0;
    }
#endif
    if (conn->keepAliveCount > 0) {
        httpSetHeaderString(conn, "Connection", "Keep-Alive");
    } else {
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
    conn->setCredentials = 0;
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
    HttpAuthType    *authType;
    HttpRx          *rx;

    mprAssert(conn->rx);

    *url = 0;
    rx = conn->rx;

    if (conn->state < HTTP_STATE_FIRST) {
        return 0;
    }
    if (rx->status == HTTP_CODE_UNAUTHORIZED) {
        if (conn->username == 0) {
            httpFormatError(conn, rx->status, "Authentication required");
        } else if (conn->setCredentials) {
            httpFormatError(conn, rx->status, "Authentication failed");
        } else {
            if (conn->authType && (authType = httpLookupAuthType(conn->authType)) != 0) {
                (authType->parseAuth)(conn);
            }
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
