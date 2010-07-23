/*
    receiver.c -- Http receiver. Parses http requests and client responses.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/***************************** Forward Declarations ***************************/

static void addMatchEtag(HttpConn *conn, char *etag);
static int getChunkPacketSize(HttpConn *conn, MprBuf *buf);
static char *getToken(HttpConn *conn, cchar *delim);
static bool parseAuthenticate(HttpConn *conn, char *authDetails);
static void parseHeaders(HttpConn *conn, HttpPacket *packet);
static bool parseIncoming(HttpConn *conn, HttpPacket *packet);
static bool parseRange(HttpConn *conn, char *value);
static void parseRequestLine(HttpConn *conn, HttpPacket *packet);
static void parseResponseLine(HttpConn *conn, HttpPacket *packet);
static bool processCompletion(HttpConn *conn);
static bool processContent(HttpConn *conn, HttpPacket *packet);
static bool startPipeline(HttpConn *conn);
static bool processPipeline(HttpConn *conn);
static bool servicePipeline(HttpConn *conn);
static void threadRequest(HttpConn *conn);

/*********************************** Code *************************************/

HttpReceiver *httpCreateReceiver(HttpConn *conn)
{
    HttpReceiver    *rec;
    MprHeap         *arena;

    /*  
        Create a request memory arena. From this arena, are all allocations made for this entire request.
        Arenas are scalable, non-thread-safe virtual memory blocks.
     */
    arena  = mprAllocArena(conn, "request", HTTP_REC_MEM, 0, NULL);
    if (arena == 0) {
        return 0;
    }
    rec = mprAllocObjZeroed(arena, HttpReceiver);
    if (rec == 0) {
        return 0;
    }
    rec->conn = conn;
    rec->arena = arena;
    rec->length = -1;
    rec->ifMatch = 1;
    rec->ifModified = 1;
    rec->remainingContent = 0;
    rec->method = 0;
    rec->pathInfo = mprStrdup(rec, "/");
    rec->scriptName = mprStrdup(rec, "");
    rec->status = 0;
    rec->statusMessage = "";
    rec->mimeType = "";
    rec->needInputPipeline = !conn->server;
    rec->headers = mprCreateHash(rec, HTTP_SMALL_HASH_SIZE);
    mprSetHashCase(rec->headers, 0);
    return rec;
}


void httpDestroyReceiver(HttpConn *conn)
{
    if (conn->receiver) {
        mprFree(conn->receiver->arena);
        conn->receiver = 0;
    }
    if (conn->server) {
        httpPrepServerConn(conn);
    }
    if (conn->input) {
        /* Left over packet */
        if (mprGetParent(conn->input) != conn) {
            conn->input = httpSplitPacket(conn, conn->input, 0);
        }
    }
}


/*  
    Process incoming requests. This will process as many requests as possible before returning. All socket I/O is
    non-blocking, and this routine must not block. Note: packet may be null.
 */
void httpAdvanceReceiver(HttpConn *conn, HttpPacket *packet)
{
    mprAssert(conn);

    conn->canProceed = 1;

    while (conn->canProceed) {
        LOG(conn, 7, "httpAdvanceReceiver, state %d, error %d", conn->state, conn->error);

        switch (conn->state) {
        case HTTP_STATE_BEGIN:
        case HTTP_STATE_STARTED:
        case HTTP_STATE_WAIT:
            conn->canProceed = parseIncoming(conn, packet);
            break;

        case HTTP_STATE_PARSED:
            conn->canProceed = startPipeline(conn);
            break;

        case HTTP_STATE_CONTENT:
            conn->canProceed = processContent(conn, packet);
            break;

        case HTTP_STATE_PROCESS:
            conn->canProceed = processPipeline(conn);
            break;

        case HTTP_STATE_RUNNING:
            conn->canProceed = servicePipeline(conn);
            break;

        case HTTP_STATE_ERROR:
            conn->canProceed = httpServiceQueues(conn);
            break;

        case HTTP_STATE_COMPLETE:
            conn->canProceed = processCompletion(conn);
            break;
        }
        packet = conn->input;
    }
}


/*  
    Parse the incoming http message. Return true to keep going with this or subsequent request, zero means
    insufficient data to proceed.
 */
static bool parseIncoming(HttpConn *conn, HttpPacket *packet)
{
    HttpReceiver    *rec;
    HttpTransmitter *trans;
    HttpLocation    *location;
    char            *start, *end;
    int             len;

    if (conn->receiver == NULL) {
        conn->receiver = httpCreateReceiver(conn);
        conn->transmitter = httpCreateTransmitter(conn, NULL);
    }
    rec = conn->receiver;
    trans = conn->transmitter;

    if ((len = mprGetBufLength(packet->content)) == 0) {
        return 0;
    }
    start = mprGetBufStart(packet->content);
    if ((end = mprStrnstr(start, "\r\n\r\n", len)) == 0) {
        return 0;
    }
    len = end - start;
    mprAddNullToBuf(packet->content);

    if (len >= conn->limits->headerSize) {
        httpLimitError(conn, HTTP_CODE_REQUEST_TOO_LARGE, "Header too big");
        return 0;
    }
    if (conn->server) {
        parseRequestLine(conn, packet);
    } else {
        parseResponseLine(conn, packet);
    }
    parseHeaders(conn, packet);

    if (conn->server) {
        httpSetState(conn, HTTP_STATE_PARSED);        
        location = (rec->location) ? rec->location : conn->server->location;
        httpCreatePipeline(conn, rec->location, trans->handler);
        //  MOB -- TODO
        if (0 && trans->handler->flags & HTTP_STAGE_THREAD && !conn->threaded) {
            threadRequest(conn);
            return 0;
        }
    } else {
        if (!(100 <= rec->status && rec->status < 200))
            httpSetState(conn, HTTP_STATE_PARSED);        
        }
    return 1;
}


/*  
    Parse the first line of a http request. Return true if the first line parsed. This is only called once all the headers
    have been read and buffered. Requests look like: METHOD URL HTTP/1.X.
 */
static void parseRequestLine(HttpConn *conn, HttpPacket *packet)
{
    HttpReceiver        *rec;
    HttpTransmitter     *trans;
    MprBuf              *content;
    cchar               *endp;
    char                *method, *uri, *protocol;
    int                 methodFlags, mask, len;

    mprLog(conn, 4, "New request from %s:%d to %s:%d", conn->ip, conn->port, conn->sock->ip, conn->sock->port);

    rec = conn->receiver;
    trans = conn->transmitter;
    protocol = uri = 0;
    methodFlags = 0;
    method = getToken(conn, " ");

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

    case 'P':
        if (strcmp(method, "POST") == 0) {
            methodFlags = HTTP_POST;
            rec->needInputPipeline = 1;

        } else if (strcmp(method, "PUT") == 0) {
            methodFlags = HTTP_PUT;
            rec->needInputPipeline = 1;
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
            httpOmitBody(conn);
        }
        break;

    case 'T':
        if (strcmp(method, "TRACE") == 0) {
            methodFlags = HTTP_TRACE;
            httpOmitBody(conn);
        }
        break;
    }
    if (methodFlags == 0) {
        httpProtocolError(conn, HTTP_CODE_BAD_METHOD, "Unknown method");
    }

    uri = getToken(conn, " ");
    if (*uri == '\0') {
        httpProtocolError(conn, HTTP_CODE_BAD_REQUEST, "Bad HTTP request. Empty URI");
    } else if ((int) strlen(uri) >= conn->limits->uriSize) {
        httpError(conn, HTTP_CODE_REQUEST_URL_TOO_LARGE, "Bad request. URI too long");
    }

    protocol = getToken(conn, "\r\n");
    if (strcmp(protocol, "HTTP/1.0") == 0) {
        conn->keepAliveCount = 0;
        if (methodFlags & (HTTP_POST|HTTP_PUT)) {
            rec->remainingContent = MAXINT;
            rec->needInputPipeline = 1;
        }
        conn->legacy = 1;
        conn->protocol = "HTTP/1.0";
    } else if (strcmp(protocol, "HTTP/1.1") != 0) {
        httpProtocolError(conn, HTTP_CODE_NOT_ACCEPTABLE, "Unsupported HTTP protocol");
    }
    rec->flags |= methodFlags;
    rec->method = method;

    if (httpSetUri(conn, uri) < 0) {
        httpProtocolError(conn, HTTP_CODE_BAD_REQUEST, "Bad URL format");
    }
    httpSetState(conn, HTTP_STATE_FIRST);

    conn->traceMask = httpSetupTrace(conn, conn->transmitter->extension);
    if (conn->traceMask) {
        mask = HTTP_TRACE_RECEIVE | HTTP_TRACE_HEADERS;
        if (httpShouldTrace(conn, mask)) {
            mprLog(conn, conn->traceLevel, "\n@@@ New request from %s:%d to %s:%d\n%s %s %s", 
                conn->ip, conn->port, conn->sock->ip, conn->sock->port, method, uri, protocol);
            content = packet->content;
            endp = strstr((char*) content->start, "\r\n\r\n");
            len = (endp) ? (endp - mprGetBufStart(content) + 4) : 0;
            httpTraceContent(conn, packet, len, 0, mask);
        }
    } else {
        mprLog(rec, 2, "%s %s %s", method, uri, protocol);
    }
}


/*  
    Parse the first line of a http response. Return true if the first line parsed. This is only called once all the headers
    have been read and buffered. Response status lines look like: HTTP/1.X CODE Message
 */
static void parseResponseLine(HttpConn *conn, HttpPacket *packet)
{
    HttpReceiver    *rec;
    HttpTransmitter *trans;
    MprBuf          *content;
    cchar           *endp;
    char            *protocol, *status;
    int             len;

    rec = conn->receiver;
    trans = conn->transmitter;

    mprLog(rec, 4, "Response from %s:%d to %s:%d", conn->ip, conn->port, conn->sock->ip, conn->sock->port);
    protocol = getToken(conn, " ");
    if (strcmp(protocol, "HTTP/1.0") == 0) {
        conn->keepAliveCount = 0;
        conn->legacy = 1;
        conn->protocol = "HTTP/1.0";
    } else if (strcmp(protocol, "HTTP/1.1") != 0) {
        httpProtocolError(conn, HTTP_CODE_NOT_ACCEPTABLE, "Unsupported HTTP protocol");
    }

    status = getToken(conn, " ");
    if (*status == '\0') {
        httpProtocolError(conn, HTTP_CODE_NOT_ACCEPTABLE, "Bad response status code");
    }
    rec->status = atoi(status);
    rec->statusMessage = getToken(conn, "\r\n");

    if ((int) strlen(rec->statusMessage) >= conn->limits->uriSize) {
        httpError(conn, HTTP_CODE_REQUEST_URL_TOO_LARGE, "Bad response. Status message too long");
    }
    if (httpShouldTrace(conn, HTTP_TRACE_RECEIVE | HTTP_TRACE_HEADERS)) {
        content = packet->content;
        endp = strstr((char*) content->start, "\r\n\r\n");
        len = (endp) ? (endp - mprGetBufStart(content) + 4) : 0;
        httpTraceContent(conn, packet, len, 0, HTTP_TRACE_RECEIVE | HTTP_TRACE_HEADERS);
    }
}


/*  
    Parse the request headers. Return true if the header parsed.
 */
static void parseHeaders(HttpConn *conn, HttpPacket *packet)
{
    Http            *http;
    HttpReceiver    *rec;
    HttpTransmitter *trans;
    HttpLimits      *limits;
    MprBuf          *content;
    char            *key, *value, *tok, *tp;
    cchar           *oldValue;
    int             len, count, keepAlive;

    http = conn->http;
    rec = conn->receiver;
    trans = conn->transmitter;
    content = packet->content;
    conn->receiver->headerPacket = packet;
    limits = conn->limits;
    keepAlive = 0;

    for (count = 0; content->start[0] != '\r' && !conn->error; count++) {
        if (count >= limits->headerCount) {
            httpLimitError(conn, HTTP_CODE_BAD_REQUEST, "Too many headers");
            break;
        }
        if ((key = getToken(conn, ":")) == 0 || *key == '\0') {
            httpProtocolError(conn, HTTP_CODE_BAD_REQUEST, "Bad header format");
            break;
        }
        value = getToken(conn, "\r\n");
        while (isspace((int) *value)) {
            value++;
        }
        mprStrLower(key);

        LOG(rec, 8, "Key %s, value %s", key, value);
        if (strspn(key, "%<>/\\") > 0) {
            httpProtocolError(conn, HTTP_CODE_BAD_REQUEST, "Bad header key value");
            break;
        }
        if ((oldValue = mprLookupHash(rec->headers, key)) != 0) {
            mprAddHash(rec->headers, key, mprAsprintf(rec->headers, -1, "%s, %s", oldValue, value));
        } else {
            mprAddHash(rec->headers, key, value);
        }

        switch (key[0]) {
        case 'a':
            if (strcmp(key, "authorization") == 0) {
                value = mprStrdup(rec, value);
                rec->authType = mprStrTok(value, " \t", &tok);
                rec->authDetails = tok;

            } else if (strcmp(key, "accept-charset") == 0) {
                rec->acceptCharset = value;

            } else if (strcmp(key, "accept") == 0) {
                rec->accept = value;

            } else if (strcmp(key, "accept-encoding") == 0) {
                rec->acceptEncoding = value;
            }
            break;

        case 'c':
            if (strcmp(key, "content-length") == 0) {
                if (rec->length >= 0) {
                    httpProtocolError(conn, HTTP_CODE_BAD_REQUEST, "Mulitple content length headers");
                    break;
                }
                rec->length = atoi(value);
                if (rec->length < 0) {
                    httpProtocolError(conn, HTTP_CODE_BAD_REQUEST, "Bad content length");
                    break;
                }
                if (rec->length >= conn->limits->receiveBodySize) {
                    httpLimitError(conn, HTTP_CODE_REQUEST_TOO_LARGE,
                        "Request content length %d is too big. Limit %d", rec->length, conn->limits->receiveBodySize);
                    break;
                }
                rec->contentLength = value;
                mprAssert(rec->length >= 0);
                if (conn->server || strcmp(trans->method, "HEAD") != 0) {
                    rec->remainingContent = rec->length;
                    rec->needInputPipeline = 1;
                }

            } else if (strcmp(key, "content-range") == 0) {
                /*
                    This headers specifies the range of any posted body data
                    Format is:  Content-Range: bytes n1-n2/length
                    Where n1 is first byte pos and n2 is last byte pos
                 */
                char    *sp;
                int     start, end, size;

                start = end = size = -1;
                sp = value;
                while (*sp && !isdigit((int) *sp)) {
                    sp++;
                }
                if (*sp) {
                    start = (int) mprAtoi(sp, 10);

                    if ((sp = strchr(sp, '-')) != 0) {
                        end = (int) mprAtoi(++sp, 10);
                    }
                    if ((sp = strchr(sp, '/')) != 0) {
                        /*
                            Note this is not the content length transmitted, but the original size of the input of which
                            the client is transmitting only a portion.
                         */
                        size = (int) mprAtoi(++sp, 10);
                    }
                }
                if (start < 0 || end < 0 || size < 0 || end <= start) {
                    httpError(conn, HTTP_CODE_RANGE_NOT_SATISFIABLE, "Bad content range");
                    break;
                }
                rec->inputRange = httpCreateRange(conn, start, end);

            } else if (strcmp(key, "content-type") == 0) {
                rec->mimeType = value;
                rec->form = strstr(rec->mimeType, "application/x-www-form-urlencoded") != 0;

            } else if (strcmp(key, "cookie") == 0) {
                if (rec->cookie && *rec->cookie) {
                    rec->cookie = mprStrcat(rec, -1, rec->cookie, "; ", value, NULL);
                } else {
                    rec->cookie = value;
                }

            } else if (strcmp(key, "connection") == 0) {
                rec->connection = value;
                if (mprStrcmpAnyCase(value, "KEEP-ALIVE") == 0) {
                    keepAlive++;
                } else if (mprStrcmpAnyCase(value, "CLOSE") == 0) {
                    conn->keepAliveCount = -1;
                }
            }
            break;

        case 'h':
            if (strcmp(key, "host") == 0) {
                rec->hostName = value;
            }
            break;

        case 'i':
            if ((strcmp(key, "if-modified-since") == 0) || (strcmp(key, "if-unmodified-since") == 0)) {
                MprTime     newDate = 0;
                char        *cp;
                bool        ifModified = (key[3] == 'M');

                if ((cp = strchr(value, ';')) != 0) {
                    *cp = '\0';
                }
                if (mprParseTime(conn, &newDate, value, MPR_UTC_TIMEZONE, NULL) < 0) {
                    mprAssert(0);
                    break;
                }
                if (newDate) {
                    rec->since = newDate;
                    rec->ifModified = ifModified;
                    rec->flags |= HTTP_REC_IF_MODIFIED;
                }

            } else if ((strcmp(key, "if-match") == 0) || (strcmp(key, "if-none-match") == 0)) {
                char    *word, *tok;
                bool    ifMatch = key[3] == 'M';

                if ((tok = strchr(value, ';')) != 0) {
                    *tok = '\0';
                }
                rec->ifMatch = ifMatch;
                rec->flags |= HTTP_REC_IF_MODIFIED;
                value = mprStrdup(conn, value);
                word = mprStrTok(value, " ,", &tok);
                while (word) {
                    addMatchEtag(conn, word);
                    word = mprStrTok(0, " ,", &tok);
                }

            } else if (strcmp(key, "if-range") == 0) {
                char    *word, *tok;

                if ((tok = strchr(value, ';')) != 0) {
                    *tok = '\0';
                }
                rec->ifMatch = 1;
                rec->flags |= HTTP_REC_IF_MODIFIED;
                value = mprStrdup(conn, value);
                word = mprStrTok(value, " ,", &tok);
                while (word) {
                    addMatchEtag(conn, word);
                    word = mprStrTok(0, " ,", &tok);
                }
            }
            break;

        case 'k':
            if (strcmp(key, "keep-alive") == 0) {
                /*
                    Keep-Alive: timeout=N, max=1
                 */
                len = (int) strlen(value);
                if (len > 2 && value[len - 1] == '1' && value[len - 2] == '=' && tolower((int)(value[len - 3])) == 'x') {

                    /*  
                        IMPORTANT: Deliberately close the connection one request early. This ensures a client-led 
                        termination and helps relieve server-side TIME_WAIT conditions.
                     */
                    conn->keepAliveCount = 0;
                }
            }
            break;                
                
        case 'l':
            if (strcmp(key, "location") == 0) {
                rec->redirect = value;
            }
            break;

        case 'p':
            if (strcmp(key, "pragma") == 0) {
                rec->pragma = value;
            }
            break;

        case 'r':
            if (strcmp(key, "range") == 0) {
                if (!parseRange(conn, value)) {
                    httpError(conn, HTTP_CODE_RANGE_NOT_SATISFIABLE, "Bad range");
                }
            } else if (strcmp(key, "referer") == 0) {
                rec->referer = value;
            }
            break;

        case 't':
            if (strcmp(key, "transfer-encoding") == 0) {
                mprStrLower(value);
                if (strcmp(value, "chunked") == 0) {
                    rec->flags |= HTTP_REC_CHUNKED;
                    /*  
                        This will be revised by the chunk filter as chunks are processed and will be set to zero when the
                        last chunk has been received.
                     */
                    rec->remainingContent = MAXINT;
                    rec->needInputPipeline = 1;
                }
            }
            break;

#if BLD_DEBUG
        case 'x':
            if (strcmp(key, "x-appweb-chunk-size") == 0) {
                trans->chunkSize = atoi(value);
                if (trans->chunkSize <= 0) {
                    trans->chunkSize = 0;
                } else if (trans->chunkSize > conn->limits->chunkSize) {
                    trans->chunkSize = conn->limits->chunkSize;
                }
            }
            break;
#endif

        case 'u':
            if (strcmp(key, "user-agent") == 0) {
                rec->userAgent = value;
            }
            break;

        case 'w':
            if (strcmp(key, "www-authenticate") == 0) {
                tp = value;
                while (*value && !isspace((int) *value)) {
                    value++;
                }
                *value++ = '\0';
                mprStrLower(tp);
                mprFree(conn->authType);
                conn->authType = mprStrdup(conn, tp);
                if (!parseAuthenticate(conn, value)) {
                    httpError(conn, HTTP_CODE_BAD_REQUEST, "Bad Authentication header");
                    break;
                }
            }
            break;
        }
    }
    if (conn->protocol == 0 && !keepAlive) {
        conn->keepAliveCount = 0;
    }
    if (!(rec->flags & HTTP_REC_CHUNKED)) {
        /*  
            Step over "\r\n" after headers. As an optimization, don't do this if chunked so chunking can parse a single
            chunk delimiter of "\r\nSIZE ...\r\n"
         */
        if (mprGetBufLength(content) >= 2) {
            mprAdjustBufStart(content, 2);
        }
    }
    if (rec->remainingContent == 0) {
        rec->readComplete = 1;
    }
}


/*  
    Parse an authentication response (client side only)
 */
static bool parseAuthenticate(HttpConn *conn, char *authDetails)
{
    HttpReceiver    *rec;
    char            *value, *tok, *key, *dp, *sp;
    int             seenComma;

    rec = conn->receiver;
    key = (char*) authDetails;

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
            algorithm, domain, nonce, oqaque, realm, qop, stale
            We don't strdup any of the values as the headers are persistently saved.
         */
        switch (tolower((int) *key)) {
        case 'a':
            if (mprStrcmpAnyCase(key, "algorithm") == 0) {
                mprFree(rec->authAlgorithm);
                rec->authAlgorithm = value;
                break;
            }
            break;

        case 'd':
            if (mprStrcmpAnyCase(key, "domain") == 0) {
                mprFree(conn->authDomain);
                conn->authDomain = mprStrdup(conn, value);
                break;
            }
            break;

        case 'n':
            if (mprStrcmpAnyCase(key, "nonce") == 0) {
                mprFree(conn->authNonce);
                conn->authNonce = mprStrdup(conn, value);
                conn->authNc = 0;
            }
            break;

        case 'o':
            if (mprStrcmpAnyCase(key, "opaque") == 0) {
                mprFree(conn->authOpaque);
                conn->authOpaque = mprStrdup(conn, value);
            }
            break;

        case 'q':
            if (mprStrcmpAnyCase(key, "qop") == 0) {
                mprFree(conn->authQop);
                conn->authQop = mprStrdup(conn, value);
            }
            break;

        case 'r':
            if (mprStrcmpAnyCase(key, "realm") == 0) {
                mprFree(conn->authRealm);
                conn->authRealm = mprStrdup(conn, value);
            }
            break;

        case 's':
            if (mprStrcmpAnyCase(key, "stale") == 0) {
                rec->authStale = mprStrdup(rec, value);
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
    if (strcmp(rec->conn->authType, "basic") == 0) {
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
        if (conn->authDomain == 0 || conn->authOpaque == 0 || rec->authAlgorithm == 0 || rec->authStale == 0) {
            return 0;
        }
    }
    return 1;
}


static void httpThreadEvent(HttpConn *conn)
{
    httpCallEvent(conn, 0);
}


static void threadRequest(HttpConn *conn)
{
    mprAssert(!conn->dispatcher->enabled);
    mprAssert(conn->dispatcher != conn->server->dispatcher);

    conn->threaded = 1;
    conn->startingThread = 1;
    mprInitEvent(conn->dispatcher, &conn->runEvent, "runEvent", 0, (MprEventProc) httpThreadEvent, conn, 0);
    mprQueueEvent(conn->dispatcher, &conn->runEvent);
    mprAssert(!conn->dispatcher->enabled);
}


static bool startPipeline(HttpConn *conn)
{
    HttpReceiver    *rec;
    HttpTransmitter *trans;

    rec = conn->receiver;
    trans = conn->transmitter;

    httpStartPipeline(conn);

    if (conn->state < HTTP_STATE_COMPLETE) {
        if (conn->connError) {
            httpCompleteRequest(conn);
        } else if (rec->remainingContent > 0) {
            httpSetState(conn, HTTP_STATE_CONTENT);
        } else if (conn->server) {
            httpSetState(conn, HTTP_STATE_PROCESS);
        } else {
            httpCompleteRequest(conn);
        }
        if (!trans->writeComplete) {
            HTTP_NOTIFY(conn, 0, HTTP_NOTIFY_WRITABLE);
        }
    }
    return 1;
}


/*  
    Process request body data (typically post or put content). Packet will be null if the client closed the
    connection to signify end of data.
 */
static bool processContent(HttpConn *conn, HttpPacket *packet)
{
    HttpReceiver    *rec;
    HttpTransmitter *trans;
    HttpQueue       *q;
    MprBuf          *content;
    int             nbytes, remaining, mask;

    rec = conn->receiver;
    trans = conn->transmitter;
    q = &trans->queue[HTTP_QUEUE_RECEIVE];

    mprAssert(packet);
    if (packet == 0) {
        return 0;
    }
    content = packet->content;
    if (rec->flags & HTTP_REC_CHUNKED) {
        if ((remaining = getChunkPacketSize(conn, content)) == 0) {
            /* Need more data or bad chunk specification */
            if (mprGetBufLength(content) > 0) {
                conn->input = packet;
            }
            return 0;
        }
    } else {
        remaining = rec->remainingContent;
    }
    nbytes = min(remaining, mprGetBufLength(content));
    mprAssert(nbytes >= 0);

    mask = HTTP_TRACE_BODY | HTTP_TRACE_RECEIVE;
    if (httpShouldTrace(conn, mask)) {
        httpTraceContent(conn, packet, 0, 0, mask);
    }
    LOG(conn, 7, "processContent: packet of %d bytes, remaining %d", mprGetBufLength(content), remaining);

    if (nbytes > 0) {
        mprAssert(httpGetPacketLength(packet) > 0);
        remaining -= nbytes;
        rec->remainingContent -= nbytes;
        rec->receivedContent += nbytes;

        if (rec->receivedContent >= conn->limits->receiveBodySize) {
            httpLimitError(conn, HTTP_CODE_REQUEST_TOO_LARGE, "Request content body is too big %d vs limit %d",
                rec->receivedContent, conn->limits->receiveBodySize);
            return 1;
        }
        if (packet == rec->headerPacket) {
            /* Preserve headers if more data to come. Otherwise handlers may free the packet and destory the headers */
            packet = httpSplitPacket(conn, packet, 0);
        } else {
            mprStealBlock(trans, packet);
        }
        conn->input = 0;
        if (remaining == 0 && mprGetBufLength(packet->content) > nbytes) {
            /*  Split excess data belonging to the next pipelined request.  */
            LOG(conn, 7, "processContent: Split packet of %d at %d", httpGetPacketLength(packet), nbytes);
            conn->input = httpSplitPacket(conn, packet, nbytes);
        }
        if ((q->count + httpGetPacketLength(packet)) > q->max) {
            /*  
                MOB -- should flow control instead
                httpLimitError(q->conn, HTTP_CODE_REQUEST_TOO_LARGE, "Too much body data");
            */
            return 0;
        }
        if (conn->error) {
            /* Discard input data if the request has an error */
            mprFree(packet);
        } else {
            httpSendPacketToNext(q, packet);
        }

    } else {
        if (conn->input != rec->headerPacket) {
            mprFree(packet);
        }
        conn->input = 0;
    }
    if (rec->remainingContent == 0) {
        if (rec->remainingContent > 0 && !conn->legacy) {
            httpProtocolError(conn, HTTP_CODE_COMMS_ERROR, "Insufficient content data sent with request");
            httpSetState(conn, HTTP_STATE_ERROR);
        } else if (!(rec->flags & HTTP_REC_CHUNKED) || (rec->chunkState == HTTP_CHUNK_EOF)) {
            rec->readComplete = 1;
            httpSendPacketToNext(q, httpCreateEndPacket(rec));
            httpSetState(conn, HTTP_STATE_PROCESS);
        }
        return 1;
    }
    httpServiceQueues(conn);
    return conn->connError || (conn->input ? mprGetBufLength(conn->input->content) : 0);
}


static bool processPipeline(HttpConn *conn)
{
    httpProcessPipeline(conn);
    if (conn->error) {
        httpSetState(conn, HTTP_STATE_ERROR);
    }
    httpSetState(conn, (conn->connError) ? HTTP_STATE_COMPLETE : HTTP_STATE_RUNNING);
    return 1;
}


static bool servicePipeline(HttpConn *conn)
{
    HttpTransmitter     *trans;
    int                 canProceed;

    trans = conn->transmitter;

    canProceed = httpServiceQueues(conn);
    if (conn->state < HTTP_STATE_COMPLETE) {
        if (conn->server) {
            if (trans->writeComplete) {
                httpSetState(conn, HTTP_STATE_COMPLETE);
                canProceed = 1;
            } else {
                //  MOB -- should only issue this on transitions
                HTTP_NOTIFY(conn, 0, HTTP_NOTIFY_WRITABLE);
                canProceed = httpServiceQueues(conn);
            }
        } else {
            httpCompleteRequest(conn);
            canProceed = 1;
        }
    }
    return canProceed;
}


static bool processCompletion(HttpConn *conn)
{
    HttpReceiver        *rec;
    HttpTransmitter     *trans;
    HttpPacket          *packet;
    Mpr                 *mpr;
    bool                more;

    mprAssert(conn->state == HTTP_STATE_COMPLETE);

    rec = conn->receiver;
    trans = conn->transmitter;
    mpr = mprGetMpr(conn);

    mprLog(rec, 4, "Request complete used %,d K, mpr usage %,d K, page usage %,d K",
        rec->arena->allocBytes / 1024, mpr->heap.allocBytes / 1024, mpr->pageHeap.allocBytes / 1024);

    packet = conn->input;
    more = packet && (mprGetBufLength(packet->content) > 0);
    if (mprGetParent(packet) != conn) {
        if (more) {
            conn->input = httpSplitPacket(conn, packet, 0);
        } else {
            conn->input = 0;
        }
    }
    if (conn->server) {
        httpDestroyReceiver(conn);
        return more;
    } else {
        return 0;
    }
}


/*  
    Optimization to correctly size the packets to the chunk filter.
 */
static int getChunkPacketSize(HttpConn *conn, MprBuf *buf)
{
    HttpReceiver    *rec;
    char            *start, *cp;
    int             need, size;

    rec = conn->receiver;
    need = 0;

    switch (rec->chunkState) {
    case HTTP_CHUNK_DATA:
        need = rec->remainingContent;
        if (need != 0) {
            break;
        }
        /* Fall through */

    case HTTP_CHUNK_START:
        start = mprGetBufStart(buf);
        if (mprGetBufLength(buf) < 3) {
            return 0;
        }
        if (start[0] != '\r' || start[1] != '\n') {
            httpProtocolError(conn, HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
            return 0;
        }
        for (cp = &start[2]; cp < (char*) buf->end && *cp != '\n'; cp++) {}
        if ((cp - start) < 2 || (cp[-1] != '\r' || cp[0] != '\n')) {
            /* Insufficient data */
            if ((cp - start) > 80) {
                httpProtocolError(conn, HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
                return 0;
            }
            return 0;
        }
        need = cp - start + 1;
        size = (int) mprAtoi(&start[2], 16);
        if (size == 0 && &cp[2] < buf->end && cp[1] == '\r' && cp[2] == '\n') {
            /*
                This is the last chunk (size == 0). Now need to consume the trailing "\r\n".
                We are lenient if the request does not have the trailing "\r\n" as required by the spec.
             */
            need += 2;
        }
        break;

    default:
        mprAssert(0);
    }
    rec->remainingContent = need;
    return need;
}


bool httpContentNotModified(HttpConn *conn)
{
    HttpReceiver    *rec;
    HttpTransmitter       *trans;
    bool            same;

    rec = conn->receiver;
    trans = conn->transmitter;

    if (rec->flags & HTTP_REC_IF_MODIFIED) {
        /*  
            If both checks, the last modification time and etag, claim that the request doesn't need to be
            performed, skip the transfer. TODO - need to check if fileInfo is actually set.
         */
        same = httpMatchModified(conn, trans->fileInfo.mtime) && httpMatchEtag(conn, trans->etag);
        if (rec->ranges && !same) {
            /*
                Need to transfer the entire resource
             */
            mprFree(rec->ranges);
            rec->ranges = 0;
        }
        return same;
    }
    return 0;
}


HttpRange *httpCreateRange(HttpConn *conn, int start, int end)
{
    HttpRange     *range;

    range = mprAllocObjZeroed(conn->receiver, HttpRange);
    if (range == 0) {
        return 0;
    }
    range->start = start;
    range->end = end;
    range->len = end - start;

    return range;
}


int httpGetContentLength(HttpConn *conn)
{
    if (conn->receiver == 0) {
        mprAssert(conn->receiver);
        return 0;
    }
    return conn->receiver->length;
    return 0;
}


cchar *httpGetCookies(HttpConn *conn)
{
    if (conn->receiver == 0) {
        mprAssert(conn->receiver);
        return 0;
    }
    return conn->receiver->cookie;
}


cchar *httpGetHeader(HttpConn *conn, cchar *key)
{
    cchar   *value;
    char    *lower;

    if (conn->receiver == 0) {
        mprAssert(conn->receiver);
        return 0;
    }
    lower = mprStrdup(conn, key);
    mprStrLower(lower);
    value = mprLookupHash(conn->receiver->headers, lower);
    mprFree(lower);
    return value;
}


char *httpGetHeaders(HttpConn *conn)
{
    HttpReceiver    *rec;
    MprHash         *hp;
    char            *headers, *key, *cp;
    int             len;

    if (conn->receiver == 0) {
        mprAssert(conn->receiver);
        return 0;
    }
    rec = conn->receiver;
    headers = 0;
    for (len = 0, hp = mprGetFirstHash(rec->headers); hp; ) {
        headers = mprReallocStrcat(rec, -1, headers, hp->key, NULL);
        key = &headers[len];
        for (cp = &key[1]; *cp; cp++) {
            *cp = tolower((int) *cp);
            if (*cp == '-') {
                cp++;
            }
        }
        headers = mprReallocStrcat(rec, -1, headers, ": ", hp->data, "\n", NULL);
        len = strlen(headers);
        hp = mprGetNextHash(rec->headers, hp);
    }
    return headers;
}


MprHashTable *httpGetHeaderHash(HttpConn *conn)
{
    if (conn->receiver == 0) {
        mprAssert(conn->receiver);
        return 0;
    }
    return conn->receiver->headers;
}


cchar *httpGetQueryString(HttpConn *conn)
{
    return conn->receiver->parsedUri->query;
}


int httpGetStatus(HttpConn *conn)
{
    return conn->receiver->status;
}


char *httpGetStatusMessage(HttpConn *conn)
{
    return conn->receiver->statusMessage;
}


int httpSetUri(HttpConn *conn, cchar *uri)
{
    HttpReceiver   *rec;

    rec = conn->receiver;

    /*  
        Parse and tokenize the uri. Then decode and validate the URI path portion.
     */
    rec->parsedUri = httpCreateUri(rec, uri, 0);
    if (rec->parsedUri == 0) {
        return MPR_ERR_BAD_ARGS;
    }

    /*
        Start out with no scriptName and the entire URI in the pathInfo. Stages may rewrite.
     */
    rec->uri = rec->parsedUri->uri;
    conn->transmitter->extension = rec->parsedUri->ext;
    mprFree(rec->pathInfo);
    rec->pathInfo = httpNormalizeUriPath(rec, mprUriDecode(rec, rec->parsedUri->path));
    rec->scriptName = mprStrdup(rec, "");
    return 0;
}


/*  
    Wait for the Http object to achieve a given state.
    NOTE: timeout is an inactivity timeout
 */
int httpWait(HttpConn *conn, int state, int inactivityTimeout)
{
    Http            *http;
    HttpTransmitter *trans;
    MprTime         expire;
    int             events, remainingTime, fd;

    http = conn->http;
    trans = conn->transmitter;

    if (inactivityTimeout < 0) {
        inactivityTimeout = conn->inactivityTimeout;
    }
    if (inactivityTimeout <= 0) {
        inactivityTimeout = MAXINT;
    }
    if (conn->state <= HTTP_STATE_BEGIN) {
        mprAssert(conn->state >= HTTP_STATE_BEGIN);
        return MPR_ERR_BAD_STATE;
    } 
    http->now = mprGetTime(conn);
    expire = http->now + inactivityTimeout;
    remainingTime = inactivityTimeout;
    while (conn->state < state && conn->sock && !mprIsSocketEof(conn->sock) && remainingTime >= 0) {
        fd = conn->sock->fd;
        if (!trans->writeComplete) {
            events = mprWaitForSingleIO(conn, fd, MPR_WRITABLE, remainingTime);
        } else {
            if (mprSocketHasPendingData(conn->sock)) {
                events = MPR_READABLE;
            } else {
                events = mprWaitForSingleIO(conn, fd, MPR_READABLE, remainingTime);
            }
        }
        http->now = mprGetTime(conn);
        remainingTime = (int) (expire - http->now);
        if (events) {
            expire = http->now + inactivityTimeout;
            httpCallEvent(conn, events);
        }
        if (conn->state >= HTTP_STATE_PARSED) {
            if (conn->state < state) {
                return MPR_ERR_BAD_STATE;
            }
            if (conn->error) {
                return MPR_ERR_BAD_STATE;
            }
            break;
        }
    }
    if (conn->sock == 0 || conn->error) {
        return MPR_ERR_CONNECTION;
    }
    if (conn->state < state) {
        return MPR_ERR_TIMEOUT;
    }
    return 0;
}


/*  
    Set the connector as write blocked and can't proceed.
 */
void httpWriteBlocked(HttpConn *conn)
{
    mprLog(conn, 7, "Write Blocked");
    conn->canProceed = 0;
    conn->writeBlocked = 0;
}


static void addMatchEtag(HttpConn *conn, char *etag)
{
    HttpReceiver   *rec;

    rec = conn->receiver;
    if (rec->etags == 0) {
        rec->etags = mprCreateList(rec);
    }
    mprAddItem(rec->etags, etag);
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
    nextToken = mprStrnstr(mprGetBufStart(buf), delim, mprGetBufLength(buf));
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
    HttpReceiver   *rec;
    char        *tag;
    int         next;

    rec = conn->receiver;

    if (rec->etags == 0) {
        return 1;
    }
    if (requestedEtag == 0) {
        return 0;
    }

    for (next = 0; (tag = mprGetNextItem(rec->etags, &next)) != 0; ) {
        if (strcmp(tag, requestedEtag) == 0) {
            return (rec->ifMatch) ? 0 : 1;
        }
    }
    return (rec->ifMatch) ? 1 : 0;
}


/*  
    If an IF-MODIFIED-SINCE was specified, then return true if the resource has not been modified. If using
    IF-UNMODIFIED, then return true if the resource was modified.
 */
bool httpMatchModified(HttpConn *conn, MprTime time)
{
    HttpReceiver   *rec;

    rec = conn->receiver;

    if (rec->since == 0) {
        /*  If-Modified or UnModified not supplied. */
        return 1;
    }
    if (rec->ifModified) {
        /*  Return true if the file has not been modified.  */
        return !(time > rec->since);
    } else {
        /*  Return true if the file has been modified.  */
        return (time > rec->since);
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
    HttpReceiver   *rec;
    HttpTransmitter  *trans;
    HttpRange     *range, *last, *next;
    char        *tok, *ep;

    rec = conn->receiver;
    trans = conn->transmitter;

    value = mprStrdup(conn, value);
    if (value == 0) {
        return 0;
    }

    /*  
        Step over the "bytes="
     */
    tok = mprStrTok(value, "=", &value);

    for (last = 0; value && *value; ) {
        range = mprAllocObjZeroed(rec, HttpRange);
        if (range == 0) {
            return 0;
        }

        /*  A range "-7" will set the start to -1 and end to 8
         */
        tok = mprStrTok(value, ",", &value);
        if (*tok != '-') {
            range->start = (int) mprAtoi(tok, 10);
        } else {
            range->start = -1;
        }
        range->end = -1;

        if ((ep = strchr(tok, '-')) != 0) {
            if (*++ep != '\0') {
                /*
                    End is one beyond the range. Makes the math easier.
                 */
                range->end = (int) mprAtoi(ep, 10) + 1;
            }
        }
        if (range->start >= 0 && range->end >= 0) {
            range->len = (int) (range->end - range->start);
        }
        if (last == 0) {
            rec->ranges = range;
        } else {
            last->next = range;
        }
        last = range;
    }

    /*  
        Validate ranges
     */
    for (range = rec->ranges; range; range = range->next) {
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
            if (next->start >= 0 && range->end > next->start) {
                return 0;
            }
        }
    }
    trans->currentRange = rec->ranges;
    return (last) ? 1: 0;
}


static void traceBuf(HttpConn *conn, cchar *buf, int len, int mask)
{
    cchar   *cp, *tag, *digits;
    char    *data, *dp;
    int     level, i, printable;

    level = conn->traceLevel;

    for (printable = 1, i = 0; i < len; i++) {
        if (!isascii(buf[i])) {
            printable = 0;
        }
    }
    tag = (mask & HTTP_TRACE_TRANSMIT) ? "Transmit" : "Receive";
    if (printable) {
        data = mprAlloc(conn, len + 1);
        memcpy(data, buf, len);
        data[len] = '\0';
        mprRawLog(conn, level, "%s packet, conn %d, len %d >>>>>>>>>>\n%s", tag, conn->seqno, len, data);
        mprFree(data);
    } else {
        mprRawLog(conn, level, "%s packet, conn %d, len %d >>>>>>>>>> (binary)\n", tag, conn->seqno, len);
        data = mprAlloc(conn, len * 3 + ((len / 16) + 1) + 1);
        digits = "0123456789ABCDEF";
        for (i = 0, cp = buf, dp = data; cp < &buf[len]; cp++) {
            *dp++ = digits[(*cp >> 4) & 0x0f];
            *dp++ = digits[*cp++ & 0x0f];
            *dp++ = ' ';
            if ((++i % 16) == 0) {
                *dp++ = '\n';
            }
        }
        *dp++ = '\n';
        *dp = '\0';
        mprRawLog(conn, level, "%s", data);
    }
    mprRawLog(conn, level, "<<<<<<<<<< %s packet, conn %d\n\n", tag, conn->seqno);
}


void httpTraceContent(HttpConn *conn, HttpPacket *packet, int size, int offset, int mask)
{
    int     len;

    mprAssert(conn->traceMask);

    if (offset >= conn->traceMaxLength) {
        mprLog(conn, conn->traceLevel, "Abbreviating response trace for conn %d", conn->seqno);
        conn->traceMask = 0;
        return;
    }
    if (size <= 0) {
        size = INT_MAX;
    }
    if (packet->prefix) {
        len = mprGetBufLength(packet->prefix);
        len = min(len, size);
        traceBuf(conn, mprGetBufStart(packet->prefix), len, mask);
    }
    if (packet->content) {
        len = mprGetBufLength(packet->content);
        len = min(len, size);
        traceBuf(conn, mprGetBufStart(packet->content), len, mask);
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
