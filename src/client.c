/*
    client.c -- Client side specific support.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/*********************************** Code *************************************/

HttpConn *httpCreateClient(Http *http, MprDispatcher *dispatcher)
{
    HttpConn    *conn;

    conn = httpCreateConn(http, NULL);
    if (dispatcher == NULL) {
        dispatcher = mprGetDispatcher(http);
    }
    conn->dispatcher = dispatcher;
    conn->receiver = httpCreateReceiver(conn);
    conn->transmitter = httpCreateTransmitter(conn, NULL);
    httpCreatePipeline(conn, NULL, NULL);
    return conn;
}


static HttpConn *openConnection(HttpConn *conn, cchar *url)
{
    Http        *http;
    HttpUri     *uri;
    MprSocket   *sp;
    char        *ip;
    int         port, rc;

    mprAssert(conn);

    http = conn->http;
    uri = httpCreateUri(conn, url, 0);

    if (uri->secure) {
#if BLD_FEATURE_SSL
        if (!http->sslLoaded) {
            if (!mprLoadSsl(http, 0)) {
                mprError(http, "Can't load SSL provider");
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
        ip = (http->proxyHost) ? http->proxyHost : http->defaultHost;
        port = (http->proxyHost) ? http->proxyPort : http->defaultPort;
    } else {
        ip = (http->proxyHost) ? http->proxyHost : uri->host;
        port = (http->proxyHost) ? http->proxyPort : uri->port;
    }
    if (port == 0) {
        port = (uri->secure) ? 443 : 80;
    }
    if (conn && conn->sock) {
        if (conn->keepAliveCount < 0 || port != conn->port || strcmp(ip, conn->ip) != 0) {
            httpCloseConn(conn);
        } else {
            mprLog(http, 4, "Http: reusing keep-alive socket on: %s:%d", ip, port);
        }
    }
    if (conn->sock) {
        return conn;
    }
    mprLog(conn, 3, "Http: Opening socket on: %s:%d", ip, port);
    if ((sp = mprCreateSocket(conn, (uri->secure) ? MPR_SECURE_CLIENT: NULL)) == 0) {
        httpError(conn, HTTP_CODE_COMMS_ERROR, "Can't create socket for %s", url);
        mprFree(sp);
        return 0;
    }
    rc = mprOpenClientSocket(sp, ip, port, 0);
    if (rc < 0) {
        httpError(conn, HTTP_CODE_COMMS_ERROR, "Can't open socket on %s:%d", ip, port);
        mprFree(sp);
        return 0;
    }
    conn->sock = sp;
    conn->ip = mprStrdup(conn, ip);
    conn->port = port;
    conn->secure = uri->secure;
    conn->keepAliveCount = (http->maxKeepAlive) ? http->maxKeepAlive : -1;
    return conn;
}


/*  
    Define headers and create an empty header packet that will be filled later by the pipeline.
 */
static HttpPacket *createHeaderPacket(HttpConn *conn)
{
    Http                *http;
    HttpTransmitter     *trans;
    HttpPacket          *packet;
    HttpUri             *parsedUri;
    char                *encoded;
    int                 len, rc;

    mprAssert(conn);

    rc = 0;
    http = conn->http;
    trans = conn->transmitter;
    parsedUri = trans->parsedUri;

    packet = httpCreateHeaderPacket(trans);

    if (conn->authType && strcmp(conn->authType, "basic") == 0) {
        char    abuf[MPR_MAX_STRING];
        mprSprintf(conn, abuf, sizeof(abuf), "%s:%s", conn->authUser, conn->authPassword);
        encoded = mprEncode64(conn, abuf);
        httpAddHeader(conn, "Authorization", "basic %s", encoded);
        mprFree(encoded);
        conn->sentCredentials = 1;

    } else if (conn->authType && strcmp(conn->authType, "digest") == 0) {
        char    a1Buf[256], a2Buf[256], digestBuf[256];
        char    *ha1, *ha2, *digest, *qop;
        if (http->secret == 0 && httpCreateSecret(http) < 0) {
            mprLog(trans, MPR_ERROR, "Http: Can't create secret for digest authentication");
            mprFree(trans);
            conn->transmitter = 0;
            return 0;
        }
        mprFree(conn->authCnonce);
        conn->authCnonce = mprAsprintf(conn, -1, "%s:%s:%x", http->secret, conn->authRealm, (uint) mprGetTime(conn)); 

        mprSprintf(conn, a1Buf, sizeof(a1Buf), "%s:%s:%s", conn->authUser, conn->authRealm, conn->authPassword);
        len = strlen(a1Buf);
        ha1 = mprGetMD5Hash(trans, a1Buf, len, NULL);
        mprSprintf(conn, a2Buf, sizeof(a2Buf), "%s:%s", trans->method, parsedUri->path);
        len = strlen(a2Buf);
        ha2 = mprGetMD5Hash(trans, a2Buf, len, NULL);
        qop = (conn->authQop) ? conn->authQop : (char*) "";

        conn->authNc++;
        if (mprStrcmpAnyCase(conn->authQop, "auth") == 0) {
            mprSprintf(conn, digestBuf, sizeof(digestBuf), "%s:%s:%08x:%s:%s:%s",
                ha1, conn->authNonce, conn->authNc, conn->authCnonce, conn->authQop, ha2);
        } else if (mprStrcmpAnyCase(conn->authQop, "auth-int") == 0) {
            mprSprintf(conn, digestBuf, sizeof(digestBuf), "%s:%s:%08x:%s:%s:%s",
                ha1, conn->authNonce, conn->authNc, conn->authCnonce, conn->authQop, ha2);
        } else {
            qop = "";
            mprSprintf(conn, digestBuf, sizeof(digestBuf), "%s:%s:%s", ha1, conn->authNonce, ha2);
        }
        mprFree(ha1);
        mprFree(ha2);
        digest = mprGetMD5Hash(trans, digestBuf, strlen(digestBuf), NULL);

        if (*qop == '\0') {
            httpAddHeader(conn, "Authorization", "Digest user=\"%s\", realm=\"%s\", nonce=\"%s\", "
                "uri=\"%s\", response=\"%s\"",
                conn->authUser, conn->authRealm, conn->authNonce, parsedUri->path, digest);

        } else if (strcmp(qop, "auth") == 0) {
            httpAddHeader(conn, "Authorization", "Digest user=\"%s\", realm=\"%s\", domain=\"%s\", "
                "algorithm=\"MD5\", qop=\"%s\", cnonce=\"%s\", nc=\"%08x\", nonce=\"%s\", opaque=\"%s\", "
                "stale=\"FALSE\", uri=\"%s\", response=\"%s\"",
                conn->authUser, conn->authRealm, conn->authDomain, conn->authQop, conn->authCnonce, conn->authNc,
                conn->authNonce, conn->authOpaque, parsedUri->path, digest);

        } else if (strcmp(qop, "auth-int") == 0) {
            ;
        }
        mprFree(digest);
        conn->sentCredentials = 1;
    }
    httpAddSimpleHeader(conn, "Host", conn->ip);

    if (strcmp(conn->protocol, "HTTP/1.1") == 0) {
        /* If zero, we ask the client to close one request early. This helps with client led closes */
        if (conn->keepAliveCount > 0) {
            httpSetSimpleHeader(conn, "Connection", "Keep-Alive");
        } else {
            httpSetSimpleHeader(conn, "Connection", "close");
        }
#if UNUSED
        /*
            This code assumes the chunkFilter is always in the outgoing pipeline
         */
        if ((trans->length <= 0) && (strcmp(trans->method, "POST") == 0 || strcmp(trans->method, "PUT") == 0)) {
            if (conn->chunked != 0) {
                httpSetSimpleHeader(conn, "Transfer-Encoding", "chunked");
                conn->chunked = 1;
            }
        } else {
            conn->chunked = 0;
        }
#endif

    } else {
        /* Set to zero to let the client initiate the close */
        conn->keepAliveCount = 0;
        httpSetSimpleHeader(conn, "Connection", "close");
    }
    return packet;
}


int httpConnect(HttpConn *conn, cchar *method, cchar *url)
{
    Http                *http;
    HttpTransmitter     *trans;
    HttpPacket          *headers;

    mprAssert(conn);
    mprAssert(method && *method);
    mprAssert(url && *url);

    if (conn->server) {
        httpSetState(conn, HTTP_STATE_ERROR);
        httpSetState(conn, HTTP_STATE_COMPLETE);
        return MPR_ERR_BAD_STATE;
    }
    mprLog(conn, 4, "Http: client request: %s %s", method, url);

    if (conn->sock) {
        /* 
            Callers requiring retry must call PrepClientConn(conn, HTTP_RETRY_REQUEST) themselves
         */
        httpPrepClientConn(conn, HTTP_NEW_REQUEST);
    }
    http = conn->http;
    trans = conn->transmitter;
    httpSetState(conn, HTTP_STATE_STARTED);
    conn->sentCredentials = 0;

    mprFree(trans->method);
    method = trans->method = mprStrdup(trans, method);
    mprStrUpper(trans->method);
    mprFree(trans->parsedUri);
    trans->parsedUri = httpCreateUri(trans, url, 0);

    if (openConnection(conn, url) == 0) {
        httpSetState(conn, HTTP_STATE_ERROR);
        httpSetState(conn, HTTP_STATE_COMPLETE);
        return MPR_ERR_CANT_OPEN;
    }
    if ((headers = createHeaderPacket(conn)) == 0) {
        httpSetState(conn, HTTP_STATE_ERROR);
        httpSetState(conn, HTTP_STATE_COMPLETE);
        return MPR_ERR_CANT_INITIALIZE;
    }
    mprAssert(conn->writeq);
    httpPutForService(conn->writeq, headers, 0);
    return 0;
}


/*  
    Check the response for authentication failures and redirections. Return true if a retry is requried.
 */
bool httpNeedRetry(HttpConn *conn, char **url)
{
    HttpReceiver    *rec;
    HttpTransmitter *trans;

    mprAssert(conn->receiver);
    mprAssert(conn->state > HTTP_STATE_WAIT);

    rec = conn->receiver;
    trans = conn->transmitter;
    *url = 0;

    if (conn->state < HTTP_STATE_WAIT) {
        return 0;
    }
    if (rec->status == HTTP_CODE_UNAUTHORIZED) {
        if (conn->authUser == 0) {
            httpFormatError(conn, rec->status, "Authentication required");
        } else if (conn->sentCredentials) {
            httpFormatError(conn, rec->status, "Authentication failed");
        } else {
            return 1;
        }
    } else if (HTTP_CODE_MOVED_PERMANENTLY <= rec->status && rec->status <= HTTP_CODE_MOVED_TEMPORARILY && 
            conn->followRedirects) {
        *url = rec->redirect;
        return 1;
    }
    return 0;
}


/*  
    Set the request as being a multipart mime upload. This defines the content type and defines a multipart mime boundary
 */
void httpEnableUpload(HttpConn *conn)
{
    mprFree(conn->boundary);
    conn->boundary = mprAsprintf(conn, -1, "--BOUNDARY--%Ld", conn->http->now);
    httpSetHeader(conn, "Content-Type", "multipart/form-data; boundary=%s", &conn->boundary[2]);
}


static int blockingFileCopy(HttpConn *conn, cchar *path)
{
    MprFile     *file;
    char        buf[MPR_BUFSIZE];
    int         bytes;

    file = mprOpen(conn, path, O_RDONLY | O_BINARY, 0);
    if (file == 0) {
        mprError(conn, "Can't open %s", path);
        return MPR_ERR_CANT_OPEN;
    }
    while ((bytes = mprRead(file, buf, sizeof(buf))) > 0) {
        if (httpWriteBlock(conn->writeq, buf, bytes, 1) != bytes) {
            mprFree(file);
            return MPR_ERR_CANT_WRITE;
        }
    }
    mprFree(file);
    return 0;
}


/*  
    Write upload data. This routine blocks. If you need non-blocking ... cut and paste.
 */
int httpWriteUploadData(HttpConn *conn, MprList *fileData, MprList *formData)
{
    char        *path, *pair, *key, *value, *name;
    int         next, rc;

    rc = 0;

    if (formData) {
        for (rc = next = 0; rc >= 0 && (pair = mprGetNextItem(formData, &next)) != 0; ) {
            key = mprStrTok(mprStrdup(conn, pair), "=", &value);
            rc += httpWrite(conn->writeq, "%s\r\nContent-Disposition: form-data; name=\"%s\";\r\n", conn->boundary, key);
            rc += httpWrite(conn->writeq, "Content-Type: application/x-www-form-urlencoded\r\n\r\n%s\r\n", value);
        }
    }
    if (fileData) {
        for (rc = next = 0; rc >= 0 && (path = mprGetNextItem(fileData, &next)) != 0; ) {
            name = mprGetPathBase(conn, path);
            rc += httpWrite(conn->writeq, "%s\r\nContent-Disposition: form-data; name=\"file%d\"; filename=\"%s\"\r\n", 
                conn->boundary, next - 1, name);
            mprFree(name);
            rc += httpWrite(conn->writeq, "Content-Type: %s\r\n\r\n", mprLookupMimeType(conn->http, path));
            rc += blockingFileCopy(conn, path);
            rc += httpWrite(conn->writeq, "\r\n", value);
        }
    }
    rc += httpWrite(conn->writeq, "%s--\r\n--", conn->boundary);
    httpFinalize(conn);
    return rc;
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
