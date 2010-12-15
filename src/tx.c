/*
    tx.c - Http transmitter for server responses and client requests.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static void manageTx(HttpTx *tx, int flags);
static void setDefaultHeaders(HttpConn *conn);

/*********************************** Code *************************************/

HttpTx *httpCreateTx(HttpConn *conn, MprHashTable *headers)
{
    Http        *http;
    HttpTx      *tx;

    http = conn->http;

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

    if (headers) {
        tx->headers = headers;
    } else {
        tx->headers = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS);
        setDefaultHeaders(conn);
    }
    httpInitQueue(conn, &tx->queue[HTTP_QUEUE_TRANS], "TxHead");
    httpInitQueue(conn, &tx->queue[HTTP_QUEUE_RECEIVE], "RxHead");
    return tx;
}


static void manageTx(HttpTx *tx, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(tx->outputPipeline);
        httpManageQueue(&tx->queue[0], flags);
        httpManageQueue(&tx->queue[1], flags);
        mprMark(tx->parsedUri);
        mprMark(tx->currentRange);
        mprMark(tx->rangeBoundary);
        mprMark(tx->etag);
        mprMark(tx->method);
        mprMark(tx->altBody);
        mprMark(tx->file);
        mprMark(tx->filename);
        mprMark(tx->extension);

    } else if (flags & MPR_MANAGE_FREE) {
        httpDestroyPipeline(tx->conn);
        tx->conn->tx = 0;
    }
}


/*
    Add key/value to the header hash. If already present, update the value
*/
static void addHeader(HttpConn *conn, cchar *key, cchar *value)
{
    mprAssert(key && *key);
    mprAssert(value);

    if (scasecmp(key, "content-length") == 0) {
        conn->tx->length = stoi(value, 10, NULL);
    }
    mprAddHash(conn->tx->headers, key, value);
}


int httpRemoveHeader(HttpConn *conn, cchar *key)
{
    HttpTx      *tx;

    mprAssert(key && *key);

    tx = conn->tx;
    if (tx) {
        return mprRemoveHash(tx->headers, key);
    }
    return MPR_ERR_CANT_FIND;
}


/*  
    Add a http header if not already defined
 */
void httpAddHeader(HttpConn *conn, cchar *key, cchar *fmt, ...)
{
    HttpTx      *tx;
    char        *value;
    va_list     vargs;

    mprAssert(key && *key);
    mprAssert(fmt && *fmt);

    tx = conn->tx;
    va_start(vargs, fmt);
    value = mprAsprintfv(fmt, vargs);
    va_end(vargs);

    if (!mprLookupHash(tx->headers, key)) {
        addHeader(conn, key, value);
    }
}


/*
    Add a simple (non-formatted) header if not already defined
 */
void httpAddSimpleHeader(HttpConn *conn, cchar *key, cchar *value)
{
    HttpTx      *tx;

    mprAssert(key && *key);
    mprAssert(value);

    tx = conn->tx;
    if (!mprLookupHash(tx->headers, key)) {
        addHeader(conn, key, value);
    }
}


/* 
   Append a header. If already defined, the value is catenated to the pre-existing value after a ", " separator.
   As per the HTTP/1.1 spec.
 */
void httpAppendHeader(HttpConn *conn, cchar *key, cchar *fmt, ...)
{
    HttpTx      *tx;
    va_list     vargs;
    char        *value;
    cchar       *oldValue;

    mprAssert(key && *key);
    mprAssert(fmt && *fmt);

    tx = conn->tx;
    va_start(vargs, fmt);
    value = mprAsprintfv(fmt, vargs);
    va_end(vargs);

    oldValue = mprLookupHash(tx->headers, key);
    if (oldValue) {
        addHeader(conn, key, mprAsprintf("%s, %s", oldValue, value));
    } else {
        addHeader(conn, key, value);
    }
}


/*  
    Set a http header. Overwrite if present.
 */
void httpSetHeader(HttpConn *conn, cchar *key, cchar *fmt, ...)
{
    HttpTx      *tx;
    char        *value;
    va_list     vargs;

    mprAssert(key && *key);
    mprAssert(fmt && *fmt);

    tx = conn->tx;
    va_start(vargs, fmt);
    value = mprAsprintfv(fmt, vargs);
    va_end(vargs);
    addHeader(conn, key, value);
}


void httpSetSimpleHeader(HttpConn *conn, cchar *key, cchar *value)
{
    HttpTx      *tx;

    mprAssert(key && *key);
    mprAssert(value);

    tx = conn->tx;
    addHeader(conn, key, sclone(value));
}


void httpClearHeaders(HttpConn *conn) 
{
    HttpTx      *tx;

    tx = conn->tx;
    tx->headers = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS);
    setDefaultHeaders(conn);
}


void httpDontCache(HttpConn *conn)
{
    conn->tx->flags |= HTTP_TX_DONT_CACHE;
}


void httpFinalize(HttpConn *conn)
{
    HttpTx      *tx;

    tx = conn->tx;
    if (tx->finalized || conn->state < HTTP_STATE_CONNECTED || conn->writeq == 0 || conn->sock == 0) {
        return;
    }
    tx->finalized = 1;
    httpPutForService(conn->writeq, httpCreateEndPacket(), 1);
    httpServiceQueues(conn);
    if (conn->state == HTTP_STATE_RUNNING && conn->writeComplete && !conn->advancing) {
        httpProcess(conn, NULL);
    }
}


int httpIsFinalized(HttpConn *conn)
{
    return conn->tx && conn->tx->finalized;
}


/*
    Flush the write queue
 */
void httpFlush(HttpConn *conn)
{
    httpFlushQueue(conn->writeq, !conn->async);
}


/*
    Format alternative body content. The message is HTML escaped.
 */
int httpFormatBody(HttpConn *conn, cchar *title, cchar *fmt, ...)
{
    HttpTx      *tx;
    va_list     args;
    char        *body;

    tx = conn->tx;
    mprAssert(tx->altBody == 0);

    va_start(args, fmt);
    body = mprAsprintfv(fmt, args);
    tx->altBody = mprAsprintf(
        "<!DOCTYPE html>\r\n"
        "<html><head><title>%s</title></head>\r\n"
        "<body>\r\n%s\r\n</body>\r\n</html>\r\n",
        title, body);
    httpOmitBody(conn);
    va_end(args);
    return (int) strlen(tx->altBody);
}


/*
    Create an alternate body response. Typically used for error responses. The message is HTML escaped.
 */
void httpSetResponseBody(HttpConn *conn, int status, cchar *msg)
{
    HttpTx      *tx;
    cchar       *statusMsg;
    char        *emsg;

    mprAssert(msg && msg);
    tx = conn->tx;

    if (tx->flags & HTTP_TX_HEADERS_CREATED) {
        mprError("Can't set response body if headers have already been created");
        /* Connectors will detect this also and disconnect */
    } else {
        httpDiscardTransmitData(conn);
    }
    tx->status = status;
    if (tx->altBody == 0) {
        statusMsg = httpLookupStatus(conn->http, status);
        emsg = mprEscapeHtml(msg);
        httpFormatBody(conn, statusMsg, "<h2>Access Error: %d -- %s</h2>\r\n<p>%s</p>\r\n", status, statusMsg, emsg);
    }
}


void *httpGetQueueData(HttpConn *conn)
{
    HttpQueue     *q;

    q = &conn->tx->queue[HTTP_QUEUE_TRANS];
    return q->nextQ->queueData;
}


void httpOmitBody(HttpConn *conn)
{
    if (conn->tx) {
        conn->tx->flags |= HTTP_TX_NO_BODY;
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

    mprLog(3, "redirect %d %s", status, targetUri);

    rx = conn->rx;
    tx = conn->tx;
    uri = 0;
    tx->status = status;
    prev = rx->parsedUri;
    target = httpCreateUri(targetUri, 0);

    if (tx->redirectCallback) {
        targetUri = (tx->redirectCallback)(conn, &status, target);
    }
    if (strstr(targetUri, "://") == 0) {
        port = strchr(targetUri, ':') ? prev->port : conn->server->port;
        if (target->path[0] == '/') {
            /*
                Absolute URL. If hostName has a port specifier, it overrides prev->port.
             */
            uri = httpFormatUri(prev->scheme, rx->hostName, port, target->path, target->reference, target->query, 1);
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
            uri = httpFormatUri(prev->scheme, rx->hostName, port, path, target->reference, target->query, 1);
        }
        targetUri = uri;
    }
    httpSetHeader(conn, "Location", "%s", targetUri);
    mprAssert(tx->altBody == 0);
    msg = httpLookupStatus(conn->http, status);
    tx->altBody = mprAsprintf(
        "<!DOCTYPE html>\r\n"
        "<html><head><title>%s</title></head>\r\n"
        "<body><h1>%s</h1>\r\n<p>The document has moved <a href=\"%s\">here</a>.</p>\r\n"
        "<address>%s at %s Port %d</address></body>\r\n</html>\r\n",
        msg, msg, targetUri, HTTP_NAME, conn->server->name, prev->port);
    httpOmitBody(conn);
}


void httpSetContentLength(HttpConn *conn, ssize length)
{
    HttpTx      *tx;

    tx = conn->tx;
    if (tx->flags & HTTP_TX_HEADERS_CREATED) {
        return;
    }
    tx->length = length;
    httpSetHeader(conn, "Content-Length", "%d", tx->length);
}


void httpSetCookie(HttpConn *conn, cchar *name, cchar *value, cchar *path, cchar *cookieDomain, int lifetime, bool isSecure)
{
    HttpRx      *rx;
    HttpTx      *tx;
    struct tm   tm;
    char        *cp, *expiresAtt, *expires, *domainAtt, *domain, *secure;
    int         webkitVersion;

    rx = conn->rx;
    tx = conn->tx;

    if (path == 0) {
        path = "/";
    }

    /* 
        Fix for Safari >= 3.2.1 with Bonjour addresses with a trailing ".". Safari discards cookies without a domain 
        specifier or with a domain that includes a trailing ".". Solution: include an explicit domain and trim the 
        trailing ".".
      
        User-Agent: Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10_5_6; en-us) 
             AppleWebKit/530.0+ (KHTML, like Gecko) Version/3.1.2 Safari/525.20.1
    */
    webkitVersion = 0;
    if (cookieDomain == 0 && rx->userAgent && strstr(rx->userAgent, "AppleWebKit") != 0) {
        if ((cp = strstr(rx->userAgent, "Version/")) != NULL && strlen(cp) >= 13) {
            cp = &cp[8];
            webkitVersion = (cp[0] - '0') * 100 + (cp[2] - '0') * 10 + (cp[4] - '0');
        }
    }
    if (webkitVersion >= 312) {
        domain = sclone(rx->hostName);
        if ((cp = strchr(domain, ':')) != 0) {
            *cp = '\0';
        }
        if (*domain && domain[strlen(domain) - 1] == '.') {
            domain[strlen(domain) - 1] = '\0';
        } else {
            domain = 0;
        }
    } else {
        domain = 0;
    }
    if (domain) {
        domainAtt = "; domain=";
    } else {
        domainAtt = "";
    }
    if (lifetime > 0) {
        mprDecodeUniversalTime(&tm, conn->time + (lifetime * MPR_TICKS_PER_SEC));
        expiresAtt = "; expires=";
        expires = mprFormatTime(MPR_HTTP_DATE, &tm);

    } else {
        expires = expiresAtt = "";
    }
    if (isSecure) {
        secure = "; secure";
    } else {
        secure = ";";
    }
    /* 
       Allow multiple cookie headers. Even if the same name. Later definitions take precedence
     */
    httpAppendHeader(conn, "Set-Cookie", 
        sjoin(name, "=", value, "; path=", path, domainAtt, domain, expiresAtt, expires, secure, NULL));
    httpAppendHeader(conn, "Cache-control", "no-cache=\"set-cookie\"");
}


static void setDefaultHeaders(HttpConn *conn)
{
    if (conn->server) {
        httpAddSimpleHeader(conn, "Server", conn->server->software);
    } else {
        httpAddSimpleHeader(conn, "User-Agent", HTTP_NAME);
    }
}


/*  
    Set headers for httpWriteHeaders. This defines standard headers.
 */
static void setHeaders(HttpConn *conn, HttpPacket *packet)
{
    Http        *http;
    HttpRx      *rx;
    HttpTx      *tx;
    HttpRange   *range;
    MprBuf      *buf;
    MprTime     expires;
    cchar       *mimeType;
    char        *hdr;
    struct tm   tm;
    int         handlerFlags;

    mprAssert(packet->flags == HTTP_PACKET_HEADER);

    http = conn->http;
    rx = conn->rx;
    tx = conn->tx;
    buf = packet->content;

    if (rx->flags & HTTP_TRACE) {
        if (!conn->limits->enableTraceMethod) {
            tx->status = HTTP_CODE_NOT_ACCEPTABLE;
            httpFormatBody(conn, "Trace Request Denied", "<p>The TRACE method is disabled on this server.</p>");
        } else {
            tx->altBody = mprAsprintf("%s %s %s\r\n", rx->method, rx->uri, conn->protocol);
        }
    } else if (rx->flags & HTTP_OPTIONS) {
        handlerFlags = tx->traceMethods;
        httpSetHeader(conn, "Allow", "OPTIONS%s%s%s%s%s%s",
            (conn->limits->enableTraceMethod) ? ",TRACE" : "",
            (handlerFlags & HTTP_STAGE_GET) ? ",GET" : "",
            (handlerFlags & HTTP_STAGE_HEAD) ? ",HEAD" : "",
            (handlerFlags & HTTP_STAGE_POST) ? ",POST" : "",
            (handlerFlags & HTTP_STAGE_PUT) ? ",PUT" : "",
            (handlerFlags & HTTP_STAGE_DELETE) ? ",DELETE" : "");
        tx->length = 0;
    }
    httpAddSimpleHeader(conn, "Date", conn->http->currentDate);

    if (tx->flags & HTTP_TX_DONT_CACHE) {
        httpAddSimpleHeader(conn, "Cache-Control", "no-cache");

    } else if (rx->loc && rx->loc->expires) {
        mimeType = mprLookupHash(tx->headers, "Content-Type");
        expires = PTOL(mprLookupHash(rx->loc->expires, mimeType ? mimeType : ""));
        if (expires == 0) {
            expires = PTOL(mprLookupHash(rx->loc->expires, ""));
        }
        if (expires) {
            mprDecodeUniversalTime(&tm, mprGetTime(conn) + (expires * MPR_TICKS_PER_SEC));
            hdr = mprFormatTime(MPR_HTTP_DATE, &tm);
            httpAddHeader(conn, "Cache-Control", "max-age=%d", expires);
            httpAddHeader(conn, "Expires", "%s", hdr);
        }
    }
    if (tx->etag) {
        httpAddHeader(conn, "ETag", "%s", tx->etag);
    }
    if (tx->altBody) {
        tx->length = (int) strlen(tx->altBody);
    }
    if (tx->chunkSize > 0 && !tx->altBody) {
        if (!(rx->flags & HTTP_HEAD)) {
            httpSetSimpleHeader(conn, "Transfer-Encoding", "chunked");
        }
    } else if (tx->length > 0) {
        httpSetHeader(conn, "Content-Length", "%d", tx->length);
    }
    if (rx->ranges) {
        if (rx->ranges->next == 0) {
            range = rx->ranges;
            if (tx->entityLength > 0) {
                httpSetHeader(conn, "Content-Range", "bytes %d-%d/%d", range->start, range->end, tx->entityLength);
            } else {
                httpSetHeader(conn, "Content-Range", "bytes %d-%d/*", range->start, range->end);
            }
        } else {
            httpSetHeader(conn, "Content-Type", "multipart/byteranges; boundary=%s", tx->rangeBoundary);
        }
        httpAddHeader(conn, "Accept-Ranges", "bytes");
    }
    if (tx->extension) {
        if ((mimeType = (char*) mprLookupMimeType(tx->extension)) != 0) {
            httpAddSimpleHeader(conn, "Content-Type", mimeType);
        }
    }
    if (conn->server) {
        if (--conn->keepAliveCount > 0) {
            httpSetSimpleHeader(conn, "Connection", "keep-alive");
            httpSetHeader(conn, "Keep-Alive", "timeout=%d, max=%d", conn->limits->inactivityTimeout / 1000, 
                conn->keepAliveCount);
        } else {
            httpSetSimpleHeader(conn, "Connection", "close");
        }
    }
}


void httpSetEntityLength(HttpConn *conn, ssize len)
{
    HttpTx      *tx;

    tx = conn->tx;
    tx->entityLength = len;
    if (conn->rx->ranges == 0) {
        tx->length = len;
    }
}


/*
    Set the tx status.
 */
void httpSetStatus(HttpConn *conn, int status)
{
    conn->tx->status = status;
}


void httpSetMimeType(HttpConn *conn, cchar *mimeType)
{
    httpSetSimpleHeader(conn, "Content-Type", sclone(mimeType));
}


void httpWriteHeaders(HttpConn *conn, HttpPacket *packet)
{
    Http        *http;
    HttpRx      *rx;
    HttpTx      *tx;
    HttpUri     *parsedUri;
    MprHash     *hp;
    MprBuf      *buf;
    int         level;

    mprAssert(packet->flags == HTTP_PACKET_HEADER);

    http = conn->http;
    tx = conn->tx;
    rx = conn->rx;
    buf = packet->content;

    if (tx->flags & HTTP_TX_HEADERS_CREATED) {
        return;
    }    
    if (conn->fillHeaders) {
        (conn->fillHeaders)(conn->fillHeadersArg);
    }
    setHeaders(conn, packet);

    if (conn->server) {
        mprPutStringToBuf(buf, conn->protocol);
        mprPutCharToBuf(buf, ' ');
        mprPutIntToBuf(buf, tx->status);
        mprPutCharToBuf(buf, ' ');
        mprPutStringToBuf(buf, httpLookupStatus(http, tx->status));
        mprPutStringToBuf(buf, "\r\n");
    } else {
        mprPutStringToBuf(buf, tx->method);
        mprPutCharToBuf(buf, ' ');
        parsedUri = tx->parsedUri;
        if (http->proxyHost && *http->proxyHost) {
            if (parsedUri->query && *parsedUri->query) {
                mprPutFmtToBuf(buf, "http://%s:%d%s?%s %s\r\n", http->proxyHost, http->proxyPort, 
                    parsedUri->path, parsedUri->query, conn->protocol);
            } else {
                mprPutFmtToBuf(buf, "http://%s:%d%s %s\r\n", http->proxyHost, http->proxyPort, parsedUri->path,
                    conn->protocol);
            }
        } else {
            if (parsedUri->query && *parsedUri->query) {
                mprPutFmtToBuf(buf, "%s?%s %s\r\n", parsedUri->path, parsedUri->query, conn->protocol);
            } else {
                mprPutStringToBuf(buf, parsedUri->path);
                mprPutCharToBuf(buf, ' ');
                mprPutStringToBuf(buf, conn->protocol);
                mprPutStringToBuf(buf, "\r\n");
            }
        }
    }

    /* 
       Output headers
     */
    hp = mprGetFirstHash(tx->headers);
    while (hp) {
        mprPutStringToBuf(packet->content, hp->key);
        mprPutStringToBuf(packet->content, ": ");
        if (hp->data) {
            mprPutStringToBuf(packet->content, hp->data);
        }
        mprPutStringToBuf(packet->content, "\r\n");
        hp = mprGetNextHash(tx->headers, hp);
    }

    /* 
       By omitting the "\r\n" delimiter after the headers, chunks can emit "\r\nSize\r\n" as a single chunk delimiter
     */
    if (tx->chunkSize <= 0 || tx->altBody) {
        mprPutStringToBuf(buf, "\r\n");
    }
    if (tx->altBody) {
        mprPutStringToBuf(buf, tx->altBody);
        httpDiscardData(tx->queue[HTTP_QUEUE_TRANS].nextQ, 0);
    }
    tx->headerSize = mprGetBufLength(buf);
    tx->flags |= HTTP_TX_HEADERS_CREATED;

    if ((level = httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_FIRST, NULL)) == mprGetLogLevel(tx)) {
        mprLog(level, "%s %s %d", rx->method, rx->uri, tx->status);
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

    @end
 */
