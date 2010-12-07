/*
    httpCmd.c -- Http client program

    The http program is a client to issue HTTP requests. It is also a test platform for loading and testing web servers. 

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */
 
/******************************** Includes ***********************************/

#include    "http.h"

/*********************************** Locals ***********************************/

static int      activeLoadThreads;  /* Still running test threads */
static int      benchmark;          /* Output benchmarks */
static int      chunkSize;          /* Ask for response data to be chunked in this quanta */
static int      continueOnErrors;   /* Continue testing even if an error occurs. Default is to stop */
static int      success;            /* Total success flag */
static int      fetchCount;         /* Total count of fetches */
static MprList  *files;             /* Upload files */
static MprList  *formData;          /* Form body data */
static MprBuf   *bodyData;          /* Block body data */
static Mpr      *mpr;               /* Portable runtime */
static MprList  *headers;           /* Request headers */
static Http     *http;              /* Http service object */
static int      iterations;         /* URLs to fetch */
static int      isBinary;           /* Looks like a binary output file */
static char     *host;              /* Host to connect to */
static int      loadThreads;        /* Number of threads to use for URL requests */
static char     *method;            /* HTTP method when URL on cmd line */
static int      nextArg;            /* Next arg to parse */
static int      noout;              /* Don't output files */
static int      nofollow;           /* Don't automatically follow redirects */
static char     *password;          /* Password for authentication */
static int      printable;          /* Make binary output printable */
static char     *protocol;          /* HTTP/1.0, HTTP/1.1 */
static char     *ranges;            /* Request ranges */
static int      retries;            /* Times to retry a failed request */
static int      sequence;           /* Sequence requests with a custom header */
static int      showStatus;         /* Output the Http response status */
static int      showHeaders;        /* Output the response headers */
static int      singleStep;         /* Pause between requests */
static char     *target;            /* Destination url */
static int      timeout;            /* Timeout in secs for a non-responsive server */
static int      upload;             /* Upload using multipart mime */
static char     *username;          /* User name for authentication of requests */
static int      verbose;            /* Trace progress */
static int      workers;            /* Worker threads. >0 if multi-threaded */
static MprMutex *mutex;

/***************************** Forward Declarations ***************************/

static void     addFormVars(cchar *buf);
static void     processing();
static int      doRequest(HttpConn *conn, cchar *url);
static void     finishThread(MprThread *tp);
static char     *getPassword();
static void     initSettings(Mpr *mpr);
static bool     isPort(cchar *name);
static bool     iterationsComplete();
static bool     parseArgs(Mpr *mpr, int argc, char **argv);
static int      processThread(HttpConn *conn, MprEvent *event);
static void     threadMain(void *data, MprThread *tp);
static char     *resolveUrl(HttpConn *conn, cchar *url);
static int      setContentLength(HttpConn *conn);
static void     showOutput(HttpConn *conn, cchar *content, int contentLen);
static void     showUsage(Mpr *mpr);
static int      startLogging(Mpr *mpr, char *logSpec);
static void     trace(HttpConn *conn, cchar *url, int fetchCount, cchar *method, int status, int contentLen);
static void     waitForUser();
static int      writeBody(HttpConn *conn);

/*********************************** Code *************************************/

MAIN(httpMain, int argc, char *argv[])
{
    MprTime         start;
    double          elapsed;

    mpr = mprCreate(argc, argv, NULL);

    /*  
        Explicit initialization of globals for re-entrancy on Vxworks
     */
    activeLoadThreads = benchmark = continueOnErrors = fetchCount = iterations = isBinary = 0;
    success = loadThreads = nextArg = noout = nofollow = showHeaders = printable = workers = 0;
    chunkSize = retries = singleStep = timeout = verbose = 0;

    host = method = password = ranges = 0;
    username = 0;
    headers = 0;
    formData = 0;
    bodyData = 0;

    initSettings(mpr);
    if (!parseArgs(mpr, argc, argv)) {
        showUsage(mpr);
        return MPR_ERR_BAD_ARGS;
    }
    mprSetMaxWorkers(workers);

#if BLD_FEATURE_SSL
    if (!mprLoadSsl(1)) {
        mprError("Can't load SSL");
        exit(1);
    }
#endif

    /*  
        Start the Timer, Socket and Worker services
     */
    if (mprStart(mpr) < 0) {
        mprError("Can't start MPR for %s", mprGetAppTitle(mpr));
        exit(2);
    }
    start = mprGetTime(mpr);
    http = httpCreate(mpr);
    processing();

    /*  
        Wait for all the threads to complete (simple but effective). Keep servicing events as we wind down.
     */
    while (activeLoadThreads > 0) {
        mprServiceEvents(NULL, 10, 0);
    }
    if (benchmark) {
        elapsed = (double) (mprGetTime(mpr) - start);
        if (fetchCount == 0) {
            elapsed = 0;
            fetchCount = 1;
        }
        mprPrintf("\nRequest Count:       %13d\n", fetchCount);
        mprPrintf("Time elapsed:        %13.4f sec\n", elapsed / 1000.0);
        mprPrintf("Time per request:    %13.4f sec\n", elapsed / 1000.0 / fetchCount);
        mprPrintf("Requests per second: %13.4f\n", fetchCount * 1.0 / (elapsed / 1000.0));
        mprPrintf("Load threads:        %13d\n", loadThreads);
        mprPrintf("Worker threads:      %13d\n", workers);
    }
    if (!success && verbose) {
        mprError("Request failed");
    }
    return (success) ? 0 : 255;
}


static void initSettings(Mpr *mpr)
{
    method = 0;
    verbose = continueOnErrors = showHeaders = 0;

    host = "localhost";
    iterations = 1;
    loadThreads = 1;
    protocol = "HTTP/1.1";
    retries = HTTP_RETRIES;
    success = 1;
    timeout = 60;
    workers = 1;            
    headers = mprCreateList(mpr);
    mutex = mprCreateLock(mpr);
#if WIN
    _setmode(fileno(stdout), O_BINARY);
#endif
}


static bool parseArgs(Mpr *mpr, int argc, char **argv)
{
    char    *argp, *key, *value;
    int     i, setWorkers, httpVersion;

    setWorkers = 0;

    for (nextArg = 1; nextArg < argc; nextArg++) {
        argp = argv[nextArg];
        if (*argp != '-') {
            break;
        }

        if (strcmp(argp, "--benchmark") == 0 || strcmp(argp, "-b") == 0) {
            benchmark++;

        } else if (strcmp(argp, "--chunk") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                value = argv[++nextArg];
                chunkSize = atoi(value);
                if (chunkSize < 0) {
                    mprError("Bad chunksize %d", chunkSize);
                    return 0;
                }
            }

        } else if (strcmp(argp, "--continue") == 0) {
            continueOnErrors++;

        } else if (strcmp(argp, "--cookie") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                mprAddItem(headers, mprCreateKeyPair("Cookie", argv[++nextArg]));
            }

        } else if (strcmp(argp, "--data") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                if (bodyData == 0) {
                    bodyData = mprCreateBuf(-1, -1);
                }
                mprPutStringToBuf(bodyData, argv[++nextArg]);
            }

        } else if (strcmp(argp, "--debugger") == 0 || strcmp(argp, "-D") == 0) {
            mprSetDebugMode(1);
            retries = 0;
            timeout = INT_MAX / MPR_TICKS_PER_SEC;

        } else if (strcmp(argp, "--delete") == 0) {
            method = "DELETE";

        } else if (strcmp(argp, "--form") == 0 || strcmp(argp, "-f") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                if (formData == 0) {
                    formData = mprCreateList(mpr);
                }
                addFormVars(argv[++nextArg]);
            }

        } else if (strcmp(argp, "--header") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                key = argv[++nextArg];
                if ((value = strchr(key, ':')) == 0) {
                    mprError("Bad header format. Must be \"key: value\"");
                    return 0;
                }
                *value++ = '\0';
                while (isspace((int) *value)) {
                    value++;
                }
                mprAddItem(headers, mprCreateKeyPair(key, value));
            }

        } else if (strcmp(argp, "--host") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                host = argv[++nextArg];
            }

        } else if (strcmp(argp, "--http") == 0) {
            //  DEPRECATED
            if (nextArg >= argc) {
                return 0;
            } else {
                httpVersion = atoi(argv[++nextArg]);
                protocol = (httpVersion == 0) ? "HTTP/1.0" : "HTTP/1.1";
            }

        } else if (strcmp(argp, "--iterations") == 0 || strcmp(argp, "-i") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                iterations = atoi(argv[++nextArg]);
            }

        } else if (strcmp(argp, "--log") == 0 || strcmp(argp, "-l") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                startLogging(mpr, argv[++nextArg]);
            }

        } else if (strcmp(argp, "--method") == 0 || strcmp(argp, "-m") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                method = argv[++nextArg];
            }

        } else if (strcmp(argp, "--noout") == 0 || strcmp(argp, "-n") == 0  ||
                   strcmp(argp, "--quiet") == 0 || strcmp(argp, "-q") == 0) {
            noout++;

        } else if (strcmp(argp, "--nofollow") == 0) {
            nofollow++;

        } else if (strcmp(argp, "--password") == 0 || strcmp(argp, "-p") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                password = argv[++nextArg];
            }

        } else if (strcmp(argp, "--post") == 0) {
            method = "POST";

        } else if (strcmp(argp, "--printable") == 0) {
            printable++;

        } else if (strcmp(argp, "--protocol") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                protocol = sclone(argv[++nextArg]);
                supper(protocol);
            }

        } else if (strcmp(argp, "--put") == 0) {
            method = "PUT";

        } else if (strcmp(argp, "--range") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                //  TODO - should allow multiple ranges
                if (ranges == 0) {
                    ranges = mprAsprintf("bytes=%s", argv[++nextArg]);
                } else {
                    ranges = sjoin(ranges, ",", argv[++nextArg], NULL);
                }
            }
            
        } else if (strcmp(argp, "--retries") == 0 || strcmp(argp, "-r") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                retries = atoi(argv[++nextArg]);
            }
            
        } else if (strcmp(argp, "--sequence") == 0) {
            sequence++;

        } else if (strcmp(argp, "--showHeaders") == 0 || strcmp(argp, "--show") == 0) {
            showHeaders++;

        } else if (strcmp(argp, "--showStatus") == 0 || strcmp(argp, "--showCode") == 0) {
            showStatus++;

        } else if (strcmp(argp, "--single") == 0 || strcmp(argp, "-s") == 0) {
            singleStep++;

        } else if (strcmp(argp, "--threads") == 0 || strcmp(argp, "-t") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                loadThreads = atoi(argv[++nextArg]);
            }

        } else if (strcmp(argp, "--timeout") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                timeout = atoi(argv[++nextArg]);
            }

        } else if (strcmp(argp, "--upload") == 0 || strcmp(argp, "-u") == 0) {
            upload++;

        } else if (strcmp(argp, "--user") == 0 || strcmp(argp, "--username") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                username = argv[++nextArg];
            }

        } else if (strcmp(argp, "--verbose") == 0 || strcmp(argp, "-v") == 0) {
            verbose++;

        } else if (strcmp(argp, "--version") == 0 || strcmp(argp, "-V") == 0) {
            mprPrintfError("%s %s\n"
                "Copyright (C) Embedthis Software 2003-2010\n"
                "Copyright (C) Michael O'Brien 2003-2010\n",
               BLD_NAME, BLD_VERSION);
            exit(0);

        } else if (strcmp(argp, "--workerTheads") == 0 || strcmp(argp, "-w") == 0) {
            if (nextArg >= argc) {
                return 0;
            } else {
                workers = atoi(argv[++nextArg]);
            }
            setWorkers++;

        } else if (strcmp(argp, "--") == 0) {
            nextArg++;
            break;

        } else if (strcmp(argp, "-") == 0) {
            break;

        } else {
            return 0;
            break;
        }
    }
    if (argc == nextArg) {
        return 0;
    }
    argc = argc - nextArg;
    argv = &argv[nextArg];
    target = argv[argc - 1];
    argc--;
    if (argc > 0) {
        /*
            Files present on command line
         */
        files = mprCreateList(mpr);
        for (i = 0; i < argc; i++) {
            mprAddItem(files, argv[i]);
        }
    }
    if (!setWorkers) {
        workers = loadThreads + 2;
    }
    if (method == 0) {
        if (bodyData || formData || files) {
            method = "POST";
        } else if (files) {
            method = "PUT";
        } else {
            method = "GET";
        }
    }
    return 1;
}


static void showUsage(Mpr *mpr)
{
    mprPrintfError("usage: %s [options] [files] url\n"
        "  Options:\n"
        "  --benchmark           # Compute benchmark results.\n"
        "  --chunk size          # Request response data to use this chunk size.\n"
        "  --continue            # Continue on errors.\n"
        "  --cookie CookieString # Define a cookie header. Multiple uses okay.\n"
        "  --data                # Body data to send with PUT or POST.\n"
        "  --debugger            # Disable timeouts to make running in a debugger easier.\n"
        "  --delete              # Use the DELETE method. Shortcut for --method DELETE..\n"
        "  --form string         # Form data. Must already be form-www-urlencoded.\n"
        "  --header 'key: value' # Add a custom request header.\n"
        "  --host hostName       # Host name or IP address for unqualified URLs.\n"
        "  --iterations count    # Number of times to fetch the urls (default 1).\n"
        "  --log logFile:level   # Log to the file at the verbosity level.\n"
        "  --method KIND         # HTTP request method GET|OPTIONS|POST|PUT|TRACE (default GET).\n"
        "  --nofollow            # Don't automatically follow redirects.\n"
        "  --noout               # Don't output files to stdout.\n"
        "  --password pass       # Password for authentication.\n"
        "  --post                # Use POST method. Shortcut for --method POST.\n"
        "  --printable           # Make binary output printable.\n"
        "  --protocol PROTO      # Set HTTP protocol to HTTP/1.0 or HTTP/1.1 .\n"
        "  --put                 # Use PUT method. Shortcut for --method PUT.\n"
        "  --range byteRanges    # Request a subset range of the document.\n"
        "  --retries count       # Number of times to retry failing requests.\n"
        "  --sequence            # Sequence requests with a custom header.\n"
        "  --showHeaders         # Output response headers.\n"
        "  --showStatus          # Output the Http response status code.\n"
        "  --single              # Single step. Pause for input between requests.\n"
        "  --timeout secs        # Request timeout period in seconds.\n"
        "  --threads count       # Number of thread instances to spawn.\n"
        "  --upload              # Use multipart mime upload.\n"
        "  --user name           # User name for authentication.\n"
        "  --verbose             # Verbose operation. Trace progress.\n"
        "  --workers count       # Set maximum worker threads.\n",
        mprGetAppName(mpr));
}


/*
    Process the requests
 */
static void processing()
{
    MprThread   *tp;
    int         j;

    if (chunkSize > 0) {
        mprAddItem(headers, mprCreateKeyPair("X-Appweb-Chunk-Size", mprAsprintf("%d", chunkSize)));
    }
    activeLoadThreads = loadThreads;
    for (j = 0; j < loadThreads; j++) {
        char name[64];
        mprSprintf(name, sizeof(name), "http.%d", j);
        tp = mprCreateThread(name, threadMain, mpr, 0); 
        mprStartThread(tp);
    }
}


/*  
    Per-thread execution. Called for main thread and helper threads.
 */ 
static void threadMain(void *data, MprThread *tp)
{
    HttpConn        *conn;
    MprDispatcher   *dispatcher;
    MprEvent        e;

    dispatcher = mprCreateDispatcher(tp->name, 1);
    conn = httpCreateClient(http, dispatcher);

    /*  
        Relay to processThread via the dispatcher. This serializes all activity on the conn->dispatcher
     */
    e.mask = MPR_READABLE;
    e.data = tp;
    mprRelayEvent(conn->dispatcher, (MprEventProc) processThread, conn, &e);
}


static int processThread(HttpConn *conn, MprEvent *event)
{
    cchar       *path;
    char        *url;
    int         next;

    httpFollowRedirects(conn, !nofollow);
    httpSetTimeout(conn, timeout, timeout);

    if (strcmp(protocol, "HTTP/1.0") == 0) {
        httpSetKeepAliveCount(conn, 0);
        httpSetProtocol(conn, "HTTP/1.0");
    }
    if (username) {
        if (password == 0 && !strchr(username, ':')) {
            password = getPassword();
        }
        httpSetCredentials(conn, username, password);
    }
    while (!mprIsExiting(conn) && (success || continueOnErrors)) {
        if (singleStep) waitForUser();
        if (files && !upload) {
            for (next = 0; (path = mprGetNextItem(files, &next)) != 0; ) {
                if (target[strlen(target) - 1] == '/') {
                    url = mprJoinPath(target, mprGetPathBase(path));
                } else {
                    url = target;
                }
                files = mprCreateList(conn);
                mprAddItem(files, path);
                url = resolveUrl(conn, url);
                if (verbose) {
                    mprPrintf("putting: %s to %s\n", path, url);
                }
                if (doRequest(conn, url) < 0) {
                    success = 0;
                    mprFree(files);
                    mprFree(url);
                    break;
                }
                mprFree(files);
                mprFree(url);
            }
        } else {
            url = resolveUrl(conn, target);
            if (doRequest(conn, url) < 0) {
                success = 0;
                mprFree(url);
                break;
            }
        }
        if (iterationsComplete()) {
            break;
        }
    }
    mprFree(conn);
    finishThread((MprThread*) event->data);
    return -1;
}


static int prepRequest(HttpConn *conn)
{
    MprKeyValue     *header;
    char            seqBuf[16];
    int             next;

    for (next = 0; (header = mprGetNextItem(headers, &next)) != 0; ) {
        httpAppendHeader(conn, header->key, header->value);
    }
    if (sequence) {
        static int next = 0;
        itos(seqBuf, sizeof(seqBuf), next++, 10);
        httpSetHeader(conn, "X-Http-Seq", seqBuf);
    }
    if (ranges) {
        httpSetHeader(conn, "Range", ranges);
    }
    if (formData) {
        httpSetHeader(conn, "Content-Type", "application/x-www-form-urlencoded");
    }
    if (chunkSize > 0) {
        httpSetChunkSize(conn, chunkSize);
    }
    if (setContentLength(conn) < 0) {
        return MPR_ERR_CANT_OPEN;
    }
    return 0;
}


static int sendRequest(HttpConn *conn, cchar *method, cchar *url)
{
    if (httpConnect(conn, method, url) < 0) {
        mprError("Can't process request for \"%s\". %s.", url, httpGetError(conn));
        return MPR_ERR_CANT_OPEN;
    }
    /*  
        This program does not do full-duplex writes with reads. ie. if you have a request that sends and receives
        data in parallel -- http will do the writes first then read the response.
     */
    if (bodyData || formData || files) {
        if (writeBody(conn) < 0) {
            mprError("Can't write body data to \"%s\". %s", url, httpGetError(conn));
            return MPR_ERR_CANT_WRITE;
        }
    }
    httpFinalize(conn);
    return 0;
}


static int retryRequest(HttpConn *conn, cchar *url) 
{
    HttpRx  *rx;
    HttpUri *target, *location;
    char    *redirect;
    cchar   *msg, *sep;
    int     count, redirectCount;

    httpSetRetries(conn, retries);
    httpSetTimeout(conn, timeout, timeout);

    for (redirectCount = count = 0; count <= conn->retries && redirectCount < 16 && !mprIsExiting(conn); count++) {
        if (count > 0) {
            mprLog(MPR_DEBUG, "retry %d of %d for: %s %s", count, conn->retries, method, url);
            httpSetKeepAliveCount(conn, -1);
            httpPrepClientConn(conn, HTTP_RETRY_REQUEST);
        }
        if (prepRequest(conn) < 0) {
            return MPR_ERR_CANT_OPEN;
        }
        if (sendRequest(conn, method, url) < 0) {
            return MPR_ERR_CANT_WRITE;
        }
        if (httpWait(conn, conn->dispatcher, HTTP_STATE_PARSED, conn->limits->requestTimeout) == 0) {
            if (httpNeedRetry(conn, &redirect)) {
                if (redirect) {
                    location = httpCreateUri(redirect, 0);
                    target = httpJoinUri(conn->tx->parsedUri, 1, &location);
                    url = httpUriToString(target, 1);
                    mprFree(location);
                    mprFree(target);
                    httpPrepClientConn(conn, HTTP_NEW_REQUEST);
                }
                /* Count redirects and auth retries */
                redirectCount++;
                count--; 
            } else {
                break;
            }
        } else if (!conn->error) {
            httpConnError(conn, HTTP_CODE_REQUEST_TIMEOUT,
                "Inactive request timed out, exceeded request timeout %d", timeout);
        }
        if ((rx = conn->rx) != 0) {
            if (rx->status == HTTP_CODE_REQUEST_TOO_LARGE || rx->status == HTTP_CODE_REQUEST_URL_TOO_LARGE ||
                (rx->status == HTTP_CODE_UNAUTHORIZED && conn->authUser == 0)) {
                /* No point retrying */
                break;
            }
        }
    }
    if (conn->error || conn->errorMsg) {
        msg = (conn->errorMsg) ? conn->errorMsg : "";
        sep = (msg && *msg) ? "\n" : "";
        mprError("http: failed \"%s\" request for %s after %d attempt(s).%s%s", method, url, count, sep, msg);
        return MPR_ERR_CANT_CONNECT;
    }
    return 0;
}


static int reportResponse(HttpConn *conn, cchar *url)
{
    HttpRx      *rx;
    cchar       *msg;
    char        *responseHeaders;
    int         status, contentLen;

    if (mprIsExiting(conn)) {
        return 0;
    }
    status = httpGetStatus(conn);
    contentLen = httpGetContentLength(conn);
    msg = httpGetStatusMessage(conn);

    if (conn->error) {
        success = 0;
    }
    if (conn->rx && success) {
        if (showStatus) {
            mprPrintf("%d\n", status);
        }
        if (showHeaders) {
            responseHeaders = httpGetHeaders(conn);
            rx = conn->rx;
            mprPrintfError("\nHeaders\n-------\n%s %d %s\n", conn->protocol, rx->status, rx->statusMessage);
            if (responseHeaders) {
                mprPrintfError("%s\n", responseHeaders);
                mprFree(responseHeaders);
            }
        }
    }
    if (status < 0) {
        mprError("Can't process request for \"%s\" %s", url, httpGetError(conn));
        httpDestroyRx(conn);
        return MPR_ERR_CANT_READ;

    } else if (status == 0 && conn->protocol == 0) {
        /* Ignore */;

    } else if (!(200 <= status && status <= 206) && !(301 <= status && status <= 304)) {
        if (!showStatus) {
            mprError("Can't process request for \"%s\" (%d) %s", url, status, httpGetError(conn));
            httpDestroyRx(conn);
            return MPR_ERR_CANT_READ;
        }
    }
    httpDestroyRx(conn);

    mprLock(mutex);
    if (verbose && noout) {
        trace(conn, url, fetchCount, method, status, contentLen);
    }
    mprUnlock(mutex);
    return 0;
}


static void readBody(HttpConn *conn)
{
    char    buf[HTTP_BUFSIZE];
    int     bytes;

    while (!conn->error && conn->sock && (bytes = httpRead(conn, buf, sizeof(buf))) > 0) {
        showOutput(conn, buf, bytes);
    }
}

static int doRequest(HttpConn *conn, cchar *url)
{
    MprTime         mark;
    MprFile         *file;
    HttpLimits      *limits;

    mprAssert(url && *url);
    file = 0;
    limits = conn->limits;

    mprLog(MPR_DEBUG, "fetch: %s %s", method, url);
    mark = mprGetTime(mpr);

    if (retryRequest(conn, url) < 0) {
        return MPR_ERR_CANT_CONNECT;
    }
    while (!conn->error && conn->state < HTTP_STATE_COMPLETE && mprGetElapsedTime(mark) <= limits->requestTimeout) {
        httpWait(conn, conn->dispatcher, HTTP_STATE_COMPLETE, 10);
        readBody(conn);
    }
    if (conn->state < HTTP_STATE_COMPLETE && !conn->error) {
        httpConnError(conn, HTTP_CODE_REQUEST_TIMEOUT,
            "Inactive request timed out, exceeded request timeout %d", timeout);
    } else {
        readBody(conn);
    }
    mprLog(6, "Response status %d, elapsed %d", httpGetStatus(conn), ((int) mprGetTime(mpr)) - mark);
    reportResponse(conn, url);
    return 0;
}

static int setContentLength(HttpConn *conn)
{
    MprPath     info;
    char        *path, *pair;
    int         len;
    int         next, count;

    len = 0;
    if (upload) {
        httpEnableUpload(conn);
        return 0;
    }
    for (next = 0; (path = mprGetNextItem(files, &next)) != 0; ) {
        if (strcmp(path, "-") != 0) {
            if (mprGetPathInfo(path, &info) < 0) {
                mprError("Can't access file %s", path);
                return MPR_ERR_CANT_ACCESS;
            }
            len += (int) info.size;
        }
    }
    if (formData) {
        count = mprGetListCount(formData);
        for (next = 0; (pair = mprGetNextItem(formData, &next)) != 0; ) {
            len += strlen(pair);
        }
        len += mprGetListCount(formData) - 1;
    }
    if (bodyData) {
        len += mprGetBufLength(bodyData);
    }
    if (len > 0) {
        httpSetContentLength(conn, len);
    }
    return 0;
}


static int writeBody(HttpConn *conn)
{
    MprFile     *file;
    char        buf[HTTP_BUFSIZE], *path, *pair;
    int         bytes, next, count, rc, len;

    rc = 0;
    if (upload) {
        if (httpWriteUploadData(conn, files, formData) < 0) {
            mprError("Can't write upload data %s", httpGetError(conn));
            return MPR_ERR_CANT_WRITE;
        }
    } else {
        if (formData) {
            count = mprGetListCount(formData);
            for (next = 0; !rc && (pair = mprGetNextItem(formData, &next)) != 0; ) {
                len = strlen(pair);
                if (next < count) {
                    len = strlen(pair);
                    if (httpWrite(conn->writeq, pair, len) != len || httpWrite(conn->writeq, "&", 1) != 1) {
                        return MPR_ERR_CANT_WRITE;
                    }
                } else {
                    if (httpWrite(conn->writeq, pair, len) != len) {
                        return MPR_ERR_CANT_WRITE;
                    }
                }
            }
        }
        if (files) {
            mprAssert(mprGetListCount(files) == 1);
            for (rc = next = 0; !rc && (path = mprGetNextItem(files, &next)) != 0; ) {
                if (strcmp(path, "-") == 0) {
                    file = mprAttachFd(0, "stdin", O_RDONLY | O_BINARY);
                } else {
                    file = mprOpen(path, O_RDONLY | O_BINARY, 0);
                }
                if (file == 0) {
                    mprError("Can't open \"%s\"", path);
                    return MPR_ERR_CANT_OPEN;
                }
                if (verbose) {
                    //  MOB - should this be to stdout or stderr?
                    mprPrintf("uploading: %s\n", path);
                }
                while ((bytes = mprRead(file, buf, sizeof(buf))) > 0) {
                    if (httpWriteBlock(conn->writeq, buf, bytes) != bytes) {
                        mprFree(file);
                        return MPR_ERR_CANT_WRITE;
                    }
                }
                mprFree(file);
            }
        }
        if (bodyData) {
            mprAddNullToBuf(bodyData);
            len = strlen(bodyData->start);
            len = mprGetBufLength(bodyData);
            if (httpWriteBlock(conn->writeq, mprGetBufStart(bodyData), len) != len) {
                return MPR_ERR_CANT_WRITE;
            }
        }
    }
    return rc;
}


static bool iterationsComplete()
{
    mprLock(mutex);
    if (verbose > 1) mprPrintf(".");
    if (++fetchCount >= iterations) {
        mprUnlock(mutex);
        return 1;
    }
    mprUnlock(mutex);
    return 0;
}


static void finishThread(MprThread *tp)
{
    if (tp) {
        mprLock(mutex);
        activeLoadThreads--;
        mprUnlock(mutex);
    }
}


static void waitForUser()
{
    int     c, rc;

    mprLock(mutex);
    mprPrintf("Pause: ");
    rc = read(0, (char*) &c, 1);
    mprUnlock(mutex);
}


static void addFormVars(cchar *buf)
{
    char    *pair, *tok;

    pair = stok(sclone(buf), "&", &tok);
    while (pair != 0) {
        mprAddItem(formData, pair);
        pair = stok(0, "&", &tok);
    }
}


static bool isPort(cchar *name)
{
    cchar   *cp;

    for (cp = name; *cp && *cp != '/'; cp++) {
        if (!isdigit((int) *cp) || *cp == '.') {
            return 0;
        }
    }
    return 1;
}


static char *resolveUrl(HttpConn *conn, cchar *url)
{
    //  TODO replace with Url join
    if (*url == '/') {
        if (host) {
            if (sncasecmp(host, "http://", 7) != 0 && sncasecmp(host, "https://", 8) != 0) {
                return mprAsprintf("http://%s%s", host, url);
            } else {
                return mprAsprintf("%s%s", host, url);
            }
        } else {
            return mprAsprintf("http://127.0.0.1%s", url);
        }
    } 
    if (sncasecmp(url, "http://", 7) != 0 && sncasecmp(url, "https://", 8) != 0) {
        if (isPort(url)) {
            return mprAsprintf("http://127.0.0.1:%s", url);
        } else {
            return mprAsprintf("http://%s", url);
        }
    }
    return sclone(url);
}


static void showOutput(HttpConn *conn, cchar *buf, int count)
{
    HttpRx      *rx;
    int         i, c, rc;
    
    rx = conn->rx;

    if (noout) {
        return;
    }
    if (rx->status == 401 || (conn->followRedirects && (301 <= rx->status && rx->status <= 302))) {
        return;
    }
    if (!printable) {
        rc = write(1, (char*) buf, count);
        return;
    }

    for (i = 0; i < count; i++) {
        if (!isprint((int) buf[i]) && buf[i] != '\n' && buf[i] != '\r' && buf[i] != '\t') {
            isBinary = 1;
            break;
        }
    }
    if (!isBinary) {
        rc = write(1, (char*) buf, count);
        return;
    }
    for (i = 0; i < count; i++) {
        c = (uchar) buf[i];
        if (printable && isBinary) {
            mprPrintf("%02x ", c & 0xff);
        } else {
            mprPrintf("%c", (int) buf[i]);
        }
    }
}


static void trace(HttpConn *conn, cchar *url, int fetchCount, cchar *method, int status, int contentLen)
{
    if (sncasecmp(url, "http://", 7) != 0) {
        url += 7;
    }
    if ((fetchCount % 200) == 1) {
        if (fetchCount == 1 || (fetchCount % 5000) == 1) {
            if (fetchCount > 1) {
                mprPrintf("\n");
            }
            mprPrintf("  Count  Thread   Op  Code   Bytes  Url\n");
        }
        mprPrintf("%7d %7s %4s %5d %7d  %s\n", fetchCount - 1,
            mprGetCurrentThreadName(conn), method, status, contentLen, url);
    }
}


static void logHandler(int flags, int level, const char *msg)
{
    Mpr         *mpr;
    MprFile     *file;
    char        *prefix;

    mpr = mprGetMpr();
    file = (MprFile*) mpr->logData;
    prefix = mpr->name;

    while (*msg == '\n') {
        mprFprintf(file, "\n");
        msg++;
    }
    if (flags & MPR_LOG_SRC) {
        mprFprintf(file, "%s: %d: %s\n", prefix, level, msg);
    } else if (flags & MPR_ERROR_SRC) {
        mprFprintf(file, "%s: Error: %s\n", prefix, msg);
    } else if (flags & MPR_FATAL_SRC) {
        mprFprintf(file, "%s: Fatal: %s\n", prefix, msg);
    } else if (flags & MPR_ASSERT_SRC) {
        mprFprintf(file, "%s: Assertion %s, failed\n", prefix, msg);
    } else if (flags & MPR_RAW) {
        mprFprintf(file, "%s", msg);
    }
    if (flags & (MPR_ERROR_SRC | MPR_FATAL_SRC | MPR_ASSERT_SRC)) {
        mprBreakpoint();
    }
}



static int startLogging(Mpr *mpr, char *logSpec)
{
    MprFile     *file;
    char        *levelSpec;
    int         level;

    level = 0;

    //  TODO - move should not be changing logSpec.
    if ((levelSpec = strchr(logSpec, ':')) != 0) {
        *levelSpec++ = '\0';
        level = atoi(levelSpec);
    }
    if (strcmp(logSpec, "stdout") == 0) {
        file = mpr->fileSystem->stdOutput;
    } else {
        if ((file = mprOpen(logSpec, O_CREAT | O_WRONLY | O_TRUNC | O_TEXT, 0664)) == 0) {
            mprPrintfError("Can't open log file %s\n", logSpec);
            return -1;
        }
    }
    mprSetLogLevel(level);
    mprSetLogHandler(logHandler, (void*) file);
    return 0;
}


#if (BLD_WIN_LIKE && !WINCE) || VXWORKS
static char *getpass(char *prompt)
{
    static char password[MPR_MAX_STRING];
    int     c, i;

    fputs(prompt, stderr);
    for (i = 0; i < (int) sizeof(password) - 1; i++) {
#if VXWORKS
        c = getchar();
#else
        c = _getch();
#endif
        if (c == '\r' || c == EOF) {
            break;
        }
        if ((c == '\b' || c == 127) && i > 0) {
            password[--i] = '\0';
            fputs("\b \b", stderr);
            i--;
        } else if (c == 26) {           /* Control Z */
            c = EOF;
            break;
        } else if (c == 3) {            /* Control C */
            fputs("^C\n", stderr);
            exit(255);
        } else if (!iscntrl(c) && (i < (int) sizeof(password) - 1)) {
            password[i] = c;
            fputc('*', stderr);
        } else {
            fputc('', stderr);
            i--;
        }
    }
    if (c == EOF) {
        return "";
    }
    fputc('\n', stderr);
    password[i] = '\0';
    return password;
}

#endif /* WIN */


static char *getPassword()
{
#if !WINCE
    char    *password;

    password = getpass("Password: ");
#else
    password = "no-user-interaction-support";
#endif
    return sclone(password);
}


#if VXWORKS
/*
    VxWorks link resolution
 */
int _cleanup() {
    return 0;
}
int _exit() {
    return 0;
}
#endif

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
