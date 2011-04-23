/*
    rx.c -- Http receiver. Parses http requests and client responses.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Locals ************************************/

static char *authTypes[] = { "none", "basic", "digest" };

/***************************** Forward Declarations ***************************/

static void addMatchEtag(HttpConn *conn, char *etag);
static ssize getChunkPacketSize(HttpConn *conn, MprBuf *buf);
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
static bool processParsed(HttpConn *conn);
static bool processRunning(HttpConn *conn);

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
    rx->needInputPipeline = !conn->server;
    rx->headers = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS);
    return rx;
}


static void manageRx(HttpRx *rx, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(rx->method);
        mprMark(rx->originalUri);
        mprMark(rx->uri);
        mprMark(rx->scriptName);
        mprMark(rx->pathInfo);
        mprMark(rx->extraPath);
        mprMark(rx->conn);
        mprMark(rx->etags);
        mprMark(rx->headerPacket);
        mprMark(rx->headers);
        mprMark(rx->inputPipeline);
        mprMark(rx->loc);
        mprMark(rx->parsedUri);
        mprMark(rx->requestData);
        mprMark(rx->statusMessage);
        mprMark(rx->accept);
        mprMark(rx->acceptCharset);
        mprMark(rx->acceptEncoding);
        mprMark(rx->cookie);
        mprMark(rx->connection);
        mprMark(rx->contentLength);
        mprMark(rx->hostHeader);
        mprMark(rx->pragma);
        mprMark(rx->mimeType);
        mprMark(rx->redirect);
        mprMark(rx->referrer);
        mprMark(rx->userAgent);
        mprMark(rx->formVars);
        mprMark(rx->ranges);
        mprMark(rx->inputRange);
        mprMark(rx->auth);
        mprMark(rx->authAlgorithm);
        mprMark(rx->authDetails);
        mprMark(rx->authStale);
        mprMark(rx->authType);
        mprMark(rx->files);
        mprMark(rx->uploadDir);
        mprMark(rx->alias);
        mprMark(rx->dir);

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
    Process incoming requests and drive the state machine. This will process as many requests as possible before returning. 
    All socket I/O is non-blocking, and this routine must not block. Note: packet may be null.
 */
void httpProcess(HttpConn *conn, HttpPacket *packet)
{
    mprAssert(conn);

    conn->canProceed = 1;
    conn->advancing = 1;

    while (conn->canProceed) {
        LOG(7, "httpProcess, state %d, error %d", conn->state, conn->error);

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

        case HTTP_STATE_RUNNING:
            conn->canProceed = processRunning(conn);
            break;

        case HTTP_STATE_COMPLETE:
            conn->canProceed = processCompletion(conn);
            break;
        }
        packet = conn->input;
    }
    conn->advancing = 0;
}


/*  
    Parse the incoming http message. Return true to keep going with this or subsequent request, zero means
    insufficient data to proceed.
 */
static bool parseIncoming(HttpConn *conn, HttpPacket *packet)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpLoc     *loc;
    ssize       len;
    char        *start, *end;

    if (packet == NULL) {
        return 0;
    }
    if (conn->rx == NULL) {
        conn->rx = httpCreateRx(conn);
        conn->tx = httpCreateTx(conn, NULL);
    }
    rx = conn->rx;
    tx = conn->tx;
    
    if ((len = httpGetPacketLength(packet)) == 0) {
        return 0;
    }
    start = mprGetBufStart(packet->content);
    if ((end = scontains(start, "\r\n\r\n", len)) == 0) {
        if (len >= conn->limits->headerSize) {
            httpError(conn, HTTP_ABORT | HTTP_CODE_REQUEST_TOO_LARGE, "Header too big");
        }
        return 0;
    }
    len = (int) (end - start);
    mprAddNullToBuf(packet->content);

    if (len >= conn->limits->headerSize) {
        httpError(conn, HTTP_ABORT | HTTP_CODE_REQUEST_TOO_LARGE, "Header too big");
        return 0;
    }
    if (conn->server) {
        if (!httpValidateLimits(conn->server, HTTP_VALIDATE_OPEN_REQUEST, conn)) {
            return 0;
        }
        parseRequestLine(conn, packet);
    } else {
        parseResponseLine(conn, packet);
    }
    parseHeaders(conn, packet);
    if (conn->server) {
        httpMatchHost(conn);
        if (httpSetUri(conn, rx->uri, "") < 0) {
            httpError(conn, HTTP_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad URL format");
            return 0;
        }
        if (conn->secure) {
            rx->parsedUri->scheme = sclone("https");
        }
        rx->parsedUri->port = conn->sock->listenSock->port;
        rx->parsedUri->host = rx->hostHeader ? rx->hostHeader : conn->host->name;
        if (!tx->handler) {
            httpMatchHandler(conn);  
        }
        loc = rx->loc;
        mprAssert(loc);

        mprLog(3, "Select handler: \"%s\" for \"%s\"", tx->handler->name, rx->uri);
        httpSetState(conn, HTTP_STATE_PARSED);        
        httpCreatePipeline(conn, loc, tx->handler);
        rx->startAfterContent = (loc->flags & HTTP_LOC_AFTER || ((rx->form || rx->upload) && loc->flags & HTTP_LOC_SMART));

    } else if (!(100 <= rx->status && rx->status < 200)) {
        httpSetState(conn, HTTP_STATE_PARSED);        
    }
    return 1;
}


static void traceRequest(HttpConn *conn, HttpPacket *packet)
{
    HttpRx  *rx;
    MprBuf  *content;
    cchar   *endp;
    int     len, level;

    rx = conn->rx;

    mprLog(4, "New request from %s:%d to %s:%d", conn->ip, conn->port, conn->sock->acceptIp, conn->sock->acceptPort);

    if (httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_HEADER, conn->tx->extension) >= 0) {
        content = packet->content;
        endp = strstr((char*) content->start, "\r\n\r\n");
        len = (endp) ? (int) (endp - mprGetBufStart(content) + 4) : 0;
        httpTraceContent(conn, HTTP_TRACE_RX, HTTP_TRACE_HEADER, packet, len, 0);

    } else if ((level = httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_FIRST, NULL)) >= 0) {
        content = packet->content;
        endp = strstr((char*) content->start, "\r\n");
        len = (endp) ? (int) (endp - mprGetBufStart(content) + 2) : 0;
        if (len > 0) {
            content->start[len - 2] = '\0';
            mprLog(level, "%s", content->start);
            content->start[len - 2] = '\r';
        }
    }
}


/*  
    Parse the first line of a http request. Return true if the first line parsed. This is only called once all the headers
    have been read and buffered. Requests look like: METHOD URL HTTP/1.X.
 */
static void parseRequestLine(HttpConn *conn, HttpPacket *packet)
{
    HttpRx      *rx;
    char        *method, *uri, *protocol;
    int         methodFlags;

    traceRequest(conn, packet);

    rx = conn->rx;
    uri = 0;
    methodFlags = 0;

#if BLD_DEBUG
    conn->startTime = mprGetTime();
    conn->startTicks = mprGetTicks();
#endif

    method = getToken(conn, " ");
    rx->method = method = supper(method);

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
            rx->needInputPipeline = 1;

        } else if (strcmp(method, "PUT") == 0) {
            methodFlags = HTTP_PUT;
            rx->needInputPipeline = 1;
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
        methodFlags = HTTP_UNKNOWN;
    }
    uri = getToken(conn, " ");
    if (*uri == '\0') {
        httpError(conn, HTTP_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad HTTP request. Empty URI");
    } else if ((int) strlen(uri) >= conn->limits->uriSize) {
        httpError(conn, HTTP_CODE_REQUEST_URL_TOO_LARGE, "Bad request. URI too long");
    }
    protocol = conn->protocol = supper(getToken(conn, "\r\n"));
    if (strcmp(protocol, "HTTP/1.0") == 0) {
        if (methodFlags & (HTTP_POST|HTTP_PUT)) {
            rx->remainingContent = MAXINT;
            rx->needInputPipeline = 1;
        }
        conn->http10 = 1;
        conn->protocol = protocol;
    } else if (strcmp(protocol, "HTTP/1.1") == 0) {
        conn->protocol = protocol;
    } else {
        httpError(conn, HTTP_CLOSE | HTTP_CODE_NOT_ACCEPTABLE, "Unsupported HTTP protocol");
    }
    rx->flags |= methodFlags;
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
    MprBuf      *content;
    cchar       *endp;
    char        *protocol, *status;
    int         len, level, traced;

    rx = conn->rx;
    traced = 0;

    if (httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_HEADER, conn->tx->extension) >= 0) {
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

    if (slen(rx->statusMessage) >= conn->limits->uriSize) {
        httpError(conn, HTTP_CODE_REQUEST_URL_TOO_LARGE, "Bad response. Status message too long");
    }
    if (!traced && (level = httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_FIRST, conn->tx->extension)) >= 0) {
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
    char        *key, *value, *tok;
    cchar       *oldValue;
    int         len, count, keepAlive;

    rx = conn->rx;
    tx = conn->tx;
    content = packet->content;
    conn->rx->headerPacket = packet;
    limits = conn->limits;
    keepAlive = (conn->http10) ? 0 : 1;

    for (count = 0; content->start[0] != '\r' && !conn->connError; count++) {
        if (count >= limits->headerCount) {
            httpError(conn, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Too many headers");
            break;
        }
        if ((key = getToken(conn, ":")) == 0 || *key == '\0') {
            httpError(conn, HTTP_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad header format");
            break;
        }
        value = getToken(conn, "\r\n");
        while (isspace((int) *value)) {
            value++;
        }
        key = slower(key);

        LOG(8, "Key %s, value %s", key, value);
        if (strspn(key, "%<>/\\") > 0) {
            httpError(conn, HTTP_CODE_BAD_REQUEST, "Bad header key value");
        }
        if ((oldValue = mprLookupHash(rx->headers, key)) != 0) {
            mprAddKey(rx->headers, key, mprAsprintf("%s, %s", oldValue, value));
        } else {
            mprAddKey(rx->headers, key, sclone(value));
        }
        switch (key[0]) {
        case 'a':
            if (strcmp(key, "authorization") == 0) {
                value = sclone(value);
                rx->authType = stok(value, " \t", &tok);
                rx->authDetails = sclone(tok);

            } else if (strcmp(key, "accept-charset") == 0) {
                rx->acceptCharset = sclone(value);

            } else if (strcmp(key, "accept") == 0) {
                rx->accept = sclone(value);

            } else if (strcmp(key, "accept-encoding") == 0) {
                rx->acceptEncoding = sclone(value);
            }
            break;

        case 'c':
            if (strcmp(key, "content-length") == 0) {
                if (rx->length >= 0) {
                    httpError(conn, HTTP_CLOSE | HTTP_CODE_BAD_REQUEST, "Mulitple content length headers");
                    break;
                }
                rx->length = stoi(value, 10, 0);
                if (rx->length < 0) {
                    httpError(conn, HTTP_CLOSE | HTTP_CODE_BAD_REQUEST, "Bad content length");
                    break;
                }
                if (rx->length >= conn->limits->receiveBodySize) {
                    httpError(conn, HTTP_ABORT | HTTP_CODE_REQUEST_TOO_LARGE,
                        "Request content length %Ld bytes is too big. Limit %Ld", rx->length, conn->limits->receiveBodySize);
                    break;
                }
                rx->contentLength = sclone(value);
                mprAssert(rx->length >= 0);
                if (conn->server || strcmp(tx->method, "HEAD") != 0) {
                    rx->remainingContent = rx->length;
                    rx->needInputPipeline = 1;
                }

            } else if (strcmp(key, "content-range") == 0) {
                /*
                    This headers specifies the range of any posted body data
                    Format is:  Content-Range: bytes n1-n2/length
                    Where n1 is first byte pos and n2 is last byte pos
                 */
                char    *sp;
                MprOff  start, end, size;

                start = end = size = -1;
                sp = value;
                while (*sp && !isdigit((int) *sp)) {
                    sp++;
                }
                if (*sp) {
                    start = stoi(sp, 10, NULL);
                    if ((sp = strchr(sp, '-')) != 0) {
                        end = stoi(++sp, 10, NULL);
                    }
                    if ((sp = strchr(sp, '/')) != 0) {
                        /*
                            Note this is not the content length transmitted, but the original size of the input of which
                            the client is transmitting only a portion.
                         */
                        size = stoi(++sp, 10, NULL);
                    }
                }
                if (start < 0 || end < 0 || size < 0 || end <= start) {
                    httpError(conn, HTTP_CODE_RANGE_NOT_SATISFIABLE, "Bad content range");
                    break;
                }
                rx->inputRange = httpCreateRange(conn, start, end);

            } else if (strcmp(key, "content-type") == 0) {
                rx->mimeType = sclone(value);
                rx->form = scontains(rx->mimeType, "application/x-www-form-urlencoded", -1) != 0;

            } else if (strcmp(key, "cookie") == 0) {
                if (rx->cookie && *rx->cookie) {
                    rx->cookie = sjoin(rx->cookie, "; ", value, NULL);
                } else {
                    rx->cookie = sclone(value);
                }

            } else if (strcmp(key, "connection") == 0) {
                rx->connection = sclone(value);
                if (scasecmp(value, "KEEP-ALIVE") == 0) {
                    keepAlive = 1;
                } else if (scasecmp(value, "CLOSE") == 0) {
                    /*  Not really required, but set to 0 to be sure */
                    conn->keepAliveCount = 0;
                }
            }
            break;

        case 'h':
            if (strcmp(key, "host") == 0) {
                rx->hostHeader = sclone(value);
            }
            break;

        case 'i':
            if ((strcmp(key, "if-modified-since") == 0) || (strcmp(key, "if-unmodified-since") == 0)) {
                MprTime     newDate = 0;
                char        *cp;
                bool        ifModified = (key[3] == 'm');

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

            } else if ((strcmp(key, "if-match") == 0) || (strcmp(key, "if-none-match") == 0)) {
                char    *word, *tok;
                bool    ifMatch = key[3] == 'm';

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

            } else if (strcmp(key, "if-range") == 0) {
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
            if (strcmp(key, "keep-alive") == 0) {
                len = (int) strlen(value);
                if (len > 2 && value[len - 1] == '1' && value[len - 2] == '=' && tolower((int)(value[len - 3])) == 'x') {
                    /*  
                        On second-last request (Keep-Alive: timeout=N, max=1)
                        IMPORTANT: Deliberately close the connection one request early. This ensures a client-led 
                        termination and helps relieve server-side TIME_WAIT conditions.
                     */
                    conn->keepAliveCount = 0;
                } else {
                    keepAlive = 1;
                }
            }
            break;                
                
        case 'l':
            if (strcmp(key, "location") == 0) {
                rx->redirect = sclone(value);
            }
            break;

        case 'p':
            if (strcmp(key, "pragma") == 0) {
                rx->pragma = sclone(value);
            }
            break;

        case 'r':
            if (strcmp(key, "range") == 0) {
                if (!parseRange(conn, value)) {
                    httpError(conn, HTTP_CODE_RANGE_NOT_SATISFIABLE, "Bad range");
                }
            } else if (strcmp(key, "referer") == 0) {
                /* NOTE: yes the header is misspelt in the spec */
                rx->referrer = sclone(value);
            }
            break;

        case 't':
            if (strcmp(key, "transfer-encoding") == 0) {
                if (scasecmp(value, "chunked") == 0) {
                    rx->flags |= HTTP_CHUNKED;
                    /*  
                        This will be revised by the chunk filter as chunks are processed and will be set to zero when the
                        last chunk has been received.
                     */
                    rx->remainingContent = MAXINT;
                    rx->needInputPipeline = 1;
                }
            }
            break;

#if BLD_DEBUG
        case 'x':
            if (strcmp(key, "x-chunk-size") == 0) {
                tx->chunkSize = atoi(value);
                if (tx->chunkSize <= 0) {
                    tx->chunkSize = 0;
                } else if (tx->chunkSize > conn->limits->chunkSize) {
                    tx->chunkSize = conn->limits->chunkSize;
                }
            }
            break;
#endif

        case 'u':
            if (strcmp(key, "user-agent") == 0) {
                rx->userAgent = sclone(value);
            }
            break;

        case 'w':
            if (strcmp(key, "www-authenticate") == 0) {
                conn->authType = value;
                while (*value && !isspace((int) *value)) {
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
    if (!keepAlive) {
        conn->keepAliveCount = 0;
    }
    if (!(rx->flags & HTTP_CHUNKED)) {
        /*  
            Step over "\r\n" after headers. As an optimization, don't do this if chunked so chunking can parse a single
            chunk delimiter of "\r\nSIZE ...\r\n"
         */
        if (httpGetPacketLength(packet) >= 2) {
            mprAdjustBufStart(content, 2);
        }
    }
    if (rx->remainingContent == 0) {
        rx->eof = 1;
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
    if (!conn->rx->startAfterContent) {
        httpStartPipeline(conn);
    }
    httpSetState(conn, HTTP_STATE_CONTENT);
    if (conn->workerEvent && !conn->rx->startAfterContent) {
        if (conn->connError || conn->rx->remainingContent <= 0) {
            httpSetState(conn, HTTP_STATE_RUNNING);
        }
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
    MprOff      remaining;
    ssize       nbytes;

    rx = conn->rx;
    tx = conn->tx;
    q = tx->queue[HTTP_QUEUE_RECEIVE];

    content = packet->content;
    if (rx->flags & HTTP_CHUNKED) {
        if ((remaining = getChunkPacketSize(conn, content)) == 0) {
            /* Need more data or bad chunk specification */
            if (mprGetBufLength(content) > 0) {
                conn->input = packet;
            }
            return 0;
        }
    } else {
        remaining = rx->remainingContent;
    }
    nbytes = (ssize) min(remaining, mprGetBufLength(content));
    mprAssert(nbytes >= 0);

    if (nbytes > 0) {
        if (httpShouldTrace(conn, HTTP_TRACE_RX, HTTP_TRACE_BODY, NULL) >= 0) {
            httpTraceContent(conn, HTTP_TRACE_RX, HTTP_TRACE_BODY, packet, nbytes, 0);
        }
    }
    LOG(7, "processContent: packet of %d bytes, remaining %d", mprGetBufLength(content), remaining);

    if (nbytes > 0) {
        mprAssert(httpGetPacketLength(packet) > 0);
        remaining -= nbytes;
        rx->remainingContent -= nbytes;
        rx->bytesRead += nbytes;

        if (rx->bytesRead >= conn->limits->receiveBodySize) {
            httpError(conn, HTTP_CLOSE | HTTP_CODE_REQUEST_TOO_LARGE, 
                "Request body of %Ld bytes is too big. Limit %Ld", rx->bytesRead, conn->limits->receiveBodySize);
            return 1;
        }
        if (packet == rx->headerPacket) {
            /* Preserve headers if more data to come. Otherwise handlers may free the packet and destory the headers */
            packet = httpSplitPacket(packet, 0);
        }
        conn->input = 0;
        if (remaining == 0 && httpGetPacketLength(packet) > nbytes) {
            /*  Split excess data belonging to the next pipelined request.  */
            LOG(7, "processContent: Split packet of %d at %d", httpGetPacketLength(packet), nbytes);
            conn->input = httpSplitPacket(packet, nbytes);
        }
        httpSendPacketToNext(q, packet);
    } else {
        conn->input = 0;
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
    q = conn->tx->queue[HTTP_QUEUE_RECEIVE];

    if (conn->connError || rx->remainingContent <= 0) {
        httpSetState(conn, HTTP_STATE_RUNNING);
        return 1;
    }
    if (packet == NULL) {
        return 0;
    }
    //  MOB - is this the best place? - move
    mprYield(0);

    if (!analyseContent(conn, packet)) {
        return 0;
    }
    if (conn->connError || 
            (rx->remainingContent == 0 && (!(rx->flags & HTTP_CHUNKED) || (rx->chunkState == HTTP_CHUNK_EOF)))) {
        rx->eof = 1;
        httpSendPacketToNext(q, httpCreateEndPacket());
        if (rx->startAfterContent) {
            httpStartPipeline(conn);
        }
        httpSetState(conn, HTTP_STATE_RUNNING);
        return conn->workerEvent ? 0 : 1;
    }
    httpServiceQueues(conn);
#if UNUSED
    if ((conn->readq->count + httpGetPacketLength(packet)) > conn->readq->max) {
        return 0;
    }
#endif
    return conn->error || (conn->input ? httpGetPacketLength(conn->input) : 0);
}


/*
    In the running state after all content has been received
    Note: may be called multiple times
 */
static bool processRunning(HttpConn *conn)
{
    int     canProceed;

    canProceed = 1;
    if (conn->connError) {
        httpSetState(conn, HTTP_STATE_COMPLETE);
    } else {
        if (conn->server) {
            httpProcessPipeline(conn);
            if (conn->connError || conn->writeComplete) {
                httpSetState(conn, HTTP_STATE_COMPLETE);
            } else {
                httpWritable(conn);
                canProceed = httpServiceQueues(conn);
            }
        } else {
            httpServiceQueues(conn);
            httpFinalize(conn);
            httpSetState(conn, HTTP_STATE_COMPLETE);
        }
    }
    return canProceed;
}


#if BLD_DEBUG
static void measure(HttpConn *conn)
{
    MprTime     elapsed;
    cchar       *uri;

    if (conn->rx == 0 || conn->tx == 0) {
        return;
    }
    uri = (conn->server) ? conn->rx->uri : conn->tx->parsedUri->path;
   
    if (httpShouldTrace(conn, 0, HTTP_TRACE_TIME, NULL) >= 0) {
        elapsed = mprGetTime() - conn->startTime;
#if MPR_HIGH_RES_TIMER
        if (elapsed < 1000) {
            mprLog(6, "TIME: Request %s took %,d msec %,d ticks", uri, elapsed, mprGetTicks() - conn->startTicks);
        } else
#endif
            mprLog(6, "TIME: Request %s took %,d msec", uri, elapsed);
    }
}
#else
#define measure(conn)
#endif


static bool processCompletion(HttpConn *conn)
{
    HttpPacket  *packet;
    bool        more;

    mprAssert(conn->state == HTTP_STATE_COMPLETE);

    httpDestroyPipeline(conn);
    measure(conn);
    if (conn->server && conn->rx) {
        conn->rx->conn = 0;
        conn->tx->conn = 0;
        conn->rx = 0;
        conn->tx = 0;
        packet = conn->input;
        more = packet && !conn->connError && (httpGetPacketLength(packet) > 0);
        httpValidateLimits(conn->server, HTTP_VALIDATE_CLOSE_REQUEST, conn);
        httpPrepServerConn(conn);
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
    if (conn->state < HTTP_STATE_COMPLETE && !conn->advancing) {
        httpProcess(conn, NULL);
    }
}


/*  
    Optimization to correctly size the packets to the chunk filter.
 */
static ssize getChunkPacketSize(HttpConn *conn, MprBuf *buf)
{
    HttpRx      *rx;
    char        *start, *cp;
    ssize       size, need;

    rx = conn->rx;
    need = 0;

    switch (rx->chunkState) {
    case HTTP_CHUNK_DATA:
        need = (ssize) min(MAXSSIZE, rx->remainingContent);
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
            httpError(conn, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
            return 0;
        }
        for (cp = &start[2]; cp < (char*) buf->end && *cp != '\n'; cp++) {}
        if ((cp - start) < 2 || (cp[-1] != '\r' || cp[0] != '\n')) {
            /* Insufficient data */
            if ((cp - start) > 80) {
                httpError(conn, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
                return 0;
            }
            return 0;
        }
        need = (cp - start + 1);
        size = (ssize) stoi(&start[2], 16, NULL);
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
    rx->remainingContent = need;
    return need;
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
            performed, skip the transfer. TODO - need to check if fileInfo is actually set.
         */
        modified = (MprTime) tx->fileInfo.mtime * MPR_TICKS_PER_SEC;
        same = httpMatchModified(conn, modified) && httpMatchEtag(conn, tx->etag);
        if (rx->ranges && !same) {
            rx->ranges = 0;
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
    return mprLookupHash(conn->rx->headers, slower(key));
}


//  MOB -- why does this allocate?
char *httpGetHeaders(HttpConn *conn)
{
    HttpRx      *rx;
    MprHash     *hp;
    char        *headers, *key, *cp;
    int         len;

    if (conn->rx == 0) {
        mprAssert(conn->rx);
        return 0;
    }
    rx = conn->rx;
    headers = 0;
    for (len = 0, hp = mprGetFirstHash(rx->headers); hp; ) {
        headers = srejoin(headers, hp->key, NULL);
        key = &headers[len];
        for (cp = &key[1]; *cp; cp++) {
            *cp = tolower((int) *cp);
            if (*cp == '-') {
                cp++;
            }
        }
        headers = srejoin(headers, ": ", hp->data, "\n", NULL);
        len = (int) strlen(headers);
        hp = mprGetNextHash(rx->headers, hp);
    }
    return headers;
}


MprHashTable *httpGetHeaderHash(HttpConn *conn)
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


int httpMapToStorage(HttpConn *conn)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpHost    *host;
    MprPath     *info, ginfo;
    char        *gfile;

    rx = conn->rx;
    tx = conn->tx;
    host = conn->host;
    info = &tx->fileInfo;

    rx->alias = httpGetAlias(host, rx->pathInfo);
    tx->filename = httpMakeFilename(conn, rx->alias, rx->pathInfo, 1);

    tx->extension = httpGetExtension(conn);
#if BLD_WIN_LIKE
    if (tx->extension) {
        tx->extension = slower(tx->extension);
    }
#endif
    if ((rx->dir = httpLookupBestDir(host, tx->filename)) == 0) {
        httpError(conn, HTTP_CODE_NOT_FOUND, "Missing directory block for \"%s\"", tx->filename);
        return MPR_ERR_CANT_ACCESS;
    }
    if (rx->dir->auth) {
        rx->auth = rx->dir->auth;
    }
    if ((rx->loc = httpLookupBestLocation(host, rx->pathInfo)) == 0) {
        httpError(conn, HTTP_CODE_NOT_FOUND, "Missing location block for \"%s\"", rx->pathInfo);
        return MPR_ERR_CANT_ACCESS;
    }
    if (rx->auth == 0) {
        rx->auth = rx->loc->auth;
    }
    mprGetPathInfo(tx->filename, info);
    if (!info->valid && rx->acceptEncoding && strstr(rx->acceptEncoding, "gzip") != 0) {
        gfile = mprAsprintf("%s.gz", tx->filename);
        if (mprGetPathInfo(gfile, &ginfo) == 0) {
            tx->filename = gfile;
            tx->fileInfo = ginfo;
            httpSetHeader(conn, "Content-Encoding", "gzip");
        }
    }
    if (info->valid) {
        tx->etag = mprAsprintf("\"%x-%Lx-%Lx\"", info->inode, info->size, info->mtime);
    }
    mprLog(5, "Request Details: uri \"%s\"", rx->uri);
    mprLog(5, "Filename: \"%s\", extension: \"%s\"", tx->filename, tx->extension);
    mprLog(5, "Location: \"%s\", alias: \"%s\" => \"%s\"", rx->loc->prefix, rx->alias->prefix, rx->alias->filename);
    mprLog(5, "Auth: \"%s\"", authTypes[rx->auth->type]);
    return 0;
}


int httpSetUri(HttpConn *conn, cchar *uri, cchar *query)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpHost    *host;
    HttpUri     *prior;

    rx = conn->rx;
    tx = conn->tx;
    host = conn->host;
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
    httpMapToStorage(conn);
    return 0;
}


static void waitHandler(HttpConn *conn, struct MprEvent *event)
{
    httpCallEvent(conn, event->mask);
    mprSignalDispatcher(conn->dispatcher);
}


/*
    Wait for an Http event until the http reaches a specified state or a timeout expires
    If timeout is == -1, then no timeout is used. If state is zero, it waits for just one event.
 */
int httpWait(HttpConn *conn, int state, MprTime timeout)
{
    Http        *http;
    MprTime     expire, remainingTime;
    int         eventMask, addedHandler, saveAsync, justOne, workDone;

    http = conn->http;
    if (timeout <= 0) {
        timeout = MAXINT;
    }
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
        httpProcess(conn, conn->input);
    }
    http->now = mprGetTime();
    expire = http->now + timeout;
    remainingTime = expire - http->now;
    saveAsync = conn->async;
    addedHandler = 0;

    while (!conn->error && conn->state < state) {
        if (conn->waitHandler == 0) {
            conn->async = 1;
            eventMask = MPR_READABLE;
            if (!conn->writeComplete) {
                eventMask |= MPR_WRITABLE;
            }
            conn->waitHandler = mprCreateWaitHandler(conn->sock->fd, eventMask, conn->dispatcher, waitHandler, conn, 0);
            addedHandler = 1;
        }
        workDone = httpServiceQueues(conn);
        remainingTime = expire - http->now;
        if (remainingTime <= 0) {
            break;
        }
        mprAssert(!mprSocketHasPendingData(conn->sock));
        mprWaitForEvent(conn->dispatcher, remainingTime);
        if (justOne || (conn->sock && mprIsSocketEof(conn->sock) && !workDone)) {
            break;
        }
    }
    if (addedHandler && conn->waitHandler) {
        mprRemoveWaitHandler(conn->waitHandler);
        conn->waitHandler = 0;
        conn->async = saveAsync;
    }
    if (conn->sock == 0 || conn->error) {
        return MPR_ERR_CANT_CONNECT;
    }
    if (!justOne && conn->state < state) {
        return (remainingTime <= 0) ? MPR_ERR_TIMEOUT : MPR_ERR_CANT_READ;
    }
    return 0;
}


/*  
    Set the connector as write blocked and can't proceed.
 */
void httpSetWriteBlocked(HttpConn *conn)
{
//  MOB
    mprLog(3, "Write Blocked");
    conn->canProceed = 0;
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
    HttpRx      *rx;
    HttpRange   *range, *last, *next;
    char        *tok, *ep;

    rx = conn->rx;

    value = sclone(value);
    if (value == 0) {
        return 0;
    }

    /*  
        Step over the "bytes="
     */
    stok(value, "=", &value);

    for (last = 0; value && *value; ) {
        if ((range = mprAllocObj(HttpRange, NULL)) == 0) {
            return 0;
        }
        /*  
            A range "-7" will set the start to -1 and end to 8
         */
        tok = stok(value, ",", &value);
        if (*tok != '-') {
            range->start = (ssize) stoi(tok, 10, NULL);
        } else {
            range->start = -1;
        }
        range->end = -1;

        if ((ep = strchr(tok, '-')) != 0) {
            if (*++ep != '\0') {
                /*
                    End is one beyond the range. Makes the math easier.
                 */
                range->end = (ssize) stoi(ep, 10, NULL) + 1;
            }
        }
        if (range->start >= 0 && range->end >= 0) {
            range->len = (int) (range->end - range->start);
        }
        if (last == 0) {
            rx->ranges = range;
        } else {
            last->next = range;
        }
        last = range;
    }

    /*  
        Validate ranges
     */
    for (range = rx->ranges; range; range = range->next) {
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
    conn->tx->currentRange = rx->ranges;
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
    return mprLookupHash(rx->requestData, key);
}


char *httpMakeFilename(HttpConn *conn, HttpAlias *alias, cchar *url, bool skipAliasPrefix)
{
    cchar   *seps;
    char    *path;
    int     len;

    mprAssert(alias);
    mprAssert(url);

    if (skipAliasPrefix) {
        url += alias->prefixLen;
    }
    while (*url == '/') {
        url++;
    }
    len = (int) strlen(alias->filename);
    if ((path = mprAlloc(len + strlen(url) + 2)) == 0) {
        return 0;
    }
    strcpy(path, alias->filename);
    if (*url) {
        seps = mprGetPathSeparators(path);
        path[len++] = seps[0];
        strcpy(&path[len], url);
    }
    return mprGetNativePath(path);
}


/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2011. All Rights Reserved.

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

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
