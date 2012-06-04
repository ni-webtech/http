/*
    rx.c -- Http receiver. Parses http requests and client responses.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static void addMatchEtag(HttpConn *conn, char *etag);
static char *getToken(HttpConn *conn, cchar *delim);
static void manageRange(HttpRange *range, int flags);
static void manageRx(HttpRx *rx, int flags);
static bool parseAuthenticate(HttpConn *conn, char *authDetails);
static void parseHeaders(HttpConn *conn, HttpPacket *packet);
static bool parseIncoming(HttpConn *conn, HttpPacket *packet);
static bool parseRange(HttpConn *conn, char *value);
static void parseRequestLine(HttpConn *conn, HttpPacket *packet);
static void parseResponseLine(HttpConn *conn, HttpPacket *packet);
static bool processCompletion(HttpConn *conn);
static bool processContent(HttpConn *conn, HttpPacket *packet);
static void parseMethod(HttpConn *conn);
static bool processParsed(HttpConn *conn);
static bool processReady(HttpConn *conn);
static bool processRunning(HttpConn *conn);
static void routeRequest(HttpConn *conn);

/*********************************** Code *************************************/

HttpRx *httpCreateRx(HttpConn *conn)
{
    HttpRx      *rx;

    if ((rx = mprAllocObj(HttpRx, manageRx)) == 0) {
        return 0;
    }
    rx->conn = conn;
    rx->length = -1;
    rx->ifMatch = 1;
    rx->ifModified = 1;
    rx->pathInfo = sclone("/");
    rx->scriptName = mprEmptyString();
    rx->needInputPipeline = !conn->endpoint;
    rx->headers = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS);
    rx->chunkState = HTTP_CHUNK_UNCHUNKED;
    rx->traceLevel = -1;
    return rx;
}


static void manageRx(HttpRx *rx, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(rx->method);
        mprMark(rx->uri);
        mprMark(rx->pathInfo);
        mprMark(rx->scriptName);
        mprMark(rx->extraPath);
        mprMark(rx->conn);
        mprMark(rx->route);
        mprMark(rx->etags);
        mprMark(rx->headerPacket);
        mprMark(rx->headers);
        mprMark(rx->inputPipeline);
        mprMark(rx->parsedUri);
        mprMark(rx->requestData);
        mprMark(rx->statusMessage);
        mprMark(rx->accept);
        mprMark(rx->acceptCharset);
        mprMark(rx->acceptEncoding);
        mprMark(rx->acceptLanguage);
        mprMark(rx->cookie);
        mprMark(rx->connection);
        mprMark(rx->contentLength);
        mprMark(rx->hostHeader);
        mprMark(rx->pragma);
        mprMark(rx->mimeType);
        mprMark(rx->originalMethod);
        mprMark(rx->originalUri);
        mprMark(rx->redirect);
        mprMark(rx->referrer);
        mprMark(rx->securityToken);
        mprMark(rx->userAgent);
        mprMark(rx->params);
        mprMark(rx->svars);
        mprMark(rx->inputRange);
        mprMark(rx->authAlgorithm);
        mprMark(rx->authDetails);
        mprMark(rx->authStale);
        mprMark(rx->authType);
        mprMark(rx->files);
        mprMark(rx->uploadDir);
        mprMark(rx->paramString);
        mprMark(rx->lang);
        mprMark(rx->target);

    } else if (flags & MPR_MANAGE_FREE) {
        if (rx->conn) {
            rx->conn->rx = 0;
        }
    }
}


void httpDestroyRx(HttpRx *rx)
{
    if (rx->conn) {
        rx->conn->rx = 0;
        rx->conn = 0;
    }
}


/*  
    Pump the Http engine.
    Process incoming requests and drive the state machine. This will process as many requests as possible before returning. 
    All socket I/O is non-blocking, and this routine must not block. Note: packet may be null.
 */
void httpPump(HttpConn *conn, HttpPacket *packet)
{
    mprAssert(conn);

    conn->canProceed = 1;
    conn->inHttpProcess = 1;

    while (conn->canProceed) {
        LOG(7, "httpProcess %s, state %d, error %d", conn->dispatcher->name, conn->state, conn->error);
        switch (conn->state) {
        case HTTP_STATE_BEGIN:
        case HTTP_STATE_CONNECTED:
            conn->canProceed = parseIncoming(conn, packet);
            break;

        case HTTP_STATE_PARSED:
            conn->canProceed = processParsed(conn);
            break;

        case HTTP_STATE_CONTENT:
            conn->canProceed = processContent(conn, packet);
            break;

        case HTTP_STATE_READY:
            conn->canProceed = processReady(conn);
            break;

        case HTTP_STATE_RUNNING:
            conn->canProceed = processRunning(conn);
            break;

        case HTTP_STATE_COMPLETE:
            conn->canProceed = processCompletion(conn);
            break;
        }
        if (conn->connError || (conn->endpoint && conn->connectorComplete && conn->state >= HTTP_STATE_RUNNING)) {
            httpSetState(conn, HTTP_STATE_COMPLETE);
            continue;
        }
        packet = conn->input;
    }
    conn->inHttpProcess = 0;
}


/*  
    Parse the incoming http message. Return true to keep going with this or subsequent request, zero means
    insufficient data to proceed.
 */
static bool parseIncoming(HttpConn *conn, HttpPacket *packet)
{
    HttpRx      *rx;
    ssize       len;
    char        *start, *end;

    if (packet == NULL) {
        return 0;
    }
    if (mprShouldDenyNewRequests()) {
        httpError(conn, HTTP_ABORT | HTTP_CODE_NOT_ACCEPTABLE, "Server terminating");
        return 0;
    }
    if (!conn->rx) {
        conn->rx = httpCreateRx(conn);
        conn->tx = httpCreateTx(conn, NULL);
    }
    rx = conn->rx;
    if ((len = httpGetPacketLength(packet)) == 0) {
        return 0;
    }
    start = mprGetBufStart(packet->content);
    if ((end = scontains(start, "\r\n\r\n", len)) == 0) {
        if (len >= conn->limits->headerSize) {
            httpError(conn, HTTP_ABORT | HTTP_CODE_REQUEST_TOO_LARGE, 
                "Header too big. Length %d vs limit %d", len, conn->limits->headerSize);
        }
        return 0;
    }
    len = end - start;
    mprAddNullToBuf(packet->content);

    if (len >= conn->limits->headerSize) {
        httpError(conn, HTTP_ABORT | HTTP_CODE_REQUEST_TOO_LARGE, 
            "Header too big. Length %d vs limit %d", len, conn->limits->headerSize);
        return 0;
    }
    if (conn->endpoint) {
        if (!httpValidateLimits(conn->endpoint, HTTP_VALIDATE_OPEN_REQUEST, conn)) {
            return 0;
        }
        parseRequestLine(conn, packet);
    } else {
        parseResponseLine(conn, packet);
    }
    parseHeaders(conn, packet);
    if (conn->endpoint) {
        httpMatchHost(conn);
        if (httpSetUri(conn, rx->uri, "") < 0 || rx->pathInfo[0] != '/') {
            httpError(conn, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad URL format");
        }
        if (conn->secure) {
            rx->parsedUri->scheme = sclone("https");
        }
        rx->parsedUri->port = conn->sock->listenSock->port;
        rx->parsedUri->host = rx->hostHeader ? rx->hostHeader : conn->host->name;

    } else if (!(100 <= rx->status && rx->status < 200)) {
        /* Clients have already created their Tx pipeline */
        httpCreateRxPipeline(conn, conn->http->clientRoute);
    }
    httpSetState(conn, HTTP_STATE_PARSED);
    return 1;
}


static void mapMethod(HttpConn *conn)
{
    HttpRx      *rx;
    cchar       *method;

    rx = conn->rx;
    if (rx->flags & HTTP_POST) {
        if ((method = httpGetParam(conn, "-http-method-", 0)) != 0) {
            if (!scasematch(method, rx->method)) {
                mprLog(3, "Change method from %s to %s for %s", rx->method, method, rx->uri);
                httpSetMethod(conn, method);
            }
        }
    }
}


static void routeRequest(HttpConn *conn)
{
    HttpRx  *rx;

    mprAssert(conn->endpoint);

    rx = conn->rx;
    httpAddParams(conn);
    mapMethod(conn);
    httpRouteRequest(conn);  
    httpCreateRxPipeline(conn, rx->route);
    httpCreateTxPipeline(conn, rx->route);
}


/*
    Only called by parseRequestLine
 */
static void traceRequest(HttpConn *conn, HttpPacket *packet)
{
    MprBuf  *content;
    cchar   *endp, *ext, *cp;
    int     len, level;

    content = packet->content;
    ext = 0;

    /*
        Find the Uri extension:   "GET /path.ext HTTP/1.1"
     */
    if ((cp = schr(content->start, ' ')) != 0) {
        if ((cp = schr(++cp, ' ')) != 0) {
            for (ext = --cp; ext > content->start && *ext != '.'; ext--) ;
            ext = (*ext == '.') ? snclone(&ext[1], cp - ext) : 0;
            conn->tx->ext = ext;
        }
    }

    /*
        If tracing header, do entire header including first line
     */
    if ((conn->rx->traceLevel = httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_HEADER, ext)) >= 0) {
        mprLog(4, "New request from %s:%d to %s:%d", conn->ip, conn->port, conn->sock->acceptIp, conn->sock->acceptPort);
        endp = strstr((char*) content->start, "\r\n\r\n");
        len = (endp) ? (int) (endp - mprGetBufStart(content) + 4) : 0;
        httpTraceContent(conn, HTTP_TRACE_RX, HTTP_TRACE_HEADER, packet, len, 0);

    } else if ((level = httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_FIRST, ext)) >= 0) {
        endp = strstr((char*) content->start, "\r\n");
        len = (endp) ? (int) (endp - mprGetBufStart(content) + 2) : 0;
        if (len > 0) {
            content->start[len - 2] = '\0';
            mprLog(level, "%s", content->start);
            content->start[len - 2] = '\r';
        }
    }
}


static void parseMethod(HttpConn *conn)
{
    HttpRx      *rx;
    cchar       *method;
    int         methodFlags;

    rx = conn->rx;
    method = rx->method;
    methodFlags = 0;

#if UNUSED
    rx->flags &= (HTTP_DELETE | HTTP_GET | HTTP_HEAD | HTTP_POST | HTTP_PUT | HTTP_TRACE | HTTP_UNKNOWN);
#endif

    switch (method[0]) {
    case 'D':
        if (strcmp(method, "DELETE") == 0) {
            methodFlags = HTTP_DELETE;
        }
        break;

    case 'G':
        if (strcmp(method, "GET") == 0) {
            methodFlags = HTTP_GET;
        }
        break;

    case 'H':
        if (strcmp(method, "HEAD") == 0) {
            methodFlags = HTTP_HEAD;
            httpOmitBody(conn);
        }
        break;

    case 'O':
        if (strcmp(method, "OPTIONS") == 0) {
            methodFlags = HTTP_OPTIONS;
        }
        break;

    case 'P':
        if (strcmp(method, "POST") == 0) {
            methodFlags = HTTP_POST;
            rx->needInputPipeline = 1;

        } else if (strcmp(method, "PUT") == 0) {
            methodFlags = HTTP_PUT;
            rx->needInputPipeline = 1;
        }
        break;

    case 'T':
        if (strcmp(method, "TRACE") == 0) {
            methodFlags = HTTP_TRACE;
        }
        break;
    }
    if (methodFlags == 0) {
        methodFlags = HTTP_UNKNOWN;
    }
    rx->flags |= methodFlags;
}


/*  
    Parse the first line of a http request. Return true if the first line parsed. This is only called once all the headers
    have been read and buffered. Requests look like: METHOD URL HTTP/1.X.
 */
static void parseRequestLine(HttpConn *conn, HttpPacket *packet)
{
    HttpRx      *rx;
    char        *uri, *protocol;
    ssize       len;

    rx = conn->rx;
#if BIT_DEBUG
    conn->startTime = conn->http->now;
    conn->startTicks = mprGetTicks();
#endif
    traceRequest(conn, packet);

    rx->originalMethod = rx->method = supper(getToken(conn, " "));
    parseMethod(conn);

    uri = getToken(conn, " ");
    len = slen(uri);
    if (*uri == '\0') {
        httpError(conn, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad HTTP request. Empty URI");
    } else if (len >= conn->limits->uriSize) {
        httpError(conn, HTTP_CLOSE | HTTP_CODE_REQUEST_URL_TOO_LARGE, 
            "Bad request. URI too long. Length %d vs limit %d", len, conn->limits->uriSize);
    }
    protocol = conn->protocol = supper(getToken(conn, "\r\n"));
    if (strcmp(protocol, "HTTP/1.0") == 0) {
        if (rx->flags & (HTTP_POST|HTTP_PUT)) {
            rx->remainingContent = MAXINT;
            rx->needInputPipeline = 1;
        }
        conn->http10 = 1;
        conn->protocol = protocol;
    } else if (strcmp(protocol, "HTTP/1.1") == 0) {
        conn->protocol = protocol;
    } else {
        conn->protocol = sclone("HTTP/1.1");
        httpError(conn, HTTP_CLOSE | HTTP_CODE_NOT_ACCEPTABLE, "Unsupported HTTP protocol");
    }
    rx->originalUri = rx->uri = sclone(uri);
    httpSetState(conn, HTTP_STATE_FIRST);
}


/*  
    Parse the first line of a http response. Return true if the first line parsed. This is only called once all the headers
    have been read and buffered. Response status lines look like: HTTP/1.X CODE Message
 */
static void parseResponseLine(HttpConn *conn, HttpPacket *packet)
{
    HttpRx      *rx;
    HttpTx      *tx;
    MprBuf      *content;
    cchar       *endp;
    char        *protocol, *status;
    ssize       len;
    int         level, traced;

    rx = conn->rx;
    tx = conn->tx;
    traced = 0;

    if (httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_HEADER, tx->ext) >= 0) {
        content = packet->content;
        endp = strstr((char*) content->start, "\r\n\r\n");
        len = (endp) ? (int) (endp - mprGetBufStart(content) + 4) : 0;
        httpTraceContent(conn, HTTP_TRACE_RX, HTTP_TRACE_HEADER, packet, len, 0);
        traced = 1;
    }
    protocol = conn->protocol = supper(getToken(conn, " "));
    if (strcmp(protocol, "HTTP/1.0") == 0) {
        conn->http10 = 1;
    } else if (strcmp(protocol, "HTTP/1.1") != 0) {
        httpError(conn, HTTP_CLOSE | HTTP_CODE_NOT_ACCEPTABLE, "Unsupported HTTP protocol");
    }
    status = getToken(conn, " ");
    if (*status == '\0') {
        httpError(conn, HTTP_CLOSE | HTTP_CODE_NOT_ACCEPTABLE, "Bad response status code");
    }
    rx->status = atoi(status);
    rx->statusMessage = sclone(getToken(conn, "\r\n"));

    len = slen(rx->statusMessage);
    if (len >= conn->limits->uriSize) {
        httpError(conn, HTTP_CLOSE | HTTP_CODE_REQUEST_URL_TOO_LARGE, 
            "Bad response. Status message too long. Length %d vs limit %d", len, conn->limits->uriSize);
    }
    if (!traced && (level = httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_FIRST, tx->ext)) >= 0) {
        mprLog(level, "%s %d %s", protocol, rx->status, rx->statusMessage);
    }
}


/*  
    Parse the request headers. Return true if the header parsed.
 */
static void parseHeaders(HttpConn *conn, HttpPacket *packet)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpLimits  *limits;
    MprBuf      *content;
    char        *key, *value, *tok, *hvalue;
    cchar       *oldValue;
    int         count, keepAlive;

    rx = conn->rx;
    tx = conn->tx;
    content = packet->content;
    conn->rx->headerPacket = packet;
    limits = conn->limits;
    keepAlive = (conn->http10) ? 0 : 1;

    for (count = 0; content->start[0] != '\r' && !conn->error; count++) {
        if (count >= limits->headerMax) {
            httpError(conn, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Too many headers");
            break;
        }
        if ((key = getToken(conn, ":")) == 0 || *key == '\0') {
            httpError(conn, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad header format");
            break;
        }
        value = getToken(conn, "\r\n");
        while (isspace((uchar) *value)) {
            value++;
        }
        LOG(8, "Key %s, value %s", key, value);
        if (strspn(key, "%<>/\\") > 0) {
            httpError(conn, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad header key value");
        }
        if ((oldValue = mprLookupKey(rx->headers, key)) != 0) {
            hvalue = sfmt("%s, %s", oldValue, value);
        } else {
            hvalue = sclone(value);
        }
        mprAddKey(rx->headers, key, hvalue);

        switch (tolower((uchar) key[0])) {
        case 'a':
            if (strcasecmp(key, "authorization") == 0) {
                value = sclone(value);
                rx->authType = stok(value, " \t", &tok);
                rx->authDetails = sclone(tok);

            } else if (strcasecmp(key, "accept-charset") == 0) {
                rx->acceptCharset = sclone(value);

            } else if (strcasecmp(key, "accept") == 0) {
                rx->accept = sclone(value);

            } else if (strcasecmp(key, "accept-encoding") == 0) {
                rx->acceptEncoding = sclone(value);

            } else if (strcasecmp(key, "accept-language") == 0) {
                rx->acceptLanguage = sclone(value);
            }
            break;

        case 'c':
            if (strcasecmp(key, "connection") == 0) {
                rx->connection = sclone(value);
                if (scasecmp(value, "KEEP-ALIVE") == 0) {
                    keepAlive = 1;
                } else if (scasecmp(value, "CLOSE") == 0) {
                    /*  Not really required, but set to 0 to be sure */
                    conn->keepAliveCount = 0;
                }

            } else if (strcasecmp(key, "content-length") == 0) {
                if (rx->length >= 0) {
                    httpError(conn, HTTP_CLOSE | HTTP_CODE_BAD_REQUEST, "Mulitple content length headers");
                    break;
                }
                rx->length = stoi(value);
                if (rx->length < 0) {
                    httpError(conn, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad content length");
                    break;
                }
                if (rx->length >= conn->limits->receiveBodySize) {
                    httpError(conn, HTTP_ABORT | HTTP_CODE_REQUEST_TOO_LARGE,
                        "Request content length %,Ld bytes is too big. Limit %,Ld", 
                        rx->length, conn->limits->receiveBodySize);
                    break;
                }
                rx->contentLength = sclone(value);
                mprAssert(rx->length >= 0);
                if (conn->endpoint || strcasecmp(tx->method, "HEAD") != 0) {
                    rx->remainingContent = rx->length;
                    rx->needInputPipeline = 1;
                }

            } else if (strcasecmp(key, "content-range") == 0) {
                /*
                    This headers specifies the range of any posted body data
                    Format is:  Content-Range: bytes n1-n2/length
                    Where n1 is first byte pos and n2 is last byte pos
                 */
                char    *sp;
                MprOff  start, end, size;

                start = end = size = -1;
                sp = value;
                while (*sp && !isdigit((uchar) *sp)) {
                    sp++;
                }
                if (*sp) {
                    start = stoi(sp);
                    if ((sp = strchr(sp, '-')) != 0) {
                        end = stoi(++sp);
                    }
                    if ((sp = strchr(sp, '/')) != 0) {
                        /*
                            Note this is not the content length transmitted, but the original size of the input of which
                            the client is transmitting only a portion.
                         */
                        size = stoi(++sp);
                    }
                }
                if (start < 0 || end < 0 || size < 0 || end <= start) {
                    httpError(conn, HTTP_CLOSE | HTTP_CODE_RANGE_NOT_SATISFIABLE, "Bad content range");
                    break;
                }
                rx->inputRange = httpCreateRange(conn, start, end);

            } else if (strcasecmp(key, "content-type") == 0) {
                rx->mimeType = sclone(value);
                rx->form = (rx->flags & HTTP_POST) && scontains(rx->mimeType, "application/x-www-form-urlencoded", -1);
                rx->upload = (rx->flags & HTTP_POST) && scontains(rx->mimeType, "multipart/form-data", -1);

            } else if (strcasecmp(key, "cookie") == 0) {
                if (rx->cookie && *rx->cookie) {
                    rx->cookie = sjoin(rx->cookie, "; ", value, NULL);
                } else {
                    rx->cookie = sclone(value);
                }
            }
            break;

        case 'h':
            if (strcasecmp(key, "host") == 0) {
                rx->hostHeader = sclone(value);
            }
            break;

        case 'i':
            if ((strcasecmp(key, "if-modified-since") == 0) || (strcasecmp(key, "if-unmodified-since") == 0)) {
                MprTime     newDate = 0;
                char        *cp;
                bool        ifModified = (tolower((uchar) key[3]) == 'm');

                if ((cp = strchr(value, ';')) != 0) {
                    *cp = '\0';
                }
                if (mprParseTime(&newDate, value, MPR_UTC_TIMEZONE, NULL) < 0) {
                    mprAssert(0);
                    break;
                }
                if (newDate) {
                    rx->since = newDate;
                    rx->ifModified = ifModified;
                    rx->flags |= HTTP_IF_MODIFIED;
                }

            } else if ((strcasecmp(key, "if-match") == 0) || (strcasecmp(key, "if-none-match") == 0)) {
                char    *word, *tok;
                bool    ifMatch = (tolower((uchar) key[3]) == 'm');

                if ((tok = strchr(value, ';')) != 0) {
                    *tok = '\0';
                }
                rx->ifMatch = ifMatch;
                rx->flags |= HTTP_IF_MODIFIED;
                value = sclone(value);
                word = stok(value, " ,", &tok);
                while (word) {
                    addMatchEtag(conn, word);
                    word = stok(0, " ,", &tok);
                }

            } else if (strcasecmp(key, "if-range") == 0) {
                char    *word, *tok;

                if ((tok = strchr(value, ';')) != 0) {
                    *tok = '\0';
                }
                rx->ifMatch = 1;
                rx->flags |= HTTP_IF_MODIFIED;
                value = sclone(value);
                word = stok(value, " ,", &tok);
                while (word) {
                    addMatchEtag(conn, word);
                    word = stok(0, " ,", &tok);
                }
            }
            break;

        case 'k':
            /* Keep-Alive: timeout=N, max=1 */
            if (strcasecmp(key, "keep-alive") == 0) {
                keepAlive = 1;
                if ((tok = scontains(value, "max=", -1)) != 0) {
                    conn->keepAliveCount = atoi(&tok[4]);
                    /*  
                        IMPORTANT: Deliberately close the connection one request early. This ensures a client-led 
                        termination and helps relieve server-side TIME_WAIT conditions.
                     */
                    if (conn->keepAliveCount == 1) {
                        conn->keepAliveCount = 0;
                    }
                }
            }
            break;                
                
        case 'l':
            if (strcasecmp(key, "location") == 0) {
                rx->redirect = sclone(value);
            }
            break;

        case 'p':
            if (strcasecmp(key, "pragma") == 0) {
                rx->pragma = sclone(value);
            }
            break;

        case 'r':
            if (strcasecmp(key, "range") == 0) {
                if (!parseRange(conn, value)) {
                    httpError(conn, HTTP_CLOSE | HTTP_CODE_RANGE_NOT_SATISFIABLE, "Bad range");
                }
            } else if (strcasecmp(key, "referer") == 0) {
                /* NOTE: yes the header is misspelt in the spec */
                rx->referrer = sclone(value);
            }
            break;

        case 't':
            if (strcasecmp(key, "transfer-encoding") == 0) {
                if (scasecmp(value, "chunked") == 0) {
                    /*  
                        remainingContent will be revised by the chunk filter as chunks are processed and will 
                        be set to zero when the last chunk has been received.
                     */
                    rx->flags |= HTTP_CHUNKED;
                    rx->chunkState = HTTP_CHUNK_START;
                    rx->remainingContent = MAXINT;
                    rx->needInputPipeline = 1;
                }
            }
            break;

        case 'x':
            if (strcasecmp(key, "x-http-method-override") == 0) {
                httpSetMethod(conn, value);
#if BIT_DEBUG
            } else if (strcasecmp(key, "x-chunk-size") == 0) {
                tx->chunkSize = atoi(value);
                if (tx->chunkSize <= 0) {
                    tx->chunkSize = 0;
                } else if (tx->chunkSize > conn->limits->chunkSize) {
                    tx->chunkSize = conn->limits->chunkSize;
                }
#endif
            }
            break;

        case 'u':
            if (strcasecmp(key, "user-agent") == 0) {
                rx->userAgent = sclone(value);
            }
            break;

        case 'w':
            if (strcasecmp(key, "www-authenticate") == 0) {
                conn->authType = value;
                while (*value && !isspace((uchar) *value)) {
                    value++;
                }
                *value++ = '\0';
                conn->authType = slower(conn->authType);
                if (!parseAuthenticate(conn, value)) {
                    httpError(conn, HTTP_CODE_BAD_REQUEST, "Bad Authentication header");
                    break;
                }
            }
            break;
        }
    }
    /*
        Don't stream input if a form or upload. NOTE: Upload needs the Files[] collection.
     */
    rx->streamInput = !(rx->form || rx->upload);
    if (!keepAlive) {
        conn->keepAliveCount = 0;
    }
    if (!(rx->flags & HTTP_CHUNKED)) {
        /*  
            Step over "\r\n" after headers. 
            Don't do this if chunked so chunking can parse a single chunk delimiter of "\r\nSIZE ...\r\n"
         */
        if (httpGetPacketLength(packet) >= 2) {
            mprAdjustBufStart(content, 2);
        }
    }
}


/*  
    Parse an authentication response (client side only)
 */
static bool parseAuthenticate(HttpConn *conn, char *authDetails)
{
    HttpRx  *rx;
    char    *value, *tok, *key, *dp, *sp;
    int     seenComma;

    rx = conn->rx;
    key = (char*) authDetails;

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
            algorithm, domain, nonce, oqaque, realm, qop, stale
            We don't strdup any of the values as the headers are persistently saved.
         */
        switch (tolower((uchar) *key)) {
        case 'a':
            if (scasecmp(key, "algorithm") == 0) {
                rx->authAlgorithm = sclone(value);
                break;
            }
            break;

        case 'd':
            if (scasecmp(key, "domain") == 0) {
                conn->authDomain = sclone(value);
                break;
            }
            break;

        case 'n':
            if (scasecmp(key, "nonce") == 0) {
                conn->authNonce = sclone(value);
                conn->authNc = 0;
            }
            break;

        case 'o':
            if (scasecmp(key, "opaque") == 0) {
                conn->authOpaque = sclone(value);
            }
            break;

        case 'q':
            if (scasecmp(key, "qop") == 0) {
                conn->authQop = sclone(value);
            }
            break;

        case 'r':
            if (scasecmp(key, "realm") == 0) {
                conn->authRealm = sclone(value);
            }
            break;

        case 's':
            if (scasecmp(key, "stale") == 0) {
                rx->authStale = sclone(value);
                break;
            }

        default:
            /*  For upward compatibility --  ignore keywords we don't understand */
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
    if (strcmp(rx->conn->authType, "basic") == 0) {
        if (conn->authRealm == 0) {
            return 0;
        }
        return 1;
    }
    /* Digest */
    if (conn->authRealm == 0 || conn->authNonce == 0) {
        return 0;
    }
    if (conn->authQop) {
        if (conn->authDomain == 0 || conn->authOpaque == 0 || rx->authAlgorithm == 0 || rx->authStale == 0) {
            return 0;
        }
    }
    return 1;
}


/*
    Called once the HTTP request/response headers have been parsed
 */
static bool processParsed(HttpConn *conn)
{
    HttpRx      *rx;

    rx = conn->rx;
    if (!rx->form && conn->endpoint) {
        /*
            Routes need to be able to access form data, so forms route later after all input is received.
         */
        routeRequest(conn);
    }
    if (rx->streamInput) {
        httpStartPipeline(conn);
    }
    httpSetState(conn, HTTP_STATE_CONTENT);
    if (conn->workerEvent && conn->tx->started && rx->eof) {
        httpSetState(conn, HTTP_STATE_READY);
        return 0;
    }
    return 1;
}


static bool analyseContent(HttpConn *conn, HttpPacket *packet)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpQueue   *q;
    MprBuf      *content;
    ssize       nbytes;

    rx = conn->rx;
    tx = conn->tx;
    content = packet->content;
    q = tx->queue[HTTP_QUEUE_RX];
    mprAssert(httpVerifyQueue(q));

    LOG(7, "processContent: packet of %d bytes, remaining %d", mprGetBufLength(content), rx->remainingContent);
    if ((nbytes = httpFilterChunkData(q, packet)) < 0) {
        return 0;
    }
    mprAssert(nbytes >= 0);

    if (nbytes > 0) {
        rx->remainingContent -= nbytes;
        rx->bytesRead += nbytes;
        if (httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_BODY, tx->ext) >= 0) {
            httpTraceContent(conn, HTTP_TRACE_RX, HTTP_TRACE_BODY, packet, nbytes, rx->bytesRead);
        }
        if (rx->bytesRead >= conn->limits->receiveBodySize) {
            httpError(conn, HTTP_CLOSE | HTTP_CODE_REQUEST_TOO_LARGE, 
                "Request body of %,Ld bytes is too big. Limit %,Ld", rx->bytesRead, conn->limits->receiveBodySize);
            return 1;
        }
        if (rx->form && rx->length >= conn->limits->receiveFormSize) {
            httpError(conn, HTTP_CLOSE | HTTP_CODE_REQUEST_TOO_LARGE, 
                "Request form of %,Ld bytes is too big. Limit %,Ld", rx->bytesRead, conn->limits->receiveFormSize);
            return 1;
        }
        if (packet == rx->headerPacket && nbytes > 0) {
            packet = httpSplitPacket(packet, 0);
        }
        if (httpGetPacketLength(packet) > nbytes) {
            /*  Split excess data belonging to the next chunk or pipelined request */
            LOG(7, "processContent: Split packet of %d at %d", httpGetPacketLength(packet), nbytes);
            conn->input = httpSplitPacket(packet, nbytes);
        } else {
            conn->input = 0;
        }
        if (!(conn->finalized && conn->endpoint)) {
            if (rx->form) {
                httpPutForService(q, packet, HTTP_DELAY_SERVICE);
            } else {
                httpPutPacketToNext(q, packet);
            }
            mprAssert(httpVerifyQueue(q));
        }
    }
    if (rx->remainingContent == 0 && !(rx->flags & HTTP_CHUNKED)) {
        rx->eof = 1;
    }
    return 1;
}


/*  
    Process request body data (typically post or put content)
 */
static bool processContent(HttpConn *conn, HttpPacket *packet)
{
    HttpRx      *rx;
    HttpQueue   *q;

    rx = conn->rx;

    if (packet == 0 || !analyseContent(conn, packet)) {
        return 0;
    }
    if (rx->eof) {
        q = conn->tx->queue[HTTP_QUEUE_RX];
        if (rx->form && conn->endpoint) {
            routeRequest(conn);
            while ((packet = httpGetPacket(q)) != 0) {
                httpPutPacketToNext(q, packet);
            }
        }
        httpPutPacketToNext(q, httpCreateEndPacket());
        if (!rx->streamInput) {
            httpStartPipeline(conn);
        }
        httpSetState(conn, HTTP_STATE_READY);
        return conn->workerEvent ? 0 : 1;
    }
    httpServiceQueues(conn);
    return conn->error || (conn->input ? httpGetPacketLength(conn->input) : 0);
}


/*
    In the ready state after all content has been received
 */
static bool processReady(HttpConn *conn)
{
    httpReadyHandler(conn);
    httpSetState(conn, HTTP_STATE_RUNNING);
    return 1;
}


/*
    Note: may be called multiple times in response to output I/O events
 */
static bool processRunning(HttpConn *conn)
{
    HttpQueue   *q;
    int         canProceed;

    q = conn->writeq;
    canProceed = 1;
    httpServiceQueues(conn);

    if (conn->endpoint) {
        /* Server side */
        if (conn->finalized) {
            if (!conn->connectorComplete) {
                /* Wait for Tx I/O event. Do suspend incase handler not using auto-flow routines */
                conn->writeBlocked = 1;
                httpSuspendQueue(q);
                httpEnableConnEvents(q->conn);
                canProceed = 0;
            }
        } else if (!httpProcessHandler(conn)) {
            /* No process callback defined */
            canProceed = 0;
        } else if (q->count == 0) {
            /* Queue is empty and data may have drained above in httpServiceQueues. Yield to reclaim memory. */
            mprYield(0);
        } else if (q->count < q->low) {
            if (q->flags & HTTP_QUEUE_SUSPENDED) {
                httpResumeQueue(q);
            }
        } else {
            conn->writeBlocked = 1;
            httpSuspendQueue(q);
            httpEnableConnEvents(q->conn);
            canProceed = 0;
        }
    } else {
        /* Client side */
        httpServiceQueues(conn);
        httpFinalize(conn);
        httpSetState(conn, HTTP_STATE_COMPLETE);
    }
    return canProceed;
}


#if BIT_DEBUG
static void measure(HttpConn *conn)
{
    MprTime     elapsed;
    HttpTx      *tx;
    cchar       *uri;
    int         level;

    tx = conn->tx;
    if (conn->rx == 0 || tx == 0) {
        return;
    }
    uri = (conn->endpoint) ? conn->rx->uri : tx->parsedUri->path;
   
    if ((level = httpShouldTrace(conn, HTTP_TRACE_TX, HTTP_TRACE_TIME, tx->ext)) >= 0) {
        elapsed = mprGetTime() - conn->startTime;
#if MPR_HIGH_RES_TIMER
        if (elapsed < 1000) {
            mprLog(level, "TIME: Request %s took %,d msec %,d ticks", uri, elapsed, mprGetTicks() - conn->startTicks);
        } else
#endif
            mprLog(level, "TIME: Request %s took %,d msec", uri, elapsed);
    }
}
#else
#define measure(conn)
#endif


static bool processCompletion(HttpConn *conn)
{
    HttpPacket  *packet;
    HttpRx      *rx;
    bool        more;

    rx = conn->rx;
    mprAssert(conn->state == HTTP_STATE_COMPLETE);

    httpDestroyPipeline(conn);
    measure(conn);
    if (conn->endpoint && rx) {
        if (rx->route && rx->route->log) {
            httpLogRequest(conn);
        }
        httpValidateLimits(conn->endpoint, HTTP_VALIDATE_CLOSE_REQUEST, conn);
        rx->conn = 0;
        conn->tx->conn = 0;
        conn->rx = 0;
        conn->tx = 0;
        packet = conn->input;
        more = packet && !conn->connError && (httpGetPacketLength(packet) > 0);
        if (conn->sock) {
            httpPrepServerConn(conn);
        }
        return more;
    }
    return 0;
}


void httpCloseRx(HttpConn *conn)
{
    if (!conn->rx->eof) {
        /* May not have consumed all read data, so can't be assured the next request will be okay */
        conn->keepAliveCount = -1;
    }
    if (conn->state < HTTP_STATE_COMPLETE && !conn->inHttpProcess) {
        httpPump(conn, NULL);
    }
}


bool httpContentNotModified(HttpConn *conn)
{
    HttpRx      *rx;
    HttpTx      *tx;
    MprTime     modified;
    bool        same;

    rx = conn->rx;
    tx = conn->tx;

    if (rx->flags & HTTP_IF_MODIFIED) {
        /*  
            If both checks, the last modification time and etag, claim that the request doesn't need to be
            performed, skip the transfer.
         */
        mprAssert(tx->fileInfo.valid);
        modified = (MprTime) tx->fileInfo.mtime * MPR_TICKS_PER_SEC;
        same = httpMatchModified(conn, modified) && httpMatchEtag(conn, tx->etag);
        if (tx->outputRanges && !same) {
            tx->outputRanges = 0;
        }
        return same;
    }
    return 0;
}


HttpRange *httpCreateRange(HttpConn *conn, MprOff start, MprOff end)
{
    HttpRange     *range;

    if ((range = mprAllocObj(HttpRange, manageRange)) == 0) {
        return 0;
    }
    range->start = start;
    range->end = end;
    range->len = end - start;
    return range;
}


static void manageRange(HttpRange *range, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(range->next);
    }
}


MprOff httpGetContentLength(HttpConn *conn)
{
    if (conn->rx == 0) {
        mprAssert(conn->rx);
        return 0;
    }
    return conn->rx->length;
}


cchar *httpGetCookies(HttpConn *conn)
{
    if (conn->rx == 0) {
        mprAssert(conn->rx);
        return 0;
    }
    return conn->rx->cookie;
}


cchar *httpGetHeader(HttpConn *conn, cchar *key)
{
    if (conn->rx == 0) {
        mprAssert(conn->rx);
        return 0;
    }
    return mprLookupKey(conn->rx->headers, slower(key));
}


char *httpGetHeadersFromHash(MprHash *hash)
{
    MprKey      *kp;
    char        *headers, *cp;
    ssize       len;

    for (len = 0, kp = 0; (kp = mprGetNextKey(hash, kp)) != 0; ) {
        len += strlen(kp->key) + 2 + strlen(kp->data) + 1;
    }
    if ((headers = mprAlloc(len + 1)) == 0) {
        return 0;
    }
    for (kp = 0, cp = headers; (kp = mprGetNextKey(hash, kp)) != 0; ) {
        strcpy(cp, kp->key);
        cp += strlen(cp);
        *cp++ = ':';
        *cp++ = ' ';
        strcpy(cp, kp->data);
        cp += strlen(cp);
        *cp++ = '\n';
    }
    *cp = '\0';
    return headers;
}


char *httpGetHeaders(HttpConn *conn)
{
    return httpGetHeadersFromHash(conn->rx->headers);
}


MprHash *httpGetHeaderHash(HttpConn *conn)
{
    if (conn->rx == 0) {
        mprAssert(conn->rx);
        return 0;
    }
    return conn->rx->headers;
}


cchar *httpGetQueryString(HttpConn *conn)
{
    return (conn->rx && conn->rx->parsedUri) ? conn->rx->parsedUri->query : 0;
}


int httpGetStatus(HttpConn *conn)
{
    return (conn->rx) ? conn->rx->status : 0;
}


char *httpGetStatusMessage(HttpConn *conn)
{
    return (conn->rx) ? conn->rx->statusMessage : 0;
}


void httpSetMethod(HttpConn *conn, cchar *method)
{
    conn->rx->method = sclone(method);
    parseMethod(conn);
}


int httpSetUri(HttpConn *conn, cchar *uri, cchar *query)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpUri     *prior;

    rx = conn->rx;
    tx = conn->tx;
    prior = rx->parsedUri;

    if ((rx->parsedUri = httpCreateUri(uri, 0)) == 0) {
        return MPR_ERR_BAD_ARGS;
    }
    if (prior) {
        if (rx->parsedUri->scheme == 0) {
            rx->parsedUri->scheme = prior->scheme;
        }
        if (rx->parsedUri->port == 0) {
            rx->parsedUri->port = prior->port;
        }
    }
    if (query == 0 && prior) {
        rx->parsedUri->query = prior->query;
        rx->parsedUri->host = prior->host;
    } else if (*query) {
        rx->parsedUri->query = sclone(query);
    }
    /*
        Start out with no scriptName and the entire URI in the pathInfo. Stages may rewrite.
     */
    rx->uri = rx->parsedUri->path;
    rx->pathInfo = httpNormalizeUriPath(mprUriDecode(rx->parsedUri->path));
    rx->scriptName = mprEmptyString();
    tx->ext = httpGetExt(conn);
    return 0;
}


static void waitHandler(HttpConn *conn, struct MprEvent *event)
{
    httpCallEvent(conn, event->mask);
}


/*
    Wait for the connection to reach a given state.
    @param state Desired state. Set to zero if you want to wait for one I/O event.
    @param timeout Timeout in msec. If timeout is zer, wait forever. If timeout is < 0, use default inactivity 
        and duration timeouts.
 */
int httpWait(HttpConn *conn, int state, MprTime timeout)
{
    MprTime     mark, remaining, inactivityTimeout;
    int         eventMask, saveAsync, justOne, workDone;

    if (state == 0) {
        state = HTTP_STATE_COMPLETE;
        justOne = 1;
    } else {
        justOne = 0;
    }
    if (conn->state <= HTTP_STATE_BEGIN) {
        mprAssert(conn->state >= HTTP_STATE_BEGIN);
        return MPR_ERR_BAD_STATE;
    } 
    if (conn->input && httpGetPacketLength(conn->input) > 0) {
        httpPump(conn, conn->input);
    }
    if (conn->error) {
        return MPR_ERR_BAD_STATE;
    }
    mark = mprGetTime();
    if (mprGetDebugMode()) {
        inactivityTimeout = timeout = MPR_MAX_TIMEOUT;
    } else {
        inactivityTimeout = timeout < 0 ? conn->limits->inactivityTimeout : MPR_MAX_TIMEOUT;
        if (timeout < 0) {
            timeout = conn->limits->requestTimeout;
        }
    }
    saveAsync = conn->async;
    conn->async = 1;

    eventMask = MPR_READABLE;
    if (!conn->connectorComplete) {
        eventMask |= MPR_WRITABLE;
    }
    if (conn->state < state) {
        if (conn->waitHandler == 0) {
            conn->waitHandler = mprCreateWaitHandler(conn->sock->fd, eventMask, conn->dispatcher, waitHandler, conn, 0);
        } else {
            mprWaitOn(conn->waitHandler, eventMask);
        }
    }
    remaining = timeout;
    do {
        workDone = httpServiceQueues(conn);
        if (conn->state < state) {
            mprWaitForEvent(conn->dispatcher, min(inactivityTimeout, remaining));
        }
        if (conn->sock && mprIsSocketEof(conn->sock) && !workDone) {
            break;
        }
        remaining = mprGetRemainingTime(mark, timeout);
    } while (!justOne && !conn->error && conn->state < state && remaining > 0);

    conn->async = saveAsync;
    if (conn->sock == 0 || conn->error) {
        return MPR_ERR_CANT_CONNECT;
    }
    if (!justOne && conn->state < state) {
        return (remaining <= 0) ? MPR_ERR_TIMEOUT : MPR_ERR_CANT_READ;
    }
    return 0;
}


/*  
    Set the connector as write blocked and can't proceed.
 */
void httpSocketBlocked(HttpConn *conn)
{
    mprLog(6, "Socket Blocked");
    conn->writeBlocked = 1;
}


static void addMatchEtag(HttpConn *conn, char *etag)
{
    HttpRx   *rx;

    rx = conn->rx;
    if (rx->etags == 0) {
        rx->etags = mprCreateList(-1, 0);
    }
    mprAddItem(rx->etags, etag);
}


/*  
    Get the next input token. The content buffer is advanced to the next token. This routine always returns a
    non-zero token. The empty string means the delimiter was not found. The delimiter is a string to match and not
    a set of characters. HTTP header header parsing does not work as well using classical strtok parsing as you must
    know when the "/r/n/r/n" body delimiter has been encountered. Strtok will eat such delimiters.
 */
static char *getToken(HttpConn *conn, cchar *delim)
{
    MprBuf  *buf;
    char    *token, *nextToken;
    int     len;

    buf = conn->input->content;
    token = mprGetBufStart(buf);
    nextToken = scontains(mprGetBufStart(buf), delim, mprGetBufLength(buf));
    if (nextToken) {
        *nextToken = '\0';
        len = (int) strlen(delim);
        nextToken += len;
        buf->start = nextToken;
    } else {
        buf->start = mprGetBufEnd(buf);
    }
    return token;
}


/*  
    Match the entity's etag with the client's provided etag.
 */
bool httpMatchEtag(HttpConn *conn, char *requestedEtag)
{
    HttpRx  *rx;
    char    *tag;
    int     next;

    rx = conn->rx;
    if (rx->etags == 0) {
        return 1;
    }
    if (requestedEtag == 0) {
        return 0;
    }
    for (next = 0; (tag = mprGetNextItem(rx->etags, &next)) != 0; ) {
        if (strcmp(tag, requestedEtag) == 0) {
            return (rx->ifMatch) ? 0 : 1;
        }
    }
    return (rx->ifMatch) ? 1 : 0;
}


/*  
    If an IF-MODIFIED-SINCE was specified, then return true if the resource has not been modified. If using
    IF-UNMODIFIED, then return true if the resource was modified.
 */
bool httpMatchModified(HttpConn *conn, MprTime time)
{
    HttpRx   *rx;

    rx = conn->rx;

    if (rx->since == 0) {
        /*  If-Modified or UnModified not supplied. */
        return 1;
    }
    if (rx->ifModified) {
        /*  Return true if the file has not been modified.  */
        return !(time > rx->since);
    } else {
        /*  Return true if the file has been modified.  */
        return (time > rx->since);
    }
}


/*  
    Format is:  Range: bytes=n1-n2,n3-n4,...
    Where n1 is first byte pos and n2 is last byte pos

    Examples:
        Range: 0-49             first 50 bytes
        Range: 50-99,200-249    Two 50 byte ranges from 50 and 200
        Range: -50              Last 50 bytes
        Range: 1-               Skip first byte then emit the rest

    Return 1 if more ranges, 0 if end of ranges, -1 if bad range.
 */
static bool parseRange(HttpConn *conn, char *value)
{
    HttpTx      *tx;
    HttpRange   *range, *last, *next;
    char        *tok, *ep;

    tx = conn->tx;
    value = sclone(value);
    if (value == 0) {
        return 0;
    }
    /*  
        Step over the "bytes="
     */
    stok(value, "=", &value);

    for (last = 0; value && *value; ) {
        if ((range = mprAllocObj(HttpRange, manageRange)) == 0) {
            return 0;
        }
        /*  
            A range "-7" will set the start to -1 and end to 8
         */
        tok = stok(value, ",", &value);
        if (*tok != '-') {
            range->start = (ssize) stoi(tok);
        } else {
            range->start = -1;
        }
        range->end = -1;

        if ((ep = strchr(tok, '-')) != 0) {
            if (*++ep != '\0') {
                /*
                    End is one beyond the range. Makes the math easier.
                 */
                range->end = (ssize) stoi(ep) + 1;
            }
        }
        if (range->start >= 0 && range->end >= 0) {
            range->len = (int) (range->end - range->start);
        }
        if (last == 0) {
            tx->outputRanges = range;
        } else {
            last->next = range;
        }
        last = range;
    }

    /*  
        Validate ranges
     */
    for (range = tx->outputRanges; range; range = range->next) {
        if (range->end != -1 && range->start >= range->end) {
            return 0;
        }
        if (range->start < 0 && range->end < 0) {
            return 0;
        }
        next = range->next;
        if (range->start < 0 && next) {
            /* This range goes to the end, so can't have another range afterwards */
            return 0;
        }
        if (next) {
            if (range->end < 0) {
                return 0;
            }
            if (next->start >= 0 && range->end > next->start) {
                return 0;
            }
        }
    }
    conn->tx->currentRange = tx->outputRanges;
    return (last) ? 1: 0;
}


void httpSetStageData(HttpConn *conn, cchar *key, cvoid *data)
{
    HttpRx      *rx;

    rx = conn->rx;
    if (rx->requestData == 0) {
        rx->requestData = mprCreateHash(-1, 0);
    }
    mprAddKey(rx->requestData, key, data);
}


cvoid *httpGetStageData(HttpConn *conn, cchar *key)
{
    HttpRx      *rx;

    rx = conn->rx;
    if (rx->requestData == 0) {
        return NULL;
    }
    return mprLookupKey(rx->requestData, key);
}


char *httpGetPathExt(cchar *path)
{
    char    *ep, *ext;

    if ((ext = strrchr(path, '.')) != 0) {
        ext = sclone(++ext);
        for (ep = ext; *ep && isalnum((uchar) *ep); ep++) {
            ;
        }
        *ep = '\0';
    }
    return ext;
}


/*
    Get the request extension. Look first at the URI pathInfo. If no extension, look at the filename if defined.
    Return NULL if no extension.
 */
char *httpGetExt(HttpConn *conn)
{
    HttpRx  *rx;
    char    *ext;

    rx = conn->rx;
    if ((ext = httpGetPathExt(rx->pathInfo)) == 0) {
        if (conn->tx->filename) {
            ext = httpGetPathExt(conn->tx->filename);
        }
    }
    return ext;
}


static int compareLang(char **s1, char **s2)
{
    return scmp(*s1, *s2);
}


HttpLang *httpGetLanguage(HttpConn *conn, MprHash *spoken, cchar *defaultLang)
{
    HttpRx      *rx;
    HttpLang    *lang;
    MprList     *list;
    cchar       *accept;
    char        *nextTok, *tok, *quality, *language;
    int         next;

    rx = conn->rx;
    if (rx->lang) {
        return rx->lang;
    }
    if (spoken == 0) {
        return 0;
    }
    list = mprCreateList(-1, 0);
    if ((accept = httpGetHeader(conn, "Accept-Language")) != 0) {
        for (tok = stok(sclone(accept), ",", &nextTok); tok; tok = stok(nextTok, ",", &nextTok)) {
            language = stok(tok, ";", &quality);
            if (quality == 0) {
                quality = "1";
            }
            mprAddItem(list, sfmt("%03d %s", (int) (atof(quality) * 100), language));
        }
        mprSortList(list, compareLang);
        for (next = 0; (language = mprGetNextItem(list, &next)) != 0; ) {
            if ((lang = mprLookupKey(rx->route->languages, &language[4])) != 0) {
                rx->lang = lang;
                return lang;
            }
        }
    }
    if (defaultLang && (lang = mprLookupKey(rx->route->languages, defaultLang)) != 0) {
        rx->lang = lang;
        return lang;
    }
    return 0;
}


/*
    Trim extra path information after the uri extension. This is used by CGI and PHP only. The strategy is to 
    heuristically find the script name in the uri. This is assumed to be the original uri up to and including 
    first path component containing a "." Any path information after that is regarded as extra path.
    WARNING: Extra path is an old, unreliable, CGI specific technique. Do not use directories with embedded periods.
 */
void httpTrimExtraPath(HttpConn *conn)
{
    HttpRx      *rx;
    char        *cp, *extra;
    ssize       len;

    rx = conn->rx;
    if (!(rx->flags & (HTTP_OPTIONS | HTTP_TRACE))) { 
        if ((cp = strchr(rx->pathInfo, '.')) != 0 && (extra = strchr(cp, '/')) != 0) {
            len = extra - rx->pathInfo;
            if (0 < len && len < slen(rx->pathInfo)) {
                rx->extraPath = sclone(&rx->pathInfo[len]);
                rx->pathInfo[len] = '\0';
            }
        }
        if ((cp = strchr(rx->target, '.')) != 0 && (extra = strchr(cp, '/')) != 0) {
            len = extra - rx->target;
            if (0 < len && len < slen(rx->target)) {
                rx->target[len] = '\0';
            }
        }
    }
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
