/*
    transmitter.c - Http transmitter for server responses and client requests.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static int destroyTransmitter(HttpTransmitter *trans);
static void putHeader(HttpConn *conn, HttpPacket *packet, cchar *key, cchar *value);
static void setDefaultHeaders(HttpConn *conn);

/*********************************** Code *************************************/

HttpTransmitter *httpCreateTransmitter(HttpConn *conn, MprHashTable *headers)
{
    Http            *http;
    HttpTransmitter *trans;

    http = conn->http;

    /*  
        Use the receivers arena so that freeing the receiver will also free the transmitter 
     */
#if FUTURE
    trans = mprAllocObjWithDestructorZeroed(conn->receiver->arena, HttpTransmitter, destroyTransmitter);
#else
    trans = mprAllocObjWithDestructorZeroed(conn->receiver, HttpTransmitter, destroyTransmitter);
#endif
    if (trans == 0) {
        return 0;
    }
    conn->transmitter = trans;
    trans->conn = conn;
    trans->status = HTTP_CODE_OK;
    trans->length = -1;
    trans->entityLength = -1;
    trans->traceMethods = HTTP_STAGE_ALL;
    trans->chunkSize = -1;

    if (headers) {
        trans->headers = headers;
        mprStealBlock(trans, headers);
    } else {
        trans->headers = mprCreateHash(trans, HTTP_SMALL_HASH_SIZE);
        mprSetHashCase(trans->headers, 0);
        setDefaultHeaders(conn);
    }
    httpInitQueue(conn, &trans->queue[HTTP_QUEUE_TRANS], "TransmitterHead");
    httpInitQueue(conn, &trans->queue[HTTP_QUEUE_RECEIVE], "ReceiverHead");
    return trans;
}


static int destroyTransmitter(HttpTransmitter *trans)
{
    LOG(trans, 5, "destroyTrans");
    httpDestroyPipeline(trans->conn);
    trans->conn->transmitter = 0;
    return 0;
}


//  MOB -- rationalize all these header names

static void addHeader(HttpConn *conn, cchar *key, cchar *value)
{
    if (mprStrcmpAnyCase(key, "content-length") == 0) {
        conn->transmitter->length = (int) mprAtoi(value, 10);
    }
    mprAddHash(conn->transmitter->headers, key, value);
}


static void putHeader(HttpConn *conn, HttpPacket *packet, cchar *key, cchar *value)
{
    MprBuf      *buf;

    buf = packet->content;
    mprPutStringToBuf(buf, key);
    mprPutStringToBuf(buf, ": ");
    if (value) {
        mprPutStringToBuf(buf, value);
    }
    mprPutStringToBuf(buf, "\r\n");
}


int httpRemoveHeader(HttpConn *conn, cchar *key)
{
    HttpTransmitter      *trans;

    trans = conn->transmitter;
    if (trans) {
        return mprRemoveHash(trans->headers, key);
    }
    return MPR_ERR_NOT_FOUND;
}


/*  
    Add a http header if not already defined
 */
void httpAddHeader(HttpConn *conn, cchar *key, cchar *fmt, ...)
{
    HttpTransmitter *trans;
    char            *value;
    va_list         vargs;

    trans = conn->transmitter;
    va_start(vargs, fmt);
    value = mprVasprintf(trans, HTTP_MAX_HEADERS, fmt, vargs);
    va_end(vargs);

    if (!mprLookupHash(trans->headers, key)) {
        addHeader(conn, key, value);
    }
}


void httpAddSimpleHeader(HttpConn *conn, cchar *key, cchar *value)
{
    HttpTransmitter      *trans;

    trans = conn->transmitter;
    if (!mprLookupHash(trans->headers, key)) {
        addHeader(conn, key, value);
    }
}


/* 
   Append a header. If already defined, the value is catenated to the pre-existing value after a ", " separator.
   As per the HTTP/1.1 spec.
 */
void httpAppendHeader(HttpConn *conn, cchar *key, cchar *fmt, ...)
{
    HttpTransmitter *trans;
    va_list         vargs;
    char            *value;
    cchar           *oldValue;

    trans = conn->transmitter;
    va_start(vargs, fmt);
    value = mprVasprintf(trans, HTTP_MAX_HEADERS, fmt, vargs);
    va_end(vargs);

    oldValue = mprLookupHash(trans->headers, key);
    if (oldValue) {
        addHeader(conn, key, mprAsprintf(trans->headers, -1, "%s, %s", oldValue, value));
    } else {
        addHeader(conn, key, value);
    }
}


/*  
    Set a http header. Overwrite if present.
 */
void httpSetHeader(HttpConn *conn, cchar *key, cchar *fmt, ...)
{
    HttpTransmitter      *trans;
    char            *value;
    va_list         vargs;

    trans = conn->transmitter;
    va_start(vargs, fmt);
    value = mprVasprintf(trans, HTTP_MAX_HEADERS, fmt, vargs);
    va_end(vargs);
    addHeader(conn, key, value);
}


void httpSetSimpleHeader(HttpConn *conn, cchar *key, cchar *value)
{
    HttpTransmitter      *trans;

    trans = conn->transmitter;
    addHeader(conn, key, mprStrdup(trans, value));
}


void httpDontCache(HttpConn *conn)
{
    conn->transmitter->flags |= HTTP_TRANS_DONT_CACHE;
}


void httpFinalize(HttpConn *conn)
{
    HttpTransmitter   *trans;

    trans = conn->transmitter;
    if (trans->finalized || conn->state < HTTP_STATE_STARTED || conn->writeq == 0) {
        return;
    }
    trans->finalized = 1;
    httpPutForService(conn->writeq, httpCreateEndPacket(trans), 1);
    httpServiceQueues(conn);
}


int httpIsFinalized(HttpConn *conn)
{
    return conn->transmitter && conn->transmitter->finalized;
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
    HttpTransmitter *trans;
    va_list         args;
    char            *body;

    trans = conn->transmitter;
    mprAssert(trans->altBody == 0);

    va_start(args, fmt);
    body = mprVasprintf(trans, HTTP_MAX_HEADERS, fmt, args);
    trans->altBody = mprAsprintf(trans, -1,
        "<!DOCTYPE html>\r\n"
        "<html><head><title>%s</title></head>\r\n"
        "<body>\r\n%s\r\n</body>\r\n</html>\r\n",
        title, body);
    mprFree(body);
    httpOmitBody(conn);
    va_end(args);
    return strlen(trans->altBody);
}


/*
    Create an alternate body response. Typically used for error responses. The message is HTML escaped.
 */
void httpSetResponseBody(HttpConn *conn, int status, cchar *msg)
{
    HttpTransmitter *trans;
    cchar           *statusMsg;
    char            *emsg;

    mprAssert(msg && msg);
    trans = conn->transmitter;

    if (trans->flags & HTTP_TRANS_HEADERS_CREATED) {
        mprError(conn, "Can't set response body if headers have already been created");
        /* Connectors will detect this also and disconnect */
    } else {
        httpDiscardTransmitData(conn);
    }
    trans->status = status;
    if (trans->altBody == 0) {
        statusMsg = httpLookupStatus(conn->http, status);
        emsg = mprEscapeHtml(trans, msg);
        httpFormatBody(conn, statusMsg, "<h2>Access Error: %d -- %s</h2>\r\n<p>%s</p>\r\n", status, statusMsg, emsg);
    }
}


void *httpGetQueueData(HttpConn *conn)
{
    HttpQueue     *q;

    q = &conn->transmitter->queue[HTTP_QUEUE_TRANS];
    return q->nextQ->queueData;
}


void httpOmitBody(HttpConn *conn)
{
    if (conn->transmitter) {
        conn->transmitter->flags |= HTTP_TRANS_NO_BODY;
    }
}


/*  
    Redirect the user to another web page. The targetUri may or may not have a scheme.
 */
void httpRedirect(HttpConn *conn, int status, cchar *targetUri)
{
    HttpTransmitter *trans;
    HttpReceiver    *rec;
    HttpUri         *target, *prev;
    cchar           *msg;
    char            *path, *uri, *dir, *cp;
    int             port;

    mprAssert(targetUri);

    mprLog(conn, 3, "redirect %d %s", status, targetUri);

    rec = conn->receiver;
    trans = conn->transmitter;
    uri = 0;
    trans->status = status;
    prev = rec->parsedUri;
    target = httpCreateUri(trans, targetUri, 0);

    if (strstr(targetUri, "://") == 0) {
        port = strchr(targetUri, ':') ? prev->port : conn->server->port;
        if (target->path[0] == '/') {
            /*
                Absolute URL. If hostName has a port specifier, it overrides prev->port.
             */
            uri = httpFormatUri(trans, prev->scheme, rec->hostName, port, target->path, target->reference, target->query, 1);
        } else {
            /*
                Relative file redirection to a file in the same directory as the previous request.
             */
            dir = mprStrdup(trans, rec->pathInfo);
            if ((cp = strrchr(dir, '/')) != 0) {
                /* Remove basename */
                *cp = '\0';
            }
            path = mprStrcat(trans, -1, dir, "/", target->path, NULL);
            uri = httpFormatUri(trans, prev->scheme, rec->hostName, port, path, target->reference, target->query, 1);
        }
        targetUri = uri;
    }
    httpSetHeader(conn, "Location", "%s", targetUri);
    mprAssert(trans->altBody == 0);
    msg = httpLookupStatus(conn->http, status);
    trans->altBody = mprAsprintf(trans, -1,
        "<!DOCTYPE html>\r\n"
        "<html><head><title>%s</title></head>\r\n"
        "<body><h1>%s</h1>\r\n<p>The document has moved <a href=\"%s\">here</a>.</p>\r\n"
        "<address>%s at %s Port %d</address></body>\r\n</html>\r\n",
        msg, msg, targetUri, HTTP_NAME, conn->server->name, prev->port);
    httpOmitBody(conn);
    mprFree(uri);
}


void httpSetContentLength(HttpConn *conn, int length)
{
    HttpTransmitter     *trans;

    trans = conn->transmitter;
    if (trans->flags & HTTP_TRANS_HEADERS_CREATED) {
        return;
    }
    trans->length = length;
    httpSetHeader(conn, "Content-Length", "%d", trans->length);
}


void httpSetCookie(HttpConn *conn, cchar *name, cchar *value, cchar *path, cchar *cookieDomain, int lifetime, bool isSecure)
{
    HttpReceiver   *rec;
    HttpTransmitter  *trans;
    struct tm   tm;
    char        *cp, *expiresAtt, *expires, *domainAtt, *domain, *secure;
    int         webkitVersion;

    rec = conn->receiver;
    trans = conn->transmitter;

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
    if (cookieDomain == 0 && rec->userAgent && strstr(rec->userAgent, "AppleWebKit") != 0) {
        if ((cp = strstr(rec->userAgent, "Version/")) != NULL && strlen(cp) >= 13) {
            cp = &cp[8];
            webkitVersion = (cp[0] - '0') * 100 + (cp[2] - '0') * 10 + (cp[4] - '0');
        }
    }
    if (webkitVersion >= 312) {
        domain = mprStrdup(trans, rec->hostName);
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
        mprDecodeUniversalTime(trans, &tm, conn->time + (lifetime * MPR_TICKS_PER_SEC));
        expiresAtt = "; expires=";
        expires = mprFormatTime(trans, MPR_HTTP_DATE, &tm);

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
        mprStrcat(trans, -1, name, "=", value, "; path=", path, domainAtt, domain, expiresAtt, expires, secure, NULL));
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
    Http            *http;
    HttpReceiver    *rec;
    HttpTransmitter *trans;
    HttpRange       *range;
    MprBuf          *buf;
    cchar           *mimeType;
    int             handlerFlags;

    mprAssert(packet->flags == HTTP_PACKET_HEADER);

    http = conn->http;
    rec = conn->receiver;
    trans = conn->transmitter;
    buf = packet->content;

    if (rec->flags & HTTP_TRACE) {
        if (!conn->limits->enableTraceMethod) {
            trans->status = HTTP_CODE_NOT_ACCEPTABLE;
            httpFormatBody(conn, "Trace Request Denied", "<p>The TRACE method is disabled on this server.</p>");
        } else {
            trans->altBody = mprAsprintf(trans, -1, "%s %s %s\r\n", rec->method, rec->uri, conn->protocol);
        }
    } else if (rec->flags & HTTP_OPTIONS) {
        handlerFlags = trans->traceMethods;
        httpSetHeader(conn, "Allow", "OPTIONS%s%s%s%s%s%s",
            (conn->limits->enableTraceMethod) ? ",TRACE" : "",
            (handlerFlags & HTTP_STAGE_GET) ? ",GET" : "",
            (handlerFlags & HTTP_STAGE_HEAD) ? ",HEAD" : "",
            (handlerFlags & HTTP_STAGE_POST) ? ",POST" : "",
            (handlerFlags & HTTP_STAGE_PUT) ? ",PUT" : "",
            (handlerFlags & HTTP_STAGE_DELETE) ? ",DELETE" : "");
        trans->length = 0;
    }
    httpAddSimpleHeader(conn, "Date", conn->http->currentDate);

    if (trans->flags & HTTP_TRANS_DONT_CACHE) {
        httpAddSimpleHeader(conn, "Cache-Control", "no-cache");
    }
    if (trans->etag) {
        httpAddHeader(conn, "ETag", "%s", trans->etag);
    }
    if (trans->altBody) {
        trans->length = (int) strlen(trans->altBody);
    }
    if (trans->chunkSize > 0 && !trans->altBody) {
        if (!(rec->flags & HTTP_HEAD)) {
            httpSetSimpleHeader(conn, "Transfer-Encoding", "chunked");
        }
    } else if (trans->length > 0) {
        httpSetHeader(conn, "Content-Length", "%d", trans->length);
    }
    if (rec->ranges) {
        if (rec->ranges->next == 0) {
            range = rec->ranges;
            if (trans->entityLength > 0) {
                httpSetHeader(conn, "Content-Range", "bytes %d-%d/%d", range->start, range->end, trans->entityLength);
            } else {
                httpSetHeader(conn, "Content-Range", "bytes %d-%d/*", range->start, range->end);
            }
        } else {
            httpSetHeader(conn, "Content-Type", "multipart/byteranges; boundary=%s", trans->rangeBoundary);
        }
        httpAddHeader(conn, "Accept-Ranges", "bytes");
    }
    if (trans->extension) {
        if ((mimeType = (char*) mprLookupMimeType(http, trans->extension)) != 0) {
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


void httpSetEntityLength(HttpConn *conn, int len)
{
    HttpReceiver    *rec;
    HttpTransmitter *trans;

    trans = conn->transmitter;
    rec = conn->receiver;
    trans->entityLength = len;
    if (rec->ranges == 0) {
        trans->length = len;
    }
}


void httpSetTransStatus(HttpConn *conn, int status)
{
    conn->transmitter->status = status;
}


void httpSetMimeType(HttpConn *conn, cchar *mimeType)
{
    httpSetSimpleHeader(conn, "Content-Type", mprStrdup(conn->transmitter, mimeType));
}


void httpWriteHeaders(HttpConn *conn, HttpPacket *packet)
{
    Http            *http;
    HttpReceiver    *rec;
    HttpTransmitter *trans;
    HttpUri         *parsedUri;
    MprHash         *hp;
    MprBuf          *buf;

    mprAssert(packet->flags == HTTP_PACKET_HEADER);

    http = conn->http;
    rec = conn->receiver;
    trans = conn->transmitter;
    buf = packet->content;

    if (trans->flags & HTTP_TRANS_HEADERS_CREATED) {
        return;
    }    
    setHeaders(conn, packet);

    if (conn->server) {
        mprPutStringToBuf(buf, conn->protocol);
        mprPutCharToBuf(buf, ' ');
        mprPutIntToBuf(buf, trans->status);
        mprPutCharToBuf(buf, ' ');
        mprPutStringToBuf(buf, httpLookupStatus(http, trans->status));
        mprPutStringToBuf(buf, "\r\n");
    } else {
        mprPutStringToBuf(buf, trans->method);
        mprPutCharToBuf(buf, ' ');
        parsedUri = trans->parsedUri;
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
    hp = mprGetFirstHash(trans->headers);
    while (hp) {
        putHeader(conn, packet, hp->key, hp->data);
        hp = mprGetNextHash(trans->headers, hp);
    }

    /* 
       By omitting the "\r\n" delimiter after the headers, chunks can emit "\r\nSize\r\n" as a single chunk delimiter
     */
    if (trans->chunkSize <= 0 || trans->altBody) {
        mprPutStringToBuf(buf, "\r\n");
    }
    if (trans->altBody) {
        mprPutStringToBuf(buf, trans->altBody);
        httpDiscardData(trans->queue[HTTP_QUEUE_TRANS].nextQ, 0);
    }
    trans->headerSize = mprGetBufLength(buf);
    trans->flags |= HTTP_TRANS_HEADERS_CREATED;
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
