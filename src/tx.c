/*
    tx.c - Http transmitter for server responses and client requests.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static void manageTx(HttpTx *tx, int flags);

/*********************************** Code *************************************/

HttpTx *httpCreateTx(HttpConn *conn, MprHash *headers)
{
    HttpTx      *tx;

    if ((tx = mprAllocObj(HttpTx, manageTx)) == 0) {
        return 0;
    }
    conn->tx = tx;
    tx->conn = conn;
    tx->status = HTTP_CODE_OK;
    tx->length = -1;
    tx->entityLength = -1;
    tx->traceMethods = HTTP_STAGE_ALL;
    tx->chunkSize = -1;

    tx->queue[HTTP_QUEUE_TX] = httpCreateQueueHead(conn, "TxHead");
    tx->queue[HTTP_QUEUE_RX] = httpCreateQueueHead(conn, "RxHead");

    if (headers) {
        tx->headers = headers;
    } else if ((tx->headers = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS)) != 0) {
        if (!conn->endpoint) {
            httpAddHeaderString(conn, "User-Agent", sclone(HTTP_NAME));
        }
    }
    return tx;
}


void httpDestroyTx(HttpTx *tx)
{
    if (tx->file) {
        mprCloseFile(tx->file);
        tx->file = 0;
    }
    if (tx->conn) {
        tx->conn->tx = 0;
        tx->conn = 0;
    }
}


static void manageTx(HttpTx *tx, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(tx->ext);
        mprMark(tx->etag);
        mprMark(tx->filename);
        mprMark(tx->handler);
        mprMark(tx->parsedUri);
        mprMark(tx->method);
        mprMark(tx->conn);
        mprMark(tx->outputPipeline);
        mprMark(tx->connector);
        mprMark(tx->queue[0]);
        mprMark(tx->queue[1]);
        mprMark(tx->headers);
        mprMark(tx->cache);
        mprMark(tx->cacheBuffer);
        mprMark(tx->cachedContent);
        mprMark(tx->outputRanges);
        mprMark(tx->currentRange);
        mprMark(tx->rangeBoundary);
        mprMark(tx->altBody);
        mprMark(tx->file);

    } else if (flags & MPR_MANAGE_FREE) {
        httpDestroyTx(tx);
    }
}


/*
    Add key/value to the header hash. If already present, update the value
*/
static void addHdr(HttpConn *conn, cchar *key, cchar *value)
{
    mprAssert(key && *key);
    mprAssert(value);

    mprAddKey(conn->tx->headers, key, value);
}


int httpRemoveHeader(HttpConn *conn, cchar *key)
{
    mprAssert(key && *key);
    if (conn->tx == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    return mprRemoveKey(conn->tx->headers, key);
}


/*  
    Add a http header if not already defined
 */
void httpAddHeader(HttpConn *conn, cchar *key, cchar *fmt, ...)
{
    char        *value;
    va_list     vargs;

    mprAssert(key && *key);
    mprAssert(fmt && *fmt);

    va_start(vargs, fmt);
    value = sfmtv(fmt, vargs);
    va_end(vargs);

    if (!mprLookupKey(conn->tx->headers, key)) {
        addHdr(conn, key, value);
    }
}


/*
    Add a header string if not already defined
 */
void httpAddHeaderString(HttpConn *conn, cchar *key, cchar *value)
{
    mprAssert(key && *key);
    mprAssert(value);

    if (!mprLookupKey(conn->tx->headers, key)) {
        addHdr(conn, key, sclone(value));
    }
}


/* 
   Append a header. If already defined, the value is catenated to the pre-existing value after a ", " separator.
   As per the HTTP/1.1 spec.
 */
void httpAppendHeader(HttpConn *conn, cchar *key, cchar *fmt, ...)
{
    va_list     vargs;
    char        *value;
    cchar       *oldValue;

    mprAssert(key && *key);
    mprAssert(fmt && *fmt);

    va_start(vargs, fmt);
    value = sfmtv(fmt, vargs);
    va_end(vargs);

    oldValue = mprLookupKey(conn->tx->headers, key);
    if (oldValue) {
        /*
            Set-Cookie has legacy behavior and some browsers require separate headers
         */
        if (scaselessmatch(key, "Set-Cookie")) {
            mprAddDuplicateKey(conn->tx->headers, key, value);
        } else {
            addHdr(conn, key, sfmt("%s, %s", oldValue, value));
        }
    } else {
        addHdr(conn, key, value);
    }
}


/* 
   Append a header string. If already defined, the value is catenated to the pre-existing value after a ", " separator.
   As per the HTTP/1.1 spec.
 */
void httpAppendHeaderString(HttpConn *conn, cchar *key, cchar *value)
{
    cchar   *oldValue;

    mprAssert(key && *key);
    mprAssert(value && *value);

    oldValue = mprLookupKey(conn->tx->headers, key);
    if (oldValue) {
        if (scaselessmatch(key, "Set-Cookie")) {
            mprAddDuplicateKey(conn->tx->headers, key, sclone(value));
        } else {
            addHdr(conn, key, sfmt("%s, %s", oldValue, value));
        }
    } else {
        addHdr(conn, key, sclone(value));
    }
}


/*  
    Set a http header. Overwrite if present.
 */
void httpSetHeader(HttpConn *conn, cchar *key, cchar *fmt, ...)
{
    char        *value;
    va_list     vargs;

    mprAssert(key && *key);
    mprAssert(fmt && *fmt);

    va_start(vargs, fmt);
    value = sfmtv(fmt, vargs);
    va_end(vargs);
    addHdr(conn, key, value);
}


void httpSetHeaderString(HttpConn *conn, cchar *key, cchar *value)
{
    mprAssert(key && *key);
    mprAssert(value);

    addHdr(conn, key, sclone(value));
}


/*
    Called by connectors (ONLY) when writing the transmission is complete
 */
void httpConnectorComplete(HttpConn *conn)
{
    conn->connectorComplete = 1;
    conn->finalized = 1;
}


void httpFinalize(HttpConn *conn)
{
    if (conn->finalized) {
        return;
    }
    conn->responded = 1;
    conn->finalized = 1;

    if (conn->state >= HTTP_STATE_CONNECTED && conn->writeq && conn->sock) {
        httpPutForService(conn->writeq, httpCreateEndPacket(), HTTP_SCHEDULE_QUEUE);
        httpServiceQueues(conn);
        if (conn->state >= HTTP_STATE_READY && !conn->inHttpProcess) {
            httpPump(conn, NULL);
        }
        conn->refinalize = 0;
    } else {
        /* Pipeline has not been setup yet */
        conn->refinalize = 1;
    }
}


int httpIsFinalized(HttpConn *conn)
{
    return conn->finalized;
}


/*
    Flush the write queue
 */
void httpFlush(HttpConn *conn)
{
    httpFlushQueue(conn->writeq, !conn->async);
}


/*
    This formats a response and sets the altBody. The response is not HTML escaped.
    This is the lowest level for formatResponse.
 */
ssize httpFormatResponsev(HttpConn *conn, cchar *fmt, va_list args)
{
    HttpTx      *tx;
    char        *body;

    tx = conn->tx;
    conn->responded = 1;
    body = sfmtv(fmt, args);
    tx->altBody = body;
    tx->length = slen(tx->altBody);
    conn->tx->flags |= HTTP_TX_NO_BODY;
    httpDiscardData(conn, HTTP_QUEUE_TX);
    return (ssize) tx->length;
}


/*
    This formats a response and sets the altBody. The response is not HTML escaped.
 */
ssize httpFormatResponse(HttpConn *conn, cchar *fmt, ...)
{
    va_list     args;
    ssize       rc;

    va_start(args, fmt);
    rc = httpFormatResponsev(conn, fmt, args);
    va_end(args);
    return rc;
}


/*
    This formats a complete response. Depending on the Accept header, the response will be either HTML or plain text.
    The response is not HTML escaped.  This calls httpFormatResponse.
 */
ssize httpFormatResponseBody(HttpConn *conn, cchar *title, cchar *fmt, ...)
{
    va_list     args;
    char        *msg, *body;

    va_start(args, fmt);
    body = sfmtv(fmt, args);
    if (scmp(conn->rx->accept, "text/plain") == 0) {
        msg = body;
    } else {
        msg = sfmt(
            "<!DOCTYPE html>\r\n"
            "<html><head><title>%s</title></head>\r\n"
            "<body>\r\n%s\r\n</body>\r\n</html>\r\n",
            title, body);
    }
    va_end(args);
    return httpFormatResponse(conn, "%s", msg);
}


/*
    Create an alternate body response. Typically used for error responses. The message is HTML escaped.
    NOTE: this is an internal API. Users should use httpFormatError
 */
void httpFormatResponseError(HttpConn *conn, int status, cchar *fmt, ...)
{
    va_list     args;
    cchar       *statusMsg;
    char        *msg;

    mprAssert(fmt && fmt);

    va_start(args, fmt);
    msg = sfmtv(fmt, args);
    statusMsg = httpLookupStatus(conn->http, status);
    if (scmp(conn->rx->accept, "text/plain") == 0) {
        msg = sfmt("Access Error: %d -- %s\r\n%s\r\n", status, statusMsg, msg);
    } else {
        msg = sfmt("<h2>Access Error: %d -- %s</h2>\r\n<pre>%s</pre>\r\n", status, statusMsg, mprEscapeHtml(msg));
    }
    httpAddHeaderString(conn, "Cache-Control", "no-cache");
    httpFormatResponseBody(conn, statusMsg, "%s", msg);
    va_end(args);
}


void *httpGetQueueData(HttpConn *conn)
{
    HttpQueue     *q;

    q = conn->tx->queue[HTTP_QUEUE_TX];
    return q->nextQ->queueData;
}


void httpOmitBody(HttpConn *conn)
{
    if (conn->tx) {
        conn->tx->flags |= HTTP_TX_NO_BODY;
        conn->tx->length = -1;
    }
    if (conn->tx->flags & HTTP_TX_HEADERS_CREATED) {
        mprError("Can't set response body if headers have already been created");
        /* Connectors will detect this also and disconnect */
    } else {
        httpDiscardData(conn, HTTP_QUEUE_TX);
    }
}


/*  
    Redirect the user to another web page. The targetUri may or may not have a scheme.
 */
void httpRedirect(HttpConn *conn, int status, cchar *targetUri)
{
    HttpTx      *tx;
    HttpRx      *rx;
    HttpUri     *target, *prev;
    cchar       *msg;
    char        *path, *uri, *dir, *cp;
    int         port;

    mprAssert(targetUri);
    rx = conn->rx;
    tx = conn->tx;
    tx->status = status;

    if (schr(targetUri, '$')) {
        targetUri = httpExpandRouteVars(conn, targetUri);
    }
    mprLog(3, "redirect %d %s", status, targetUri);
    msg = httpLookupStatus(conn->http, status);

    if (300 <= status && status <= 399) {
        if (targetUri == 0) {
            targetUri = "/";
        }
        target = httpCreateUri(targetUri, 0);
        if (!target->host) {
            target->host = rx->parsedUri->host;
        }
        if (!target->scheme) {
            target->scheme = rx->parsedUri->scheme;
        }
        if (conn->http->redirectCallback) {
            targetUri = (conn->http->redirectCallback)(conn, &status, target);
        } else {
            targetUri = httpUriToString(target, 0);
        }
        if (strstr(targetUri, "://") == 0) {
            prev = rx->parsedUri;
            port = strchr(targetUri, ':') ? prev->port : conn->endpoint->port;
            uri = 0;
            if (target->path[0] == '/') {
                /*
                    Absolute URL. If hostName has a port specifier, it overrides prev->port.
                 */
                uri = httpFormatUri(prev->scheme, rx->hostHeader, port, target->path, target->reference, target->query, 
                    HTTP_COMPLETE_URI);
            } else {
                /*
                    Relative file redirection to a file in the same directory as the previous request.
                 */
                dir = sclone(rx->pathInfo);
                if ((cp = strrchr(dir, '/')) != 0) {
                    /* Remove basename */
                    *cp = '\0';
                }
                path = sjoin(dir, "/", target->path, NULL);
                uri = httpFormatUri(prev->scheme, rx->hostHeader, port, path, target->reference, target->query, 
                    HTTP_COMPLETE_URI);
            }
            targetUri = uri;
        }
        httpSetHeader(conn, "Location", "%s", targetUri);
        httpFormatResponse(conn, 
            "<!DOCTYPE html>\r\n"
            "<html><head><title>%s</title></head>\r\n"
            "<body><h1>%s</h1>\r\n<p>The document has moved <a href=\"%s\">here</a>.</p></body></html>\r\n",
            msg, msg, targetUri);
    } else {
        httpFormatResponse(conn, 
            "<!DOCTYPE html>\r\n"
            "<html><head><title>%s</title></head>\r\n"
            "<body><h1>%s</h1>\r\n</body></html>\r\n",
            msg, msg);
    }
    httpFinalize(conn);
}


void httpSetContentLength(HttpConn *conn, MprOff length)
{
    HttpTx      *tx;

    tx = conn->tx;
    if (tx->flags & HTTP_TX_HEADERS_CREATED) {
        return;
    }
    tx->length = length;
    httpSetHeader(conn, "Content-Length", "%Ld", tx->length);
}

void httpSetCookie(HttpConn *conn, cchar *name, cchar *value, cchar *path, cchar *cookieDomain, MprTime lifespan, int flags)
{
    HttpRx      *rx;
    char        *cp, *expiresAtt, *expires, *domainAtt, *domain, *secure, *httponly;

    rx = conn->rx;
    if (path == 0) {
        path = "/";
    }
    domain = (char*) cookieDomain;
    if (!domain) {
        domain = sclone(rx->hostHeader);
        if ((cp = strchr(domain, ':')) != 0) {
            *cp = '\0';
        }
        if (*domain && domain[strlen(domain) - 1] == '.') {
            domain[strlen(domain) - 1] = '\0';
        }
    }
    domainAtt = domain ? "; domain=" : "";
    if (domain && !strchr(domain, '.')) {
        domain = sjoin(".", domain, NULL);
    }
    if (lifespan > 0) {
        expiresAtt = "; expires=";
        expires = mprFormatUniversalTime(MPR_HTTP_DATE, conn->http->now + lifespan);

    } else {
        expires = expiresAtt = "";
    }
    /* 
       Allow multiple cookie headers. Even if the same name. Later definitions take precedence
     */
    secure = (flags & HTTP_COOKIE_SECURE) ? "; secure" : "";
    httponly = (flags & HTTP_COOKIE_HTTP) ?  "; httponly" : "";
    httpAppendHeader(conn, "Set-Cookie", 
        sjoin(name, "=", value, "; path=", path, domainAtt, domain, expiresAtt, expires, secure, httponly, NULL));
    httpAppendHeader(conn, "Cache-control", "no-cache=\"set-cookie\"");
}


/*  
    Set headers for httpWriteHeaders. This defines standard headers.
 */
static void setHeaders(HttpConn *conn, HttpPacket *packet)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpRoute   *route;
    HttpRange   *range;
    MprOff      length;
    cchar       *mimeType;

    mprAssert(packet->flags == HTTP_PACKET_HEADER);

    rx = conn->rx;
    tx = conn->tx;
    route = rx->route;

    /*
        Mandatory headers that must be defined here use httpSetHeader which overwrites existing values. 
     */
    httpAddHeaderString(conn, "Date", conn->http->currentDate);

    if (tx->ext) {
        if ((mimeType = (char*) mprLookupMime(route->mimeTypes, tx->ext)) != 0) {
            if (conn->error) {
                httpAddHeaderString(conn, "Content-Type", "text/html");
            } else {
                httpAddHeaderString(conn, "Content-Type", mimeType);
            }
        }
    }
    if (tx->etag) {
        httpAddHeader(conn, "ETag", "%s", tx->etag);
    }
    length = tx->length > 0 ? tx->length : 0;
    if (rx->flags & HTTP_HEAD) {
        conn->tx->flags |= HTTP_TX_NO_BODY;
        httpDiscardData(conn, HTTP_QUEUE_TX);
        httpAddHeader(conn, "Content-Length", "%Ld", length);
    } else if (tx->chunkSize > 0) {
        httpSetHeaderString(conn, "Transfer-Encoding", "chunked");
    } else if (conn->endpoint) {
        /* Server must not emit a content length header for 1XX, 204 and 304 status */
        if (!((100 <= tx->status && tx->status <= 199) || tx->status == 204 || tx->status == 304)) {
            httpAddHeader(conn, "Content-Length", "%Ld", length);
        }
    } else if (tx->length > 0) {
        /* client with body */
        httpAddHeader(conn, "Content-Length", "%Ld", length);
    }
    if (tx->outputRanges) {
        if (tx->outputRanges->next == 0) {
            range = tx->outputRanges;
            if (tx->entityLength > 0) {
                httpSetHeader(conn, "Content-Range", "bytes %Ld-%Ld/%Ld", range->start, range->end, tx->entityLength);
            } else {
                httpSetHeader(conn, "Content-Range", "bytes %Ld-%Ld/*", range->start, range->end);
            }
        } else {
            httpSetHeader(conn, "Content-Type", "multipart/byteranges; boundary=%s", tx->rangeBoundary);
        }
        httpSetHeader(conn, "Accept-Ranges", "bytes");
    }
    if (conn->endpoint) {
        httpAddHeaderString(conn, "Server", conn->http->software);
        if (--conn->keepAliveCount > 0) {
            httpAddHeaderString(conn, "Connection", "keep-alive");
            httpAddHeader(conn, "Keep-Alive", "timeout=%Ld, max=%d", conn->limits->inactivityTimeout / 1000,
                conn->keepAliveCount);
        } else {
            httpAddHeaderString(conn, "Connection", "close");
        }
    }
}


void httpSetEntityLength(HttpConn *conn, int64 len)
{
    HttpTx      *tx;

    tx = conn->tx;
    tx->entityLength = len;
    if (tx->outputRanges == 0) {
        tx->length = len;
    }
}


void httpSetResponded(HttpConn *conn)
{
    conn->responded = 1;
}


void httpSetStatus(HttpConn *conn, int status)
{
    conn->tx->status = status;
    conn->responded = 1;
}


void httpSetContentType(HttpConn *conn, cchar *mimeType)
{
    httpSetHeaderString(conn, "Content-Type", sclone(mimeType));
}


void httpWriteHeaders(HttpConn *conn, HttpPacket *packet)
{
    Http        *http;
    HttpTx      *tx;
    HttpUri     *parsedUri;
    MprKey      *kp;
    MprBuf      *buf;
    int         level;

    mprAssert(packet->flags == HTTP_PACKET_HEADER);

    http = conn->http;
    tx = conn->tx;
    buf = packet->content;

    if (tx->flags & HTTP_TX_HEADERS_CREATED) {
        return;
    }    
    tx->flags |= HTTP_TX_HEADERS_CREATED;
    conn->responded = 1;
    if (conn->headersCallback) {
        /* Must be before headers below */
        (conn->headersCallback)(conn->headersCallbackArg);
    }
    if (tx->flags & HTTP_TX_USE_OWN_HEADERS && !conn->error) {
        conn->keepAliveCount = -1;
        return;
    }
    setHeaders(conn, packet);

    if (conn->endpoint) {
        mprPutStringToBuf(buf, conn->protocol);
        mprPutCharToBuf(buf, ' ');
        mprPutIntToBuf(buf, tx->status);
        mprPutCharToBuf(buf, ' ');
        mprPutStringToBuf(buf, httpLookupStatus(http, tx->status));
    } else {
        mprPutStringToBuf(buf, tx->method);
        mprPutCharToBuf(buf, ' ');
        parsedUri = tx->parsedUri;
        if (http->proxyHost && *http->proxyHost) {
            if (parsedUri->query && *parsedUri->query) {
                mprPutFmtToBuf(buf, "http://%s:%d%s?%s %s", http->proxyHost, http->proxyPort, 
                    parsedUri->path, parsedUri->query, conn->protocol);
            } else {
                mprPutFmtToBuf(buf, "http://%s:%d%s %s", http->proxyHost, http->proxyPort, parsedUri->path,
                    conn->protocol);
            }
        } else {
            if (parsedUri->query && *parsedUri->query) {
                mprPutFmtToBuf(buf, "%s?%s %s", parsedUri->path, parsedUri->query, conn->protocol);
            } else {
                mprPutStringToBuf(buf, parsedUri->path);
                mprPutCharToBuf(buf, ' ');
                mprPutStringToBuf(buf, conn->protocol);
            }
        }
    }
    if ((level = httpShouldTrace(conn, HTTP_TRACE_TX, HTTP_TRACE_FIRST, tx->ext)) >= mprGetLogLevel(tx)) {
        mprAddNullToBuf(buf);
        mprLog(level, "  %s", mprGetBufStart(buf));
    }
    mprPutStringToBuf(buf, "\r\n");

    /* 
        Output headers
     */
    kp = mprGetFirstKey(conn->tx->headers);
    while (kp) {
        mprPutStringToBuf(packet->content, kp->key);
        mprPutStringToBuf(packet->content, ": ");
        if (kp->data) {
            mprPutStringToBuf(packet->content, kp->data);
        }
        mprPutStringToBuf(packet->content, "\r\n");
        kp = mprGetNextKey(conn->tx->headers, kp);
    }

    /* 
        By omitting the "\r\n" delimiter after the headers, chunks can emit "\r\nSize\r\n" as a single chunk delimiter
     */
    if (tx->chunkSize <= 0) {
        mprPutStringToBuf(buf, "\r\n");
    }
    if (tx->altBody) {
        /* Error responses are emitted here */
        mprPutStringToBuf(buf, tx->altBody);
        httpDiscardQueueData(tx->queue[HTTP_QUEUE_TX]->nextQ, 0);
    }
    tx->headerSize = mprGetBufLength(buf);
    tx->flags |= HTTP_TX_HEADERS_CREATED;
}


bool httpFileExists(HttpConn *conn)
{
    HttpTx      *tx;

    tx = conn->tx;
    if (!tx->fileInfo.checked) {
        mprGetPathInfo(tx->filename, &tx->fileInfo);
    }
    return tx->fileInfo.valid;
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
