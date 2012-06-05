/*
    route.c -- Http request routing 

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"
#include    "pcre.h"

/********************************** Forwards **********************************/

#define GRADUATE_LIST(route, field) \
    if (route->field == 0) { \
        route->field = mprCreateList(-1, 0); \
    } else if (route->parent && route->field == route->parent->field) { \
        route->field = mprCloneList(route->parent->field); \
    }

#define GRADUATE_HASH(route, field) \
    mprAssert(route->field) ; \
    if (route->parent && route->field == route->parent->field) { \
        route->field = mprCloneHash(route->parent->field); \
    }

/********************************** Forwards **********************************/

static void addUniqueItem(MprList *list, HttpRouteOp *op);
static HttpLang *createLangDef(cchar *path, cchar *suffix, int flags);
static HttpRouteOp *createRouteOp(cchar *name, int flags);
static void definePathVars(HttpRoute *route);
static void defineHostVars(HttpRoute *route);
static char *expandTokens(HttpConn *conn, cchar *path);
static char *expandPatternTokens(cchar *str, cchar *replacement, int *matches, int matchCount);
static char *expandRequestTokens(HttpConn *conn, char *targetKey);
static cchar *expandRouteName(HttpConn *conn, cchar *routeName);
static void finalizeMethods(HttpRoute *route);
static void finalizePattern(HttpRoute *route);
static char *finalizeReplacement(HttpRoute *route, cchar *str);
static char *finalizeTemplate(HttpRoute *route);
static bool opPresent(MprList *list, HttpRouteOp *op);
static void manageRoute(HttpRoute *route, int flags);
static void manageLang(HttpLang *lang, int flags);
static void manageRouteOp(HttpRouteOp *op, int flags);
static int matchRequestUri(HttpConn *conn, HttpRoute *route);
static int testRoute(HttpConn *conn, HttpRoute *route);
static char *qualifyName(HttpRoute *route, cchar *controller, cchar *name);
static int selectHandler(HttpConn *conn, HttpRoute *route);
static int testCondition(HttpConn *conn, HttpRoute *route, HttpRouteOp *condition);
static char *trimQuotes(char *str);
static int updateRequest(HttpConn *conn, HttpRoute *route, HttpRouteOp *update);

/************************************ Code ************************************/
/*
    Host may be null
 */
HttpRoute *httpCreateRoute(HttpHost *host)
{
    HttpRoute  *route;

    if ((route = mprAllocObj(HttpRoute, manageRoute)) == 0) {
        return 0;
    }
    route->auth = httpCreateAuth();
    route->defaultLanguage = sclone("en");
    route->dir = mprGetCurrentPath(".");
    route->errorDocuments = mprCreateHash(HTTP_SMALL_HASH_SIZE, 0);
    route->extensions = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS);
    route->flags = HTTP_ROUTE_GZIP;
    route->handlers = mprCreateList(-1, 0);
    route->handlersWithMatch = mprCreateList(-1, 0);
    route->host = host;
    route->http = MPR->httpService;
    route->indicies = mprCreateList(-1, 0);
    route->inputStages = mprCreateList(-1, 0);
    route->lifespan = HTTP_CACHE_LIFESPAN;
    route->outputStages = mprCreateList(-1, 0);
    route->pathTokens = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS);
    route->pattern = MPR->emptyString;
    route->targetRule = sclone("run");
    route->autoDelete = 1;
    route->workers = -1;

    if (MPR->httpService) {
        route->limits = mprMemdup(((Http*) MPR->httpService)->serverLimits, sizeof(HttpLimits));
    }
    route->mimeTypes = MPR->mimeTypes;
    route->mutex = mprCreateLock();
    httpInitTrace(route->trace);

    if ((route->mimeTypes = mprCreateMimeTypes("mime.types")) == 0) {
        route->mimeTypes = MPR->mimeTypes;                                                                  
    }  
    definePathVars(route);
    return route;
}


/*  
    Create a new location block. Inherit from the parent. We use a copy-on-write scheme if these are modified later.
 */
HttpRoute *httpCreateInheritedRoute(HttpRoute *parent)
{
    HttpRoute  *route;

    mprAssert(parent);
    if ((route = mprAllocObj(HttpRoute, manageRoute)) == 0) {
        return 0;
    }
    route->parent = parent;
    route->auth = httpCreateInheritedAuth(parent->auth);
    route->autoDelete = parent->autoDelete;
    route->caching = parent->caching;
    route->conditions = parent->conditions;
    route->connector = parent->connector;
    route->defaultLanguage = parent->defaultLanguage;
    route->dir = parent->dir;
    route->data = parent->data;
    route->eroute = parent->eroute;
    route->errorDocuments = parent->errorDocuments;
    route->extensions = parent->extensions;
    route->handler = parent->handler;
    route->handlers = parent->handlers;
    route->handlersWithMatch = parent->handlersWithMatch;
    route->headers = parent->headers;
    route->http = MPR->httpService;
    route->host = parent->host;
    route->inputStages = parent->inputStages;
    route->indicies = parent->indicies;
    route->languages = parent->languages;
    route->lifespan = parent->lifespan;
    route->methods = parent->methods;
    route->methodSpec = parent->methodSpec;
    route->outputStages = parent->outputStages;
    route->params = parent->params;
    route->parent = parent;
    route->pathTokens = parent->pathTokens;
    route->pattern = parent->pattern;
    route->patternCompiled = parent->patternCompiled;
    route->optimizedPattern = parent->optimizedPattern;
    route->responseStatus = parent->responseStatus;
    route->script = parent->script;
    route->prefix = parent->prefix;
    route->prefixLen = parent->prefixLen;
    route->scriptPath = parent->scriptPath;
    route->sourceName = parent->sourceName;
    route->sourcePath = parent->sourcePath;
    route->ssl = parent->ssl;
    route->target = parent->target;
    route->targetRule = parent->targetRule;
    route->tokens = parent->tokens;
    route->updates = parent->updates;
    route->uploadDir = parent->uploadDir;
    route->workers = parent->workers;
    route->limits = parent->limits;
    route->mimeTypes = parent->mimeTypes;
    route->trace[0] = parent->trace[0];
    route->trace[1] = parent->trace[1];
    route->log = parent->log;
    route->logFormat = parent->logFormat;
    route->logPath = parent->logPath;
    route->logSize = parent->logSize;
    route->logBackup = parent->logBackup;
    route->logFlags = parent->logFlags;
    route->flags = parent->flags & ~(HTTP_ROUTE_FREE_PATTERN);
    return route;
}


static void manageRoute(HttpRoute *route, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(route->name);
        mprMark(route->pattern);
        mprMark(route->startSegment);
        mprMark(route->startWith);
        mprMark(route->optimizedPattern);
        mprMark(route->prefix);
        mprMark(route->tplate);
        mprMark(route->targetRule);
        mprMark(route->target);
        mprMark(route->dir);
        mprMark(route->indicies);
        mprMark(route->methodSpec);
        mprMark(route->handler);
        mprMark(route->caching);
        mprMark(route->auth);
        mprMark(route->http);
        mprMark(route->host);
        mprMark(route->parent);
        mprMark(route->defaultLanguage);
        mprMark(route->extensions);
        mprMark(route->handlers);
        mprMark(route->handlersWithMatch);
        mprMark(route->connector);
        mprMark(route->data);
        mprMark(route->eroute);
        mprMark(route->pathTokens);
        mprMark(route->languages);
        mprMark(route->inputStages);
        mprMark(route->outputStages);
        mprMark(route->errorDocuments);
        mprMark(route->context);
        mprMark(route->uploadDir);
        mprMark(route->script);
        mprMark(route->scriptPath);
        mprMark(route->methods);
        mprMark(route->params);
        mprMark(route->headers);
        mprMark(route->conditions);
        mprMark(route->updates);
        mprMark(route->sourceName);
        mprMark(route->sourcePath);
        mprMark(route->tokens);
        mprMark(route->ssl);
        mprMark(route->limits);
        mprMark(route->mimeTypes);
        httpManageTrace(&route->trace[0], flags);
        httpManageTrace(&route->trace[1], flags);
        mprMark(route->log);
        mprMark(route->logFormat);
        mprMark(route->logPath);
        mprMark(route->mutex);

    } else if (flags & MPR_MANAGE_FREE) {
        if (route->patternCompiled && (route->flags & HTTP_ROUTE_FREE_PATTERN)) {
            free(route->patternCompiled);
            route->patternCompiled = 0;
        }
    }
}


HttpRoute *httpCreateDefaultRoute(HttpHost *host)
{
    HttpRoute   *route;

    mprAssert(host);
    if ((route = httpCreateRoute(host)) == 0) {
        return 0;
    }
    httpSetRouteName(route, "default");
    httpFinalizeRoute(route);
    return route;
}


/*
    Create and configure a basic route. This is mainly used for client side piplines.
    Host may be null.
 */
HttpRoute *httpCreateConfiguredRoute(HttpHost *host, int serverSide)
{
    HttpRoute   *route;
    Http        *http;

    /*
        Create default incoming and outgoing pipelines. Order matters.
     */
    route = httpCreateRoute(host);
    http = route->http;
    httpAddRouteFilter(route, http->rangeFilter->name, NULL, HTTP_STAGE_TX);
    httpAddRouteFilter(route, http->chunkFilter->name, NULL, HTTP_STAGE_RX | HTTP_STAGE_TX);
    if (serverSide) {
        httpAddRouteFilter(route, http->uploadFilter->name, NULL, HTTP_STAGE_RX);
    }
    return route;
}


HttpRoute *httpCreateAliasRoute(HttpRoute *parent, cchar *pattern, cchar *path, int status)
{
    HttpRoute   *route;

    mprAssert(parent);
    mprAssert(pattern && *pattern);

    if ((route = httpCreateInheritedRoute(parent)) == 0) {
        return 0;
    }
    httpSetRoutePattern(route, pattern, 0);
    if (path) {
        httpSetRouteDir(route, path);
    }
    route->responseStatus = status;
    return route;
}


int httpStartRoute(HttpRoute *route)
{
#if !BIT_FEATURE_ROMFS
    if (!(route->flags & HTTP_ROUTE_STARTED)) {
        route->flags |= HTTP_ROUTE_STARTED;
        if (route->logPath && (!route->parent || route->logPath != route->parent->logPath)) {
            if (route->logBackup > 0) {
                httpBackupRouteLog(route);
            }
            mprAssert(!route->log);
            route->log = mprOpenFile(route->logPath, O_CREAT | O_APPEND | O_WRONLY | O_TEXT, 0664);
            if (route->log == 0) {
                mprError("Can't open log file %s", route->logPath);
                return MPR_ERR_CANT_OPEN;
            }
        }
    }
#endif
    return 0;
}


void httpStopRoute(HttpRoute *route)
{
    route->log = 0;
}


/*
    Find the matching route and handler for a request. If any errors occur, the pass handler is used to 
    pass errors via the net/sendfile connectors onto the client. This process may rewrite the request 
    URI and may redirect the request.
 */
void httpRouteRequest(HttpConn *conn)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpRoute   *route;
    int         next, rewrites, match;

    rx = conn->rx;
    tx = conn->tx;

    for (next = rewrites = 0; rewrites < HTTP_MAX_REWRITE; ) {
        if ((route = mprGetNextItem(conn->host->routes, &next)) == 0) {
            break;
        }
        if (route->startSegment && strncmp(rx->pathInfo, route->startSegment, route->startSegmentLen) != 0) {
            /* Failed to match the first URI segment, skip to the next group */
            mprAssert(next <= route->nextGroup);
            next = route->nextGroup;

        } else if (route->startWith && strncmp(rx->pathInfo, route->startWith, route->startWithLen) != 0) {
            /* Failed to match starting literal segment of the route pattern, advance to test the next route */
            continue;

        } else if ((match = httpMatchRoute(conn, route)) == HTTP_ROUTE_REROUTE) {
            next = 0;
            route = 0;
            rewrites++;

        } else if (match == HTTP_ROUTE_OK) {
            break;
        }
    }
    if (route == 0 || tx->handler == 0) {
        httpError(conn, HTTP_CODE_INTERNAL_SERVER_ERROR, "Can't find suitable route for request");
        return;
    }
    if (rx->traceLevel >= 0) {
        mprLog(4, "Select route \"%s\" target \"%s\"", route->name, route->targetRule);
    }
    rx->route = route;
    conn->limits = route->limits;

    conn->trace[0] = route->trace[0];
    conn->trace[1] = route->trace[1];

    if (rewrites >= HTTP_MAX_REWRITE) {
        httpError(conn, HTTP_CODE_INTERNAL_SERVER_ERROR, "Too many request rewrites");
    }
    if (conn->finalized) {
        /* 
            Must be no data in queues as handlers and pipeline are not yet started. Only errors and redirects 
            using tx->altBody 
         */ 
        tx->handler = conn->http->passHandler;
    }
    if (rx->traceLevel >= 0) {
        mprLog(rx->traceLevel, "Select handler: \"%s\" for \"%s\"", tx->handler->name, rx->uri);
    }
}



int httpMatchRoute(HttpConn *conn, HttpRoute *route)
{
    HttpRx      *rx;
    char        *savePathInfo, *pathInfo;
    int         rc;

    mprAssert(conn);
    mprAssert(route);

    rx = conn->rx;
    savePathInfo = 0;

    /*
        Remove the route prefix. Restore after matching.
     */
    if (route->prefix) {
        savePathInfo = rx->pathInfo;
        pathInfo = &rx->pathInfo[route->prefixLen];
        if (*pathInfo == '\0') {
            pathInfo = "/";
        }
        rx->pathInfo = sclone(pathInfo);
        rx->scriptName = route->prefix;
    }
    if ((rc = matchRequestUri(conn, route)) == HTTP_ROUTE_OK) {
        rc = testRoute(conn, route);
    }
    if (rc == HTTP_ROUTE_REJECT && route->prefix) {
        /* Keep the modified pathInfo if OK or REWRITE */
        rx->pathInfo = savePathInfo;
        rx->scriptName = 0;
    }
    return rc;
}


static int matchRequestUri(HttpConn *conn, HttpRoute *route)
{
    HttpRx      *rx;

    mprAssert(conn);
    mprAssert(route);
    rx = conn->rx;

    if (route->patternCompiled) {
        rx->matchCount = pcre_exec(route->patternCompiled, NULL, rx->pathInfo, (int) slen(rx->pathInfo), 0, 0, 
                rx->matches, sizeof(rx->matches) / sizeof(int));
        mprLog(6, "Test route pattern \"%s\", regexp %s, pathInfo %s", route->name, route->optimizedPattern, rx->pathInfo);

        if (route->flags & HTTP_ROUTE_NOT) {
            if (rx->matchCount > 0) {
                return HTTP_ROUTE_REJECT;
            }
            rx->matchCount = 1;
            rx->matches[0] = 0;
            rx->matches[1] = (int) slen(rx->pathInfo);

        } else if (rx->matchCount <= 0) {
            return HTTP_ROUTE_REJECT;
        }
    }
    mprLog(6, "Test route methods \"%s\"", route->name);
    if (route->methods && !mprLookupKey(route->methods, rx->method)) {
        return HTTP_ROUTE_REJECT;
    }
    rx->route = route;
    return HTTP_ROUTE_OK;
}


static int testRoute(HttpConn *conn, HttpRoute *route)
{
    HttpRouteOp     *op, *condition, *update;
    HttpRouteProc   *proc;
    HttpRx          *rx;
    HttpTx          *tx;
    cchar           *token, *value, *header, *field;
    int             next, rc, matched[HTTP_MAX_ROUTE_MATCHES * 2], count, result;

    mprAssert(conn);
    mprAssert(route);
    rx = conn->rx;
    tx = conn->tx;

    rx->target = route->target ? expandTokens(conn, route->target) : sclone(&conn->rx->pathInfo[1]);

    if (route->headers) {
        for (next = 0; (op = mprGetNextItem(route->headers, &next)) != 0; ) {
            mprLog(6, "Test route \"%s\" header \"%s\"", route->name, op->name);
            if ((header = httpGetHeader(conn, op->name)) != 0) {
                count = pcre_exec(op->mdata, NULL, header, (int) slen(header), 0, 0, 
                    matched, sizeof(matched) / sizeof(int)); 
                result = count > 0;
                if (op->flags & HTTP_ROUTE_NOT) {
                    result = !result;
                }
                if (!result) {
                    return HTTP_ROUTE_REJECT;
                }
            }
        }
    }
    if (route->params) {
        for (next = 0; (op = mprGetNextItem(route->params, &next)) != 0; ) {
            mprLog(6, "Test route \"%s\" field \"%s\"", route->name, op->name);
            if ((field = httpGetParam(conn, op->name, "")) != 0) {
                count = pcre_exec(op->mdata, NULL, field, (int) slen(field), 0, 0, 
                    matched, sizeof(matched) / sizeof(int)); 
                result = count > 0;
                if (op->flags & HTTP_ROUTE_NOT) {
                    result = !result;
                }
                if (!result) {
                    return HTTP_ROUTE_REJECT;
                }
            }
        }
    }
    if (route->conditions) {
        for (next = 0; (condition = mprGetNextItem(route->conditions, &next)) != 0; ) {
            mprLog(6, "Test route \"%s\" condition \"%s\"", route->name, condition->name);
            rc = testCondition(conn, route, condition);
            if (rc == HTTP_ROUTE_REROUTE) {
                return rc;
            }
            if (condition->flags & HTTP_ROUTE_NOT) {
                rc = !rc;
            }
            if (rc == HTTP_ROUTE_REJECT) {
                return rc;
            }
        }
    }
    if (route->updates) {
        for (next = 0; (update = mprGetNextItem(route->updates, &next)) != 0; ) {
            mprLog(6, "Run route \"%s\" update \"%s\"", route->name, update->name);
            if ((rc = updateRequest(conn, route, update)) == HTTP_ROUTE_REROUTE) {
                return rc;
            }
        }
    }
    if (route->prefix) {
        /* This is needed by some handler match routines */
        httpSetParam(conn, "prefix", route->prefix);
    }
    if ((rc = selectHandler(conn, route)) != HTTP_ROUTE_OK) {
        return rc;
    }
    if (route->tokens) {
        for (next = 0; (token = mprGetNextItem(route->tokens, &next)) != 0; ) {
            value = snclone(&rx->pathInfo[rx->matches[next * 2]], rx->matches[(next * 2) + 1] - rx->matches[(next * 2)]);
            httpSetParam(conn, token, value);
        }
    }
    if ((proc = mprLookupKey(conn->http->routeTargets, route->targetRule)) == 0) {
        httpError(conn, -1, "Can't find route target rule \"%s\"", route->targetRule);
        return HTTP_ROUTE_REJECT;
    }
    if ((rc = (*proc)(conn, route, 0)) != HTTP_ROUTE_OK) {
        return rc;
    }
    if (!conn->finalized && tx->handler->match) {
        rc = tx->handler->match(conn, route, HTTP_QUEUE_TX);
    }
    return rc;
}


static int selectHandler(HttpConn *conn, HttpRoute *route)
{
    HttpTx      *tx;
    int         next, rc;

    mprAssert(conn);
    mprAssert(route);

    tx = conn->tx;
    if (route->handler) {
        tx->handler = route->handler;
        return HTTP_ROUTE_OK;
    }
    /*
        Handlers with match routines are examined first (in-order)
     */
    for (next = 0; (tx->handler = mprGetNextItem(route->handlersWithMatch, &next)) != 0; ) {
        rc = tx->handler->match(conn, route, 0);
        if (rc == HTTP_ROUTE_OK || rc == HTTP_ROUTE_REROUTE) {
            return rc;
        }
    }
    if (!tx->handler) {
        /*
            Now match by extensions
         */
        if (!tx->ext || (tx->handler = mprLookupKey(route->extensions, tx->ext)) == 0) {
            tx->handler = mprLookupKey(route->extensions, "");
        }
    }
    return tx->handler ? HTTP_ROUTE_OK : HTTP_ROUTE_REJECT;
}


/*
    Map the target to physical storage. Sets tx->filename and tx->ext.
 */
void httpMapFile(HttpConn *conn, HttpRoute *route)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpLang    *lang;
    MprPath     *info;

    mprAssert(conn);
    mprAssert(route);

    rx = conn->rx;
    tx = conn->tx;
    lang = rx->lang;

    mprAssert(rx->target);
    tx->filename = rx->target;
    if (lang && lang->path) {
        tx->filename = mprJoinPath(lang->path, tx->filename);
    }
    tx->filename = mprJoinPath(route->dir, tx->filename);
    tx->ext = httpGetExt(conn);
    info = &tx->fileInfo;
    mprGetPathInfo(tx->filename, info);
    if (info->valid) {
        tx->etag = sfmt("\"%x-%Lx-%Lx\"", info->inode, info->size, info->mtime);
    }
    LOG(7, "mapFile uri \"%s\", filename: \"%s\", extension: \"%s\"", rx->uri, tx->filename, tx->ext);
}


/************************************ API *************************************/

int httpAddRouteCondition(HttpRoute *route, cchar *name, cchar *details, int flags)
{
    HttpRouteOp *op;
    cchar       *errMsg;
    char        *pattern, *value;
    int         column;

    mprAssert(route);

    GRADUATE_LIST(route, conditions);
    if ((op = createRouteOp(name, flags)) == 0) {
        return MPR_ERR_MEMORY;
    }
    if (scasematch(name, "auth")) {
        /* Nothing to do. Route->auth has it all */

    } else if (scasematch(name, "missing")) {
        op->details = finalizeReplacement(route, "${request:filename}");

    } else if (scasematch(name, "directory")) {
        op->details = finalizeReplacement(route, details);

    } else if (scasematch(name, "exists")) {
        op->details = finalizeReplacement(route, details);

    } else if (scasematch(name, "match")) {
        /* 
            Condition match string pattern
            String can contain matching ${tokens} from the route->pattern and can contain request ${tokens}
         */
        if (!httpTokenize(route, details, "%S %S", &value, &pattern)) {
            return MPR_ERR_BAD_SYNTAX;
        }
        if ((op->mdata = pcre_compile2(pattern, 0, 0, &errMsg, &column, NULL)) == 0) {
            mprError("Can't compile condition match pattern. Error %s at column %d", errMsg, column); 
            return MPR_ERR_BAD_SYNTAX;
        }
        op->details = finalizeReplacement(route, value);
        op->flags |= HTTP_ROUTE_FREE;
    }
    addUniqueItem(route->conditions, op);
    return 0;
}


int httpAddRouteFilter(HttpRoute *route, cchar *name, cchar *extensions, int direction)
{
    HttpStage   *stage;
    HttpStage   *filter;
    char        *extlist, *word, *tok;
    int         pos;

    mprAssert(route);
    
    stage = httpLookupStage(route->http, name);
    if (stage == 0) {
        mprError("Can't find filter %s", name); 
        return MPR_ERR_CANT_FIND;
    }
    /*
        Clone an existing stage because each filter stores its own set of extensions to match against
     */
    filter = httpCloneStage(route->http, stage);

    if (extensions && *extensions) {
        filter->extensions = mprCreateHash(0, MPR_HASH_CASELESS);
        extlist = sclone(extensions);
        word = stok(extlist, " \t\r\n", &tok);
        while (word) {
            if (*word == '*' && word[1] == '.') {
                word += 2;
            } else if (*word == '.') {
                word++;
            } else if (*word == '\"' && word[1] == '\"') {
                word = "";
            }
            mprAddKey(filter->extensions, word, filter);
            word = stok(0, " \t\r\n", &tok);
        }
    }
    if (direction & HTTP_STAGE_RX && filter->incoming) {
        GRADUATE_LIST(route, inputStages);
        mprAddItem(route->inputStages, filter);
    }
    if (direction & HTTP_STAGE_TX && filter->outgoing) {
        GRADUATE_LIST(route, outputStages);
        if (smatch(name, "cacheFilter") && 
                (pos = mprGetListLength(route->outputStages) - 1) >= 0 &&
                smatch(((HttpStage*) mprGetLastItem(route->outputStages))->name, "chunkFilter")) {
            mprInsertItemAtPos(route->outputStages, pos, filter);
        } else {
            mprAddItem(route->outputStages, filter);
        }
    }
    return 0;
}


int httpAddRouteHandler(HttpRoute *route, cchar *name, cchar *extensions)
{
    Http            *http;
    HttpStage       *handler;
    char            *extlist, *word, *tok;

    mprAssert(route);

    http = route->http;
    if ((handler = httpLookupStage(http, name)) == 0) {
        mprError("Can't find stage %s", name); 
        return MPR_ERR_CANT_FIND;
    }
    GRADUATE_HASH(route, extensions);

    if (extensions && *extensions) {
        /*
            Add to the handler extension hash. Skip over "*." and "."
         */ 
        extlist = sclone(extensions);
        word = stok(extlist, " \t\r\n", &tok);
        while (word) {
            if (*word == '*' && word[1] == '.') {
                word += 2;
            } else if (*word == '.') {
                word++;
            } else if (*word == '\"' && word[1] == '\"') {
                word = "";
            }
            mprAddKey(route->extensions, word, handler);
            word = stok(0, " \t\r\n", &tok);
        }

    } else {
        if (mprLookupItem(route->handlers, handler) < 0) {
            GRADUATE_LIST(route, handlers);
            mprAddItem(route->handlers, handler);
        }
        if (handler->match) {
            if (mprLookupItem(route->handlersWithMatch, handler) < 0) {
                GRADUATE_LIST(route, handlersWithMatch);
                mprAddItem(route->handlersWithMatch, handler);
            }
        } else {
            /*
                Only match by extensions if no-match routine provided.
             */
            mprAddKey(route->extensions, "", handler);
        }
    }
    return 0;
}


/*
    Header field valuePattern
 */
void httpAddRouteHeader(HttpRoute *route, cchar *header, cchar *value, int flags)
{
    HttpRouteOp     *op;
    cchar           *errMsg;
    int             column;

    mprAssert(route);
    mprAssert(header && *header);
    mprAssert(value && *value);

    GRADUATE_LIST(route, headers);
    if ((op = createRouteOp(header, flags | HTTP_ROUTE_FREE)) == 0) {
        return;
    }
    if ((op->mdata = pcre_compile2(value, 0, 0, &errMsg, &column, NULL)) == 0) {
        mprError("Can't compile header pattern. Error %s at column %d", errMsg, column); 
    } else {
        mprAddItem(route->headers, op);
    }
}


#if FUTURE && KEEP
void httpAddRouteLoad(HttpRoute *route, cchar *module, cchar *path)
{
    HttpRouteOp     *op;

    GRADUATE_LIST(route, updates);
    if ((op = createRouteOp("--load--", 0)) == 0) {
        return;
    }
    op->var = sclone(module);
    op->value = sclone(path);
    mprAddItem(route->updates, op);
}
#endif


/*
    Param field valuePattern
 */
void httpAddRouteParam(HttpRoute *route, cchar *field, cchar *value, int flags)
{
    HttpRouteOp     *op;
    cchar           *errMsg;
    int             column;

    mprAssert(route);
    mprAssert(field && *field);
    mprAssert(value && *value);

    GRADUATE_LIST(route, params);
    if ((op = createRouteOp(field, flags | HTTP_ROUTE_FREE)) == 0) {
        return;
    }
    if ((op->mdata = pcre_compile2(value, 0, 0, &errMsg, &column, NULL)) == 0) {
        mprError("Can't compile field pattern. Error %s at column %d", errMsg, column); 
    } else {
        mprAddItem(route->params, op);
    }
}


/*
    Add a route update record. These run to modify a request.
        Update rule var value
        rule == "cmd|param"
        details == "var value"
    Value can contain pattern and request tokens.
 */
int httpAddRouteUpdate(HttpRoute *route, cchar *rule, cchar *details, int flags)
{
    HttpRouteOp *op;
    char        *value;

    mprAssert(route);
    mprAssert(rule && *rule);

    GRADUATE_LIST(route, updates);
    if ((op = createRouteOp(rule, flags)) == 0) {
        return MPR_ERR_MEMORY;
    }
    if (scasematch(rule, "cmd")) {
        op->details = sclone(details);

    } else if (scasematch(rule, "lang")) {
        /* Nothing to do */;

    } else if (scasematch(rule, "param")) {
        if (!httpTokenize(route, details, "%S %S", &op->var, &value)) {
            return MPR_ERR_BAD_SYNTAX;
        }
        op->value = finalizeReplacement(route, value);
    
    } else {
        return MPR_ERR_BAD_SYNTAX;
    }
    addUniqueItem(route->updates, op);
    return 0;
}


void httpClearRouteStages(HttpRoute *route, int direction)
{
    mprAssert(route);

    if (direction & HTTP_STAGE_RX) {
        route->inputStages = mprCreateList(-1, 0);
    }
    if (direction & HTTP_STAGE_TX) {
        route->outputStages = mprCreateList(-1, 0);
    }
}


void httpDefineRouteTarget(cchar *key, HttpRouteProc *proc)
{
    mprAssert(key && *key);
    mprAssert(proc);

    mprAddKey(((Http*) MPR->httpService)->routeTargets, key, proc);
}


void httpDefineRouteCondition(cchar *key, HttpRouteProc *proc)
{
    mprAssert(key && *key);
    mprAssert(proc);

    mprAddKey(((Http*) MPR->httpService)->routeConditions, key, proc);
}


void httpDefineRouteUpdate(cchar *key, HttpRouteProc *proc)
{
    mprAssert(key && *key);
    mprAssert(proc);

    mprAddKey(((Http*) MPR->httpService)->routeUpdates, key, proc);
}


void *httpGetRouteData(HttpRoute *route, cchar *key)
{
    mprAssert(route);
    mprAssert(key && *key);

    if (!route->data) {
        return 0;
    }
    return mprLookupKey(route->data, key);
}


cchar *httpGetRouteDir(HttpRoute *route)
{
    mprAssert(route);
    return route->dir;
}


cchar *httpGetRouteMethods(HttpRoute *route)
{
    mprAssert(route);
    return route->methodSpec;
}


void httpResetRoutePipeline(HttpRoute *route)
{
    mprAssert(route);

    if (route->parent == 0) {
        route->errorDocuments = mprCreateHash(HTTP_SMALL_HASH_SIZE, 0);
        route->caching = 0;
        route->extensions = 0;
        route->handlers = mprCreateList(-1, 0);
        route->handlersWithMatch = mprCreateList(-1, 0);
        route->inputStages = mprCreateList(-1, 0);
        route->indicies = mprCreateList(-1, 0);
    }
    route->outputStages = mprCreateList(-1, 0);
}


void httpResetHandlers(HttpRoute *route)
{
    mprAssert(route);
    route->handlers = mprCreateList(-1, 0);
    route->handlersWithMatch = mprCreateList(-1, 0);
}


void httpSetRouteAuth(HttpRoute *route, HttpAuth *auth)
{
    mprAssert(route);
    route->auth = auth;
}


void httpSetRouteAutoDelete(HttpRoute *route, bool enable)
{
    mprAssert(route);
    route->autoDelete = enable;
}


void httpSetRouteCompression(HttpRoute *route, int flags)
{
    mprAssert(route);
    route->flags &= (HTTP_ROUTE_GZIP);
    route->flags |= (HTTP_ROUTE_GZIP & flags);
}


int httpSetRouteConnector(HttpRoute *route, cchar *name)
{
    HttpStage     *stage;

    mprAssert(route);
    
    stage = httpLookupStage(route->http, name);
    if (stage == 0) {
        mprError("Can't find connector %s", name); 
        return MPR_ERR_CANT_FIND;
    }
    route->connector = stage;
    return 0;
}


void httpSetRouteData(HttpRoute *route, cchar *key, void *data)
{
    mprAssert(route);
    mprAssert(key && *key);
    mprAssert(data);

    if (route->data == 0) {
        route->data = mprCreateHash(-1, 0);
    } else {
        GRADUATE_HASH(route, data);
    }
    mprAddKey(route->data, key, data);
}


void httpSetRouteFlags(HttpRoute *route, int flags)
{
    mprAssert(route);
    route->flags = flags;
}


int httpSetRouteHandler(HttpRoute *route, cchar *name)
{
    HttpStage     *handler;

    mprAssert(route);
    mprAssert(name && *name);
    
    if ((handler = httpLookupStage(route->http, name)) == 0) {
        mprError("Can't find handler %s", name); 
        return MPR_ERR_CANT_FIND;
    }
    route->handler = handler;
    return 0;
}


void httpSetRouteDir(HttpRoute *route, cchar *path)
{
    mprAssert(route);
    mprAssert(path && *path);
    
    route->dir = httpMakePath(route, path);
    httpSetRoutePathVar(route, "DOCUMENT_ROOT", route->dir);
}


/*
    WARNING: internal API only. 
 */
void httpSetRouteHost(HttpRoute *route, HttpHost *host)
{
    mprAssert(route);
    mprAssert(host);
    
    route->host = host;
    defineHostVars(route);
}


void httpAddRouteIndex(HttpRoute *route, cchar *index)
{
    cchar   *item;
    int     next;

    mprAssert(route);
    mprAssert(index && *index);
    
    GRADUATE_LIST(route, indicies);
    for (ITERATE_ITEMS(route->indicies, item, next)) {
        if (smatch(index, item)) {
            return;
        }
    }
    mprAddItem(route->indicies, sclone(index));
}


void httpSetRouteMethods(HttpRoute *route, cchar *methods)
{
    mprAssert(route);
    mprAssert(methods && methods);

    route->methodSpec = sclone(methods);
    finalizeMethods(route);
}


void httpSetRouteName(HttpRoute *route, cchar *name)
{
    mprAssert(route);
    mprAssert(name && *name);
    
    route->name = sclone(name);
}


void httpSetRoutePattern(HttpRoute *route, cchar *pattern, int flags)
{
    mprAssert(route);
    mprAssert(pattern && *pattern);
    
    route->flags |= (flags & HTTP_ROUTE_NOT);
    route->pattern = sclone(pattern);
    finalizePattern(route);
}


void httpSetRoutePrefix(HttpRoute *route, cchar *prefix)
{
    mprAssert(route);
    mprAssert(prefix && *prefix);
    
    route->prefix = sclone(prefix);
    route->prefixLen = slen(prefix);
    if (route->pattern) {
        finalizePattern(route);
    }
}


void httpSetRouteSource(HttpRoute *route, cchar *source)
{
    mprAssert(route);
    mprAssert(source);

    /* Source can be empty */
    route->sourceName = sclone(source);
}


void httpSetRouteScript(HttpRoute *route, cchar *script, cchar *scriptPath)
{
    mprAssert(route);
    
    if (script) {
        mprAssert(*script);
        route->script = sclone(script);
    }
    if (scriptPath) {
        mprAssert(*scriptPath);
        route->scriptPath = sclone(scriptPath);
    }
}


/*
    Target names are extensible and hashed in http->routeTargets. 

        Target close
        Target redirect status [URI]
        Target run ${DOCUMENT_ROOT}/${request:uri}.gz
        Target run ${controller}-${name} 
        Target write [-r] status "Hello World\r\n"
 */
int httpSetRouteTarget(HttpRoute *route, cchar *rule, cchar *details)
{
    char    *redirect, *msg;

    mprAssert(route);
    mprAssert(rule && *rule);

    route->targetRule = sclone(rule);
    route->target = sclone(details);

    if (scasematch(rule, "close")) {
        route->target = sclone(details);

    } else if (scasematch(rule, "redirect")) {
        if (!httpTokenize(route, details, "%N ?S", &route->responseStatus, &redirect)) {
            return MPR_ERR_BAD_SYNTAX;
        }
        route->target = finalizeReplacement(route, redirect);
        return 0;

    } else if (scasematch(rule, "run")) {
        route->target = finalizeReplacement(route, details);

    } else if (scasematch(rule, "write")) {
        /*
            Write [-r] status Message
         */
        if (sncmp(details, "-r", 2) == 0) {
            route->flags |= HTTP_ROUTE_RAW;
            details = &details[2];
        }
        if (!httpTokenize(route, details, "%N %S", &route->responseStatus, &msg)) {
            return MPR_ERR_BAD_SYNTAX;
        }
        route->target = finalizeReplacement(route, msg);

    } else {
        return MPR_ERR_BAD_SYNTAX;
    }
    return 0;
}


void httpSetRouteTemplate(HttpRoute *route, cchar *tplate)
{
    mprAssert(route);
    mprAssert(tplate && *tplate);
    
    route->tplate = sclone(tplate);
}


void httpSetRouteWorkers(HttpRoute *route, int workers)
{
    mprAssert(route);
    route->workers = workers;
}


void httpAddRouteErrorDocument(HttpRoute *route, int status, cchar *url)
{
    char    *code;

    mprAssert(route);
    GRADUATE_HASH(route, errorDocuments);
    code = itos(status);
    mprAddKey(route->errorDocuments, code, sclone(url));
}


cchar *httpLookupRouteErrorDocument(HttpRoute *route, int code)
{
    char   *num;

    mprAssert(route);
    if (route->errorDocuments == 0) {
        return 0;
    }
    num = itos(code);
    return (cchar*) mprLookupKey(route->errorDocuments, num);
}

/********************************* Route Finalization *************************/

static void finalizeMethods(HttpRoute *route)
{
    char    *method, *methods, *tok;

    mprAssert(route);
    methods = route->methodSpec;
    if (methods && *methods && !scasematch(methods, "ALL") && !smatch(methods, "*")) {
        if ((route->methods = mprCreateHash(-1, 0)) == 0) {
            return;
        }
        methods = sclone(methods);
        while ((method = stok(methods, ", \t\n\r", &tok)) != 0) {
            mprAddKey(route->methods, method, route);
            methods = 0;
        }
    } else {
        route->methodSpec = sclone("*");
    }
}


/*
    Finalize the pattern. 
        - Change "\{n[:m]}" to "{n[:m]}"
        - Change "\~" to "~"
        - Change "(~ PAT ~)" to "(?: PAT )?"
        - Extract the tokens and change tokens: "{word}" to "([^/]*)"
 */
static void finalizePattern(HttpRoute *route)
{
    MprBuf      *pattern;
    cchar       *errMsg;
    char        *startPattern, *cp, *ep, *token, *field;
    ssize       len;
    int         column;

    mprAssert(route);
    route->tokens = mprCreateList(-1, 0);
    pattern = mprCreateBuf(-1, -1);
    startPattern = route->pattern[0] == '^' ? &route->pattern[1] : route->pattern;

    if (route->name == 0) {
        route->name = sclone(startPattern);
    }
    if (route->tplate == 0) {
        /* Do this while the prefix is still in the route pattern */
        route->tplate = finalizeTemplate(route);
    }
    /*
        Create an simple literal startWith string to optimize route rejection.
     */
    len = strcspn(startPattern, "^$*+?.(|{[\\");
    if (len) {
        route->startWith = snclone(startPattern, len);
        route->startWithLen = len;
        if ((cp = strchr(&route->startWith[1], '/')) != 0) {
            route->startSegment = snclone(route->startWith, cp - route->startWith);
        } else {
            route->startSegment = route->startWith;
        }
        route->startSegmentLen = slen(route->startSegment);
    } else {
        /* Pattern has special characters */
        route->startWith = 0;
        route->startWithLen = 0;
        route->startSegmentLen = 0;
        route->startSegment = 0;
    }

    /*
        Remove the route prefix from the start of the compiled pattern.
     */
    if (route->prefix && sstarts(startPattern, route->prefix)) {
        mprAssert(route->prefixLen <= route->startWithLen);
        startPattern = sfmt("^%s", &startPattern[route->prefixLen]);
    } else {
        startPattern = sjoin("^", startPattern, NULL);
    }
    for (cp = startPattern; *cp; cp++) {
        /* Alias for optional, non-capturing pattern:  "(?: PAT )?" */
        //  MOB - change ~ is confusing with ~ for top of app
        if (*cp == '(' && cp[1] == '~') {
            mprPutStringToBuf(pattern, "(?:");
            cp++;

        } else if (*cp == '(') {
            mprPutCharToBuf(pattern, *cp);
        } else if (*cp == '~' && cp[1] == ')') {
            mprPutStringToBuf(pattern, ")?");
            cp++;

        } else if (*cp == ')') {
            mprPutCharToBuf(pattern, *cp);

        } else if (*cp == '{') {
            if (cp > route->pattern && cp[-1] == '\\') {
                mprAdjustBufEnd(pattern, -1);
                mprPutCharToBuf(pattern, *cp);
            } else {
                if ((ep = schr(cp, '}')) != 0) {
                    /* Trim {} off the token and replace in pattern with "([^/]*)"  */
                    token = snclone(&cp[1], ep - cp - 1);
                    if ((field = schr(token, '=')) != 0) {
                        *field++ = '\0';
                        field = sfmt("(%s)", field);
                    } else {
                        field = "([^/]*)";
                    }
                    mprPutStringToBuf(pattern, field);
                    mprAddItem(route->tokens, token);
                    /* Params ends up looking like "$1:$2:$3:$4" */
                    cp = ep;
                }
            }
        } else if (*cp == '\\' && *cp == '~') {
            mprPutCharToBuf(pattern, *++cp);

        } else {
            mprPutCharToBuf(pattern, *cp);
        }
    }
    mprAddNullToBuf(pattern);
    route->optimizedPattern = sclone(mprGetBufStart(pattern));
    if (mprGetListLength(route->tokens) == 0) {
        route->tokens = 0;
    }
    if (route->patternCompiled && (route->flags & HTTP_ROUTE_FREE_PATTERN)) {
        free(route->patternCompiled);
    }
    if ((route->patternCompiled = pcre_compile2(route->optimizedPattern, 0, 0, &errMsg, &column, NULL)) == 0) {
        mprError("Can't compile route. Error %s at column %d", errMsg, column); 
    }
    route->flags |= HTTP_ROUTE_FREE_PATTERN;
}


static char *finalizeReplacement(HttpRoute *route, cchar *str)
{
    MprBuf      *buf;
    cchar       *item;
    cchar       *tok, *cp, *ep, *token;
    int         next, braced;

    mprAssert(route);

    /*
        Prepare a replacement string. Change $token to $N
     */
    buf = mprCreateBuf(-1, -1);
    if (str && *str) {
        for (cp = str; *cp; cp++) {
            if ((tok = schr(cp, '$')) != 0 && (tok == str || tok[-1] != '\\')) {
                if (tok > cp) {
                    mprPutBlockToBuf(buf, cp, tok - cp);
                }
                if ((braced = (*++tok == '{')) != 0) {
                    tok++;
                }
                if (*tok == '&' || *tok == '\'' || *tok == '`' || *tok == '$') {
                    mprPutCharToBuf(buf, '$');
                    mprPutCharToBuf(buf, *tok);
                    ep = tok + 1;
                } else {
                    if (braced) {
                        for (ep = tok; *ep && *ep != '}'; ep++) ;
                    } else {
                        for (ep = tok; *ep && isdigit((uchar) *ep); ep++) ;
                    }
                    token = snclone(tok, ep - tok);
                    if (schr(token, ':')) {
                        /* Double quote to get through two levels of expansion in writeTarget */
                        mprPutStringToBuf(buf, "$${");
                        mprPutStringToBuf(buf, token);
                        mprPutCharToBuf(buf, '}');
                    } else {
                        for (next = 0; (item = mprGetNextItem(route->tokens, &next)) != 0; ) {
                            if (scmp(item, token) == 0) {
                                break;
                            }
                        }
                        /*  Insert "$" in front of "{token}" */
                        if (item) {
                            mprPutCharToBuf(buf, '$');
                            mprPutIntToBuf(buf, next);
                        } else if (snumber(token)) {
                            mprPutCharToBuf(buf, '$');
                            mprPutStringToBuf(buf, token);
                        } else {
                            mprError("Can't find token \"%s\" in template \"%s\"", token, route->pattern);
                        }
                    }
                }
                if (braced) {
                    ep++;
                }
                cp = ep - 1;

            } else {
                if (*cp == '\\') {
                    if (cp[1] == 'r') {
                        mprPutCharToBuf(buf, '\r');
                        cp++;
                    } else if (cp[1] == 'n') {
                        mprPutCharToBuf(buf, '\n');
                        cp++;
                    } else {
                        mprPutCharToBuf(buf, *cp);
                    }
                } else {
                    mprPutCharToBuf(buf, *cp);
                }
            }
        }
    }
    mprAddNullToBuf(buf);
    return sclone(mprGetBufStart(buf));
}


/*
    Convert a route pattern into a usable template to construct URI links
    NOTE: this is heuristic and not perfect. Users can define the template via the httpSetTemplate API or in appweb via the
    EspURITemplate configuration directive.
 */
static char *finalizeTemplate(HttpRoute *route)
{
    MprBuf  *buf;
    char    *sp, *tplate;

    if ((buf = mprCreateBuf(0, 0)) == 0) {
        return 0;
    }
    /*
        Note: the route->pattern includes the prefix
     */
    for (sp = route->pattern; *sp; sp++) {
        switch (*sp) {
        default:
            mprPutCharToBuf(buf, *sp);
            break;
        case '$':
            if (sp[1] == '\0') {
                sp++;
            } else {
                mprPutCharToBuf(buf, *sp);
            }
            break;
        case '^':
            if (sp > route->pattern) {
                mprPutCharToBuf(buf, *sp);
            }
            break;
        case '+':
        case '?':
        case '|':
        case '[':
        case ']':
        case '*':
        case '.':
            break;
        case '(':
            if (sp[1] == '~') {
                sp++;
            }
            break;
        case '~':
            if (sp[1] == ')') {
                sp++;
            } else {
                mprPutCharToBuf(buf, *sp);
            }
            break;
        case ')':
            break;
        case '\\':
            if (sp[1] == '\\') {
                mprPutCharToBuf(buf, *sp++);
            } else {
                mprPutCharToBuf(buf, *++sp);
            }
            break;
        case '{':
            mprPutCharToBuf(buf, '$');
            while (sp[1] && *sp != '}') {
                if (*sp == '=') {
                    while (sp[1] && *sp != '}') sp++;
                } else {
                    mprPutCharToBuf(buf, *sp++);
                }
            }
            mprPutCharToBuf(buf, '}');
            break;
        }
    }
    if (mprLookAtLastCharInBuf(buf) == '/') {
        mprAdjustBufEnd(buf, -1);
    }
    mprAddNullToBuf(buf);
    if (mprGetBufLength(buf) > 0) {
        tplate = sclone(mprGetBufStart(buf));
    } else {
        tplate = sclone("/");
    }
    return tplate;
}


void httpFinalizeRoute(HttpRoute *route)
{
    /*
        Add the route to the owning host. When using an Appweb configuration file, the order of route finalization 
        will be from the inside out. This ensures that nested routes are defined BEFORE outer/enclosing routes.
        This is important as requests process routes in-order.
     */
    mprAssert(route);
    if (mprGetListLength(route->indicies) == 0) {
        mprAddItem(route->indicies,  sclone("index.html"));
    }
    httpAddRoute(route->host, route);
#if BIT_FEATURE_SSL
    mprConfigureSsl(route->ssl);
#endif
}


/********************************* Path and URI Expansion *****************************/
/*
    What does this return. Does it return an absolute URI?
    MOB - rename httpUri() and move to uri.c
 */
char *httpLink(HttpConn *conn, cchar *target, MprHash *options)
{
    HttpRoute       *route, *lroute;
    HttpRx          *rx;
    HttpUri         *uri;
    cchar           *routeName, *action, *controller, *originalAction, *tplate;
    char            *rest;

    rx = conn->rx;
    route = rx->route;
    controller = 0;

    if (target == 0) {
        target = "";
    }
    if (*target == '@') {
        target = sjoin("{action: '", target, "'}", NULL);
    } 
    if (*target != '{') {
        target = httpTemplate(conn, target, 0);
    } else  {
        if (options) {
            options = mprBlendHash(httpGetOptions(target), options);
        } else {
            options = httpGetOptions(target);
        }
        /*
            Prep the action. Forms are:
                . @action               # Use the current controller
                . @controller/          # Use "list" as the action
                . @controller/action
         */
        if ((action = httpGetOption(options, "action", 0)) != 0) {
            originalAction = action;
            if (*action == '@') {
                action = &action[1];
            }
            if (strchr(action, '/')) {
                controller = stok((char*) action, "/", (char**) &action);
                action = stok((char*) action, "/", &rest);
            }
            if (controller) {
                httpSetOption(options, "controller", controller);
            } else {
                controller = httpGetParam(conn, "controller", 0);
            }
            if (action == 0 || *action == '\0') {
                action = "list";
            }
            if (action != originalAction) {
                httpSetOption(options, "action", action);
            }
        }
        /*
            Find the template to use. Strategy is this order:
                . options.template
                . options.route.template
                . options.action mapped to a route.template, via:
                . /app/STAR/action
                . /app/controller/action
                . /app/STAR/default
                . /app/controller/default
         */
        if ((tplate = httpGetOption(options, "template", 0)) == 0) {
            if ((routeName = httpGetOption(options, "route", 0)) != 0) {
                routeName = expandRouteName(conn, routeName);
                lroute = httpLookupRoute(conn->host, routeName);
            } else {
                lroute = 0;
            }
            if (lroute == 0) {
                if ((lroute = httpLookupRoute(conn->host, qualifyName(route, "{controller}", action))) == 0) {
                    if ((lroute = httpLookupRoute(conn->host, qualifyName(route, controller, action))) == 0) {
                        if ((lroute = httpLookupRoute(conn->host, qualifyName(route, "{controller}", "default"))) == 0) {
                            lroute = httpLookupRoute(conn->host, qualifyName(route, controller, "default"));
                        }
                    }
                }
            }
            if (lroute) {
                tplate = lroute->tplate;
            }
        }
        if (tplate) {
            target = httpTemplate(conn, tplate, options);
        } else {
            mprError("Can't find template for URI %s", target);
            target = "/";
        }
    }
    //  OPT
    uri = httpCreateUri(target, 0);
    uri = httpResolveUri(httpCreateUri(rx->uri, 0), 1, &uri, 0);
    httpNormalizeUri(uri);
    return httpUriToString(uri, 0);
}


/*
    Limited expansion of route names. Support ~/ and ${app} at the start of the route name
 */
static cchar *expandRouteName(HttpConn *conn, cchar *routeName)
{
    HttpRoute   *route;

    route = conn->rx->route;
    if (routeName[0] == '~') {
        return sjoin(route->prefix, &routeName[1], NULL);
    }
    if (sstarts(routeName, "${app}")) {
        return sjoin(route->prefix, &routeName[6], NULL);
    }
    return routeName;
}


/*
    Expect a route->tplate with embedded tokens of the form: "/${controller}/${action}/${other}"
    The options is a hash of token values.
 */
char *httpTemplate(HttpConn *conn, cchar *tplate, MprHash *options)
{
    MprBuf      *buf;
    HttpRoute   *route;
    cchar       *cp, *ep, *value;
    char        key[MPR_MAX_STRING];

    route = conn->rx->route;
    if (tplate == 0 || *tplate == '\0') {
        return MPR->emptyString;
    }
    buf = mprCreateBuf(-1, -1);
    for (cp = tplate; *cp; cp++) {
        if (*cp == '~' && (cp == tplate || cp[-1] != '\\')) {
            if (route->prefix) {
                mprPutStringToBuf(buf, route->prefix);
            }

        } else if (*cp == '$' && cp[1] == '{' && (cp == tplate || cp[-1] != '\\')) {
            cp += 2;
            if ((ep = strchr(cp, '}')) != 0) {
                sncopy(key, sizeof(key), cp, ep - cp);
                if (options && (value = httpGetOption(options, key, 0)) != 0) {
                    mprPutStringToBuf(buf, value);
                } else if ((value = mprLookupKey(conn->rx->params, key)) != 0) {
                    mprPutStringToBuf(buf, value);
                }
                if (value == 0) {
                    /* Just emit the token name if the token can't be found */
                    mprPutStringToBuf(buf, key);
                }
                cp = ep;
            }
        } else {
            mprPutCharToBuf(buf, *cp);
        }
    }
    mprAddNullToBuf(buf);
    return sclone(mprGetBufStart(buf));
}


//  MOB - rename SetRouteVar
void httpSetRoutePathVar(HttpRoute *route, cchar *key, cchar *value)
{
    mprAssert(route);
    mprAssert(key);
    mprAssert(value);

    GRADUATE_HASH(route, pathTokens);
    if (schr(value, '$')) {
        value = stemplate(value, route->pathTokens);
    }
    mprAddKey(route->pathTokens, key, sclone(value));
}


/*
    Make a path name. This replaces $references, converts to an absolute path name, cleans the path and maps delimiters.
    Paths are resolved relative to host->home (ServerRoot).
 */
char *httpMakePath(HttpRoute *route, cchar *file)
{
    char    *path;

    mprAssert(route);
    mprAssert(file);

    if ((path = stemplate(file, route->pathTokens)) == 0) {
        return 0;
    }
    if (mprIsPathRel(path) && route->host) {
        path = mprJoinPath(route->host->home, path);
    }
    return mprGetAbsPath(path);
}

/********************************* Language ***********************************/
/*
    Language can be an empty string
 */
int httpAddRouteLanguageSuffix(HttpRoute *route, cchar *language, cchar *suffix, int flags)
{
    HttpLang    *lp;

    mprAssert(route);
    mprAssert(language);
    mprAssert(suffix && *suffix);

    if (route->languages == 0) {
        route->languages = mprCreateHash(-1, 0);
    } else {
        GRADUATE_HASH(route, languages);
    }
    if ((lp = mprLookupKey(route->languages, language)) != 0) {
        lp->suffix = sclone(suffix);
        lp->flags = flags;
    } else {
        mprAddKey(route->languages, language, createLangDef(0, suffix, flags));
    }
    return httpAddRouteUpdate(route, "lang", 0, 0);
}


int httpAddRouteLanguageDir(HttpRoute *route, cchar *language, cchar *path)
{
    HttpLang    *lp;

    mprAssert(route);
    mprAssert(language && *language);
    mprAssert(path && *path);

    if (route->languages == 0) {
        route->languages = mprCreateHash(-1, 0);
    } else {
        GRADUATE_HASH(route, languages);
    }
    if ((lp = mprLookupKey(route->languages, language)) != 0) {
        lp->path = sclone(path);
    } else {
        mprAddKey(route->languages, language, createLangDef(path, 0, 0));
    }
    return httpAddRouteUpdate(route, "lang", 0, 0);
}


void httpSetRouteDefaultLanguage(HttpRoute *route, cchar *language)
{
    mprAssert(route);
    mprAssert(language && *language);

    route->defaultLanguage = sclone(language);
}


/********************************* Conditions *********************************/

static int testCondition(HttpConn *conn, HttpRoute *route, HttpRouteOp *condition)
{
    HttpRouteProc   *proc;

    mprAssert(conn);
    mprAssert(route);
    mprAssert(condition);

    if ((proc = mprLookupKey(conn->http->routeConditions, condition->name)) == 0) {
        httpError(conn, -1, "Can't find route condition rule %s", condition->name);
        return 0;
    }
    mprLog(6, "run condition on route %s condition %s", route->name, condition->name);
    return (*proc)(conn, route, condition);
}


/*
    Allow/Deny authorization
 */
static int allowDenyCondition(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpRx      *rx;
    HttpAuth    *auth;
    int         allow, deny;

    mprAssert(conn);
    mprAssert(route);

    rx = conn->rx;
    auth = rx->route->auth;
    if (auth == 0) {
        return HTTP_ROUTE_OK;
    }
    allow = 0;
    deny = 0;
    if (auth->order == HTTP_ALLOW_DENY) {
        if (auth->allow && mprLookupKey(auth->allow, conn->ip)) {
            allow++;
        } else {
            allow++;
        }
        if (auth->deny && mprLookupKey(auth->deny, conn->ip)) {
            deny++;
        }
        if (!allow || deny) {
            httpError(conn, HTTP_CODE_UNAUTHORIZED, "Access denied for this server %s", conn->ip);
            return HTTP_ROUTE_OK;
        }
    } else {
        if (auth->deny && mprLookupKey(auth->deny, conn->ip)) {
            deny++;
        }
        if (auth->allow && !mprLookupKey(auth->allow, conn->ip)) {
            deny = 0;
            allow++;
        } else {
            allow++;
        }
        if (deny || !allow) {
            httpError(conn, HTTP_CODE_UNAUTHORIZED, "Access denied for this server %s", conn->ip);
            return HTTP_ROUTE_OK;
        }
    }
    return HTTP_ROUTE_OK;
}


static int authCondition(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    mprAssert(conn);
    mprAssert(route);

    if (route->auth == 0 || route->auth->type == 0) {
        return HTTP_ROUTE_OK;
    }
    return httpCheckAuth(conn);
}


static int directoryCondition(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpTx      *tx;
    MprPath     info;
    char        *path;

    mprAssert(conn);
    mprAssert(route);
    mprAssert(op);

    /* 
        Must have tx->filename set when expanding op->details, so map target now 
     */
    tx = conn->tx;
    httpMapFile(conn, route);
    path = mprJoinPath(route->dir, expandTokens(conn, op->details));
    tx->ext = tx->filename = 0;
    mprGetPathInfo(path, &info);
    if (info.isDir) {
        return HTTP_ROUTE_OK;
    }
    return HTTP_ROUTE_REJECT;
}


/*
    Test if a file exists
 */
static int existsCondition(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpTx  *tx;
    char    *path;

    mprAssert(conn);
    mprAssert(route);
    mprAssert(op);

    tx = conn->tx;

    /* 
        Must have tx->filename set when expanding op->details, so map target now 
     */
    httpMapFile(conn, route);
    path = mprJoinPath(route->dir, expandTokens(conn, op->details));
    tx->ext = tx->filename = 0;
    if (mprPathExists(path, R_OK)) {
        return HTTP_ROUTE_OK;
    }
    return 0;
}


static int matchCondition(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    char    *str;
    int     matched[HTTP_MAX_ROUTE_MATCHES * 2], count;

    mprAssert(conn);
    mprAssert(route);
    mprAssert(op);

    str = expandTokens(conn, op->details);
    count = pcre_exec(op->mdata, NULL, str, (int) slen(str), 0, 0, matched, sizeof(matched) / sizeof(int)); 
    if (count > 0) {
        return HTTP_ROUTE_OK;
    }
    return HTTP_ROUTE_REJECT;
}


/********************************* Updates ******************************/

static int updateRequest(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpRouteProc   *proc;

    mprAssert(conn);
    mprAssert(route);
    mprAssert(op);

    if ((proc = mprLookupKey(conn->http->routeUpdates, op->name)) == 0) {
        httpError(conn, -1, "Can't find route update rule %s", op->name);
        return HTTP_ROUTE_OK;
    }
    mprLog(6, "run update on route %s update %s", route->name, op->name);
    return (*proc)(conn, route, op);
}


static int cmdUpdate(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    MprCmd  *cmd;
    char    *command, *out, *err;
    int     status;

    mprAssert(conn);
    mprAssert(route);
    mprAssert(op);

    command = expandTokens(conn, op->details);
    cmd = mprCreateCmd(conn->dispatcher);
    if ((status = mprRunCmd(cmd, command, NULL, &out, &err, -1, 0)) != 0) {
        /* Don't call httpError, just set errorMsg which can be retrieved via: ${request:error} */
        conn->errorMsg = sfmt("Command failed: %s\nStatus: %d\n%s\n%s", command, status, out, err);
        mprError("%s", conn->errorMsg);
        /* Continue */
    }
    return HTTP_ROUTE_OK;
}


static int paramUpdate(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    mprAssert(conn);
    mprAssert(route);
    mprAssert(op);

    httpSetParam(conn, op->var, expandTokens(conn, op->value));
    return HTTP_ROUTE_OK;
}


static int langUpdate(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpUri     *prior;
    HttpRx      *rx;
    HttpLang    *lang;
    char        *ext, *pathInfo, *uri;

    mprAssert(conn);
    mprAssert(route);

    rx = conn->rx;
    prior = rx->parsedUri;
    mprAssert(route->languages);

    if ((lang = httpGetLanguage(conn, route->languages, 0)) != 0) {
        rx->lang = lang;
        if (lang->suffix) {
            pathInfo = 0;
            if (lang->flags & HTTP_LANG_AFTER) {
                pathInfo = sjoin(rx->pathInfo, ".", lang->suffix, NULL);
            } else if (lang->flags & HTTP_LANG_BEFORE) {
                ext = httpGetExt(conn);
                if (ext && *ext) {
                    pathInfo = sjoin(mprJoinPathExt(mprTrimPathExt(rx->pathInfo), lang->suffix), ".", ext, NULL);
                } else {
                    pathInfo = mprJoinPathExt(mprTrimPathExt(rx->pathInfo), lang->suffix);
                }
            }
            if (pathInfo) {
                uri = httpFormatUri(prior->scheme, prior->host, prior->port, pathInfo, prior->reference, prior->query, 0);
                httpSetUri(conn, uri, 0);
            }
        }
    }
    return HTTP_ROUTE_OK;
}


/*********************************** Targets **********************************/

static int closeTarget(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    mprAssert(conn);
    mprAssert(route);

    httpError(conn, HTTP_CODE_RESET, "Route target \"close\" is closing request");
    httpDisconnect(conn);
    return HTTP_ROUTE_OK;
}


static int redirectTarget(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpRx      *rx;
    HttpUri     *dest, *prior;
    cchar       *scheme, *host, *query, *reference, *uri, *target;
    int         port;

    mprAssert(conn);
    mprAssert(route);
    mprAssert(route->target);

    rx = conn->rx;

    /* Note: target may be empty */
    target = expandTokens(conn, route->target);
    if (route->responseStatus) {
        httpRedirect(conn, route->responseStatus, target);
        return HTTP_ROUTE_OK;
    }
    prior = rx->parsedUri;
    dest = httpCreateUri(route->target, 0);
    scheme = dest->scheme ? dest->scheme : prior->scheme;
    host = dest->host ? dest->host : prior->host;
    port = dest->port ? dest->port : prior->port;
    query = dest->query ? dest->query : prior->query;
    reference = dest->reference ? dest->reference : prior->reference;
    uri = httpFormatUri(scheme, host, port, target, reference, query, 0);
    httpSetUri(conn, uri, 0);
    return HTTP_ROUTE_REROUTE;
}


static int runTarget(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    /*
        Need to re-compute output string as updates may have run to define params which affect the route->target tokens
     */
    conn->rx->target = route->target ? expandTokens(conn, route->target) : sclone(&conn->rx->pathInfo[1]);
    return HTTP_ROUTE_OK;
}


static int writeTarget(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    char    *str;

    mprAssert(conn);
    mprAssert(route);

    /*
        Need to re-compute output string as updates may have run to define params which affect the route->target tokens
     */
    str = route->target ? expandTokens(conn, route->target) : sclone(&conn->rx->pathInfo[1]);
    if (!(route->flags & HTTP_ROUTE_RAW)) {
        str = mprEscapeHtml(str);
    }
    httpSetStatus(conn, route->responseStatus);
    httpFormatResponse(conn, "%s", str);
    httpFinalize(conn);
    return HTTP_ROUTE_OK;
}


/************************************************** Route Convenience ****************************************************/

HttpRoute *httpDefineRoute(HttpRoute *parent, cchar *name, cchar *methods, cchar *pattern, cchar *target, 
        cchar *source)
{
    HttpRoute   *route;

    if ((route = httpCreateInheritedRoute(parent)) == 0) {
        return 0;
    }
    httpSetRouteName(route, name);
    httpSetRoutePattern(route, pattern, 0);
    if (methods) {
        httpSetRouteMethods(route, methods);
    }
    if (source) {
        httpSetRouteSource(route, source);
    }
    httpSetRouteTarget(route, "run", target);
    httpFinalizeRoute(route);
    return route;
}


/*
    Calculate a qualified route name. The form is: /{app}/{controller}/action
 */
static char *qualifyName(HttpRoute *route, cchar *controller, cchar *action)
{
    cchar   *prefix, *controllerPrefix;

    prefix = route->prefix ? route->prefix : "";
    if (action == 0 || *action == '\0') {
        action = "default";
    }
    if (controller) {
        controllerPrefix = (controller && smatch(controller, "{controller}")) ? "*" : controller;
        return sjoin(prefix, "/", controllerPrefix, "/", action, NULL);
    } else {
        return sjoin(prefix, "/", action, NULL);
    }
}


/*
    Add a restful route. The parent route may supply a route prefix. If defined, the route name will prepend the prefix.
 */
static void addRestful(HttpRoute *parent, cchar *action, cchar *methods, cchar *pattern, cchar *target, cchar *resource)
{
    cchar   *name, *nameResource, *source;

    nameResource = smatch(resource, "{controller}") ? "*" : resource;
    if (parent->prefix) {
        name = sfmt("%s/%s/%s", parent->prefix, nameResource, action);
        pattern = sfmt("^%s/%s%s", parent->prefix, resource, pattern);
    } else {
        name = sfmt("/%s/%s", nameResource, action);
        pattern = sfmt("^/%s%s", resource, pattern);
    }
    if (*resource == '{') {
        target = sfmt("$%s-%s", resource, target);
        source = sfmt("$%s.c", resource);
    } else {
        target = sfmt("%s-%s", resource, target);
        source = sfmt("%s.c", resource);
    }
    httpDefineRoute(parent, name, methods, pattern, target, source);
}


/*
    httpAddResourceGroup(parent, "{controller}")
 */
void httpAddResourceGroup(HttpRoute *parent, cchar *resource)
{
    addRestful(parent, "list",      "GET",    "(/)*$",                   "list",          resource);
    addRestful(parent, "init",      "GET",    "/init$",                  "init",          resource);
    addRestful(parent, "create",    "POST",   "(/)*$",                   "create",        resource);
    addRestful(parent, "edit",      "GET",    "/{id=[0-9]+}/edit$",      "edit",          resource);
    addRestful(parent, "show",      "GET",    "/{id=[0-9]+}$",           "show",          resource);
    addRestful(parent, "update",    "PUT",    "/{id=[0-9]+}$",           "update",        resource);
    addRestful(parent, "destroy",   "DELETE", "/{id=[0-9]+}$",           "destroy",       resource);
    //  MOB - rethink "custom" name - should action be after id? Should this be custom-${action}?
    addRestful(parent, "custom",    "POST",   "/{action}/{id=[0-9]+}$",  "${action}",     resource);
    addRestful(parent, "default",   "*",      "/{action}$",              "cmd-${action}", resource);
}


/*
    httpAddResource(parent, "{controller}")
 */
void httpAddResource(HttpRoute *parent, cchar *resource)
{
    addRestful(parent, "init",      "GET",    "/init$",       "init",          resource);
    addRestful(parent, "create",    "POST",   "(/)*$",        "create",        resource);
    addRestful(parent, "edit",      "GET",    "/edit$",       "edit",          resource);
    addRestful(parent, "show",      "GET",    "$",            "show",          resource);
    addRestful(parent, "update",    "PUT",    "$",            "update",        resource);
    addRestful(parent, "destroy",   "DELETE", "$",            "destroy",       resource);
    addRestful(parent, "default",   "*",      "/{action}$",   "cmd-${action}", resource);
}


void httpAddStaticRoute(HttpRoute *parent)
{
    cchar   *source, *name, *path, *pattern, *prefix;

    prefix = parent->prefix ? parent->prefix : "";
    source = parent->sourceName;
    name = qualifyName(parent, NULL, "home");
    path = stemplate("${STATIC_DIR}/index.esp", parent->pathTokens);
    pattern = sfmt("^%s%s", prefix, "(/)*$");
    httpDefineRoute(parent, name, "GET,POST,PUT", pattern, path, source);
}


void httpAddHomeRoute(HttpRoute *parent)
{
    cchar   *source, *name, *path, *pattern, *prefix;

    prefix = parent->prefix ? parent->prefix : "";
    source = parent->sourceName;

    name = qualifyName(parent, NULL, "static");
    path = stemplate("${STATIC_DIR}/$1", parent->pathTokens);
    pattern = sfmt("^%s%s", prefix, "/static/(.*)");
    httpDefineRoute(parent, name, "GET", pattern, path, source);
}


void httpAddRouteSet(HttpRoute *parent, cchar *set)
{
    if (scasematch(set, "simple")) {
        httpAddHomeRoute(parent);

    } else if (scasematch(set, "mvc")) {
        httpAddHomeRoute(parent);
        httpAddStaticRoute(parent);
        httpDefineRoute(parent, "default", NULL, "^/{controller}(~/{action}~)", "${controller}-${action}", 
            "${controller}.c");

    } else if (scasematch(set, "restful")) {
        httpAddHomeRoute(parent);
        httpAddStaticRoute(parent);
        httpAddResourceGroup(parent, "{controller}");

    } else if (!scasematch(set, "none")) {
        mprError("Unknown route set %s", set);
    }
}


/*************************************************** Support Routines ****************************************************/
/*
    Route operations are used per-route for headers and fields
 */
static HttpRouteOp *createRouteOp(cchar *name, int flags)
{
    HttpRouteOp   *op;

    mprAssert(name && *name);

    if ((op = mprAllocObj(HttpRouteOp, manageRouteOp)) == 0) {
        return 0;
    }
    op->name = sclone(name);
    op->flags = flags;
    return op;
}


static void manageRouteOp(HttpRouteOp *op, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(op->name);
        mprMark(op->details);
        mprMark(op->var);
        mprMark(op->value);

    } else if (flags & MPR_MANAGE_FREE) {
        if (op->flags & HTTP_ROUTE_FREE) {
            free(op->mdata);
            op->mdata = 0;
        }
    }
}


static bool opPresent(MprList *list, HttpRouteOp *op)
{
    HttpRouteOp   *last;

    if ((last = mprGetLastItem(list)) == 0) {
        return 0;
    }
    if (smatch(last->name, op->name) && 
        smatch(last->details, op->details) && 
        smatch(last->var, op->var) && 
        smatch(last->value, op->value) && 
        last->mdata == op->mdata && 
        last->flags == op->flags) {
        return 1;
    }
    return 0;
}


static void addUniqueItem(MprList *list, HttpRouteOp *op)
{
    mprAssert(list);
    mprAssert(op);

    if (!opPresent(list, op)) {
        mprAddItem(list, op);
    }
}


static HttpLang *createLangDef(cchar *path, cchar *suffix, int flags)
{
    HttpLang    *lang;

    if ((lang = mprAllocObj(HttpLang, manageLang)) == 0) {
        return 0;
    }
    if (path) {
        lang->path = sclone(path);
    }
    if (suffix) {
        lang->suffix = sclone(suffix);
    }
    lang->flags = flags;
    return lang;
}


static void manageLang(HttpLang *lang, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(lang->path);
        mprMark(lang->suffix);
    }
}


static void definePathVars(HttpRoute *route)
{
    mprAssert(route);

    mprAddKey(route->pathTokens, "PRODUCT", sclone(BIT_PRODUCT));
    mprAddKey(route->pathTokens, "OS", sclone(BIT_OS));
    mprAddKey(route->pathTokens, "VERSION", sclone(BIT_VERSION));
#if UNUSED
    mprAddKey(route->pathTokens, "LIBDIR", mprJoinPath(mprGetPathParent(mprGetAppDir()), mprGetPathBase(BIT_LIB_NAME)));
#else
    mprAddKey(route->pathTokens, "LIBDIR", mprGetAppDir());
#endif
    if (route->host) {
        defineHostVars(route);
    }
}


static void defineHostVars(HttpRoute *route) 
{
    mprAssert(route);
    mprAddKey(route->pathTokens, "DOCUMENT_ROOT", route->dir);
    mprAddKey(route->pathTokens, "SERVER_ROOT", route->host->home);
}


static char *expandTokens(HttpConn *conn, cchar *str)
{
    HttpRx      *rx;

    mprAssert(conn);
    mprAssert(str);

    rx = conn->rx;
    return expandRequestTokens(conn, expandPatternTokens(rx->pathInfo, str, rx->matches, rx->matchCount));
}


/*
    WARNING: str is modified. Result is allocated string.
 */
static char *expandRequestTokens(HttpConn *conn, char *str)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpRoute   *route;
    MprBuf      *buf;
    HttpLang    *lang;
    char        *tok, *cp, *key, *value, *field, *header, *defaultValue;

    mprAssert(conn);
    mprAssert(str);

    rx = conn->rx;
    route = rx->route;
    tx = conn->tx;
    buf = mprCreateBuf(-1, -1);
    tok = 0;

    for (cp = str; cp && *cp; ) {
        if ((tok = strstr(cp, "${")) == 0) {
            break;
        }
        if (tok > cp) {
            mprPutBlockToBuf(buf, cp, tok - cp);
        }
        if ((key = stok(&tok[2], ":", &value)) == 0) {
            continue;
        }
        stok(value, "}", &cp);

        if (smatch(key, "header")) {
            header = stok(value, "=", &defaultValue);
            if ((value = (char*) httpGetHeader(conn, header)) == 0) {
                value = defaultValue ? defaultValue : "";
            }
            mprPutStringToBuf(buf, value);

        } else if (smatch(key, "param")) {
            field = stok(value, "=", &defaultValue);
            if (defaultValue == 0) {
                defaultValue = "";
            }
            mprPutStringToBuf(buf, httpGetParam(conn, field, defaultValue));

        } else if (smatch(key, "request")) {
            value = stok(value, "=", &defaultValue);
            //  OPT with switch on first char
            if (smatch(value, "clientAddress")) {
                mprPutStringToBuf(buf, conn->ip);

            } else if (smatch(value, "clientPort")) {
                mprPutIntToBuf(buf, conn->port);

            } else if (smatch(value, "error")) {
                mprPutStringToBuf(buf, conn->errorMsg);

            } else if (smatch(value, "ext")) {
                mprPutStringToBuf(buf, rx->parsedUri->ext);

            } else if (smatch(value, "extraPath")) {
                mprPutStringToBuf(buf, rx->extraPath);

            } else if (smatch(value, "filename")) {
                mprPutStringToBuf(buf, tx->filename);

            } else if (scasematch(value, "language")) {
                if (!defaultValue) {
                    defaultValue = route->defaultLanguage;
                }
                if ((lang = httpGetLanguage(conn, route->languages, defaultValue)) != 0) {
                    mprPutStringToBuf(buf, lang->suffix);
                } else {
                    mprPutStringToBuf(buf, defaultValue);
                }

            } else if (scasematch(value, "languageDir")) {
                lang = httpGetLanguage(conn, route->languages, 0);
                if (!defaultValue) {
                    defaultValue = ".";
                }
                mprPutStringToBuf(buf, lang ? lang->path : defaultValue);

            } else if (smatch(value, "host")) {
                mprPutStringToBuf(buf, rx->parsedUri->host);

            } else if (smatch(value, "method")) {
                mprPutStringToBuf(buf, rx->method);

            } else if (smatch(value, "originalUri")) {
                mprPutStringToBuf(buf, rx->originalUri);

            } else if (smatch(value, "pathInfo")) {
                mprPutStringToBuf(buf, rx->pathInfo);

            } else if (smatch(value, "prefix")) {
                mprPutStringToBuf(buf, route->prefix);

            } else if (smatch(value, "query")) {
                mprPutStringToBuf(buf, rx->parsedUri->query);

            } else if (smatch(value, "reference")) {
                mprPutStringToBuf(buf, rx->parsedUri->reference);

            } else if (smatch(value, "scheme")) {
                if (rx->parsedUri->scheme) {
                    mprPutStringToBuf(buf, rx->parsedUri->scheme);
                }  else {
                    mprPutStringToBuf(buf, (conn->secure) ? "https" : "http");
                }

            } else if (smatch(value, "scriptName")) {
                mprPutStringToBuf(buf, rx->scriptName);

            } else if (smatch(value, "serverAddress")) {
                mprPutStringToBuf(buf, conn->sock->acceptIp);

            } else if (smatch(value, "serverPort")) {
                mprPutIntToBuf(buf, conn->sock->acceptPort);

            } else if (smatch(value, "uri")) {
                mprPutStringToBuf(buf, rx->uri);
            }
        }
    }
    if (tok) {
        if (tok > cp) {
            mprPutBlockToBuf(buf, tok, tok - cp);
        }
    } else {
        mprPutStringToBuf(buf, cp);
    }
    mprAddNullToBuf(buf);
    return sclone(mprGetBufStart(buf));
}


/*
    Replace text using pcre regular expression match indicies
 */
static char *expandPatternTokens(cchar *str, cchar *replacement, int *matches, int matchCount)
{
    MprBuf  *result;
    cchar   *end, *cp, *lastReplace;
    int     submatch;

    mprAssert(str);
    mprAssert(replacement);
    mprAssert(matches);

    if (matchCount <= 0) {
        return MPR->emptyString;
    }
    result = mprCreateBuf(-1, -1);
    lastReplace = replacement;
    end = &replacement[slen(replacement)];

    for (cp = replacement; cp < end; ) {
        if (*cp == '$') {
            if (lastReplace < cp) {
                mprPutSubStringToBuf(result, lastReplace, (int) (cp - lastReplace));
            }
            switch (*++cp) {
            case '$':
                mprPutCharToBuf(result, '$');
                break;
            case '&':
                /* Replace the matched string */
                mprPutSubStringToBuf(result, &str[matches[0]], matches[1] - matches[0]);
                break;
            case '`':
                /* Insert the portion that preceeds the matched string */
                mprPutSubStringToBuf(result, str, matches[0]);
                break;
            case '\'':
                /* Insert the portion that follows the matched string */
                mprPutSubStringToBuf(result, &str[matches[1]], slen(str) - matches[1]);
                break;
            default:
                /* Insert the nth submatch */
                if (isdigit((uchar) *cp)) {
                    submatch = (int) wtoi(cp);
                    while (isdigit((uchar) *++cp))
                        ;
                    cp--;
                    if (submatch < matchCount) {
                        submatch *= 2;
                        mprPutSubStringToBuf(result, &str[matches[submatch]], matches[submatch + 1] - matches[submatch]);
                    }
                } else {
                    mprError("Bad replacement $ specification in page");
                    return 0;
                }
            }
            lastReplace = cp + 1;
        }
        cp++;
    }
    if (lastReplace < cp && lastReplace < end) {
        mprPutSubStringToBuf(result, lastReplace, (int) (cp - lastReplace));
    }
    mprAddNullToBuf(result);
    return sclone(mprGetBufStart(result));
}


void httpDefineRouteBuiltins()
{
    /*
        These are the conditions that can be selected. Use httpAddRouteCondition to add to a route.
        The allow and auth conditions are internal and are configured via various Auth APIs.
     */
    httpDefineRouteCondition("allowDeny", allowDenyCondition);
    httpDefineRouteCondition("auth", authCondition);
    httpDefineRouteCondition("match", matchCondition);
    httpDefineRouteCondition("exists", existsCondition);
    httpDefineRouteCondition("directory", directoryCondition);

    httpDefineRouteUpdate("param", paramUpdate);
    httpDefineRouteUpdate("cmd", cmdUpdate);
    httpDefineRouteUpdate("lang", langUpdate);

    httpDefineRouteTarget("close", closeTarget);
    httpDefineRouteTarget("redirect", redirectTarget);
    httpDefineRouteTarget("run", runTarget);
    httpDefineRouteTarget("write", writeTarget);
}


/*
    Tokenizes a line using %formats. Mandatory tokens can be specified with %. Optional tokens are specified with ?. 
    Supported tokens:
        %B - Boolean. Parses: on/off, true/false, yes/no.
        %N - Number. Parses numbers in base 10.
        %S - String. Removes quotes.
        %T - Template String. Removes quotes and expand ${PathVars}.
        %P - Path string. Removes quotes and expands ${PathVars}. Resolved relative to host->dir (ServerRoot).
        %W - Parse words into a list
        %! - Optional negate. Set value to HTTP_ROUTE_NOT present, otherwise zero.
    Values wrapped in quotes will have the outermost quotes trimmed.
 */
bool httpTokenize(HttpRoute *route, cchar *line, cchar *fmt, ...)
{
    va_list     args;
    bool        rc;

    mprAssert(route);
    mprAssert(line);
    mprAssert(fmt);

    va_start(args, fmt);
    rc =  httpTokenizev(route, line, fmt, args);
    va_end(args);
    return rc;
}


bool httpTokenizev(HttpRoute *route, cchar *line, cchar *fmt, va_list args)
{
    MprList     *list;
    cchar       *f;
    char        *tok, *etok, *value, *word, *end;
    int         quote;

    mprAssert(route);
    mprAssert(fmt);

    if (line == 0) {
        line = "";
    }
    tok = sclone(line);
    end = &tok[slen(line)];

    for (f = fmt; *f && tok < end; f++) {
        for (; isspace((uchar) *tok); tok++) ;
        if (*tok == '\0' || *tok == '#') {
            break;
        }
        if (isspace((uchar) *f)) {
            continue;
        }
        if (*f == '%' || *f == '?') {
            f++;
            quote = 0;
            if (*f != '*' && (*tok == '"' || *tok == '\'')) {
                quote = *tok++;
            }
            if (*f == '!') {
                etok = &tok[1];
            } else {
                if (quote) {
                    for (etok = tok; *etok && !(*etok == quote && etok[-1] != '\\'); etok++) ; 
                    *etok++ = '\0';
                } else if (*f == '*') {
                    for (etok = tok; *etok; etok++) {
                        if (*etok == '#') {
                            *etok = '\0';
                        }
                    }
                } else {
                    for (etok = tok; *etok && !isspace((uchar) *etok); etok++) ;
                }
                *etok++ = '\0';
            }
            if (*f == '*') {
                f++;
                tok = trimQuotes(tok);
                * va_arg(args, char**) = tok;
                tok = etok;
                break;
            }

            switch (*f) {
            case '!':
                if (*tok == '!') {
                    *va_arg(args, int*) = HTTP_ROUTE_NOT;
                } else {
                    *va_arg(args, int*) = 0;
                    continue;
                }
                break;
            case 'B':
                if (scasecmp(tok, "on") == 0 || scasecmp(tok, "true") == 0 || scasecmp(tok, "yes") == 0) {
                    *va_arg(args, bool*) = 1;
                } else {
                    *va_arg(args, bool*) = 0;
                }
                break;
            case 'N':
                *va_arg(args, int*) = (int) stoi(tok);
                break;
            case 'P':
                *va_arg(args, char**) = httpMakePath(route, strim(tok, "\"", MPR_TRIM_BOTH));
                break;
            case 'S':
                *va_arg(args, char**) = strim(tok, "\"", MPR_TRIM_BOTH);
                break;
            case 'T':
                value = strim(tok, "\"", MPR_TRIM_BOTH);
                *va_arg(args, char**) = stemplate(value, route->pathTokens);
                break;
            case 'W':
                list = va_arg(args, MprList*);
                word = stok(tok, " \t\r\n", &tok);
                while (word) {
                    mprAddItem(list, word);
                    word = stok(0, " \t\r\n", &tok);
                }
                break;
            default:
                mprError("Unknown token pattern %%\"%c\"", *f);
                break;
            }
            tok = etok;
        }
    }
    if (tok < end) {
        /*
            Extra unparsed text
         */
        for (; tok < end && isspace((uchar) *tok); tok++) ;
        if (*tok) {
            mprError("Extra unparsed text: \"%s\"", tok);
            return 0;
        }
    }
    if (*f) {
        /*
            Extra unparsed format tokens
         */
        for (; *f; f++) {
            if (*f == '%') {
                break;
            } else if (*f == '?') {
                switch (*++f) {
                case '!':
                case 'N':
                    *va_arg(args, int*) = 0;
                    break;
                case 'B':
                    *va_arg(args, bool*) = 0;
                    break;
                case 'D':
                case 'P':
                case 'S':
                case 'T':
                case '*':
                    *va_arg(args, char**) = 0;
                    break;
                case 'W':
                    break;
                default:
                    mprError("Unknown token pattern %%\"%c\"", *f);
                    break;
                }
            }
        }
        if (*f) {
            mprError("Missing directive parameters");
            return 0;
        }
    }
    va_end(args);
    return 1;
}


static char *trimQuotes(char *str)
{
    ssize   len;

    mprAssert(str);
    len = slen(str);
    if (*str == '\"' && str[len - 1] == '\"' && len > 2 && str[1] != '\"') {
        return snclone(&str[1], len - 2);
    }
    return sclone(str);
}


MprHash *httpGetOptions(cchar *options)
{
    if (options == 0) {
        return mprCreateHash(-1, 0);
    }
    if (*options == '@') {
        /* Allow embedded URIs as options */
        options = sfmt("{ data-click: '%s'}", options);
    }
    mprAssert(*options == '{');
    if (*options != '{') {
        options = sfmt("{%s}", options);
    }
    return mprDeserialize(options);
}


cchar *httpGetOption(MprHash *options, cchar *field, cchar *defaultValue)
{
    MprKey      *kp;
    cchar       *value;

    if (options == 0) {
        value = defaultValue;
    } else if ((kp = mprLookupKeyEntry(options, field)) == 0) {
        value = defaultValue;
    } else {
        value = kp->data;
    }
    return value;
}


MprHash *httpGetOptionHash(MprHash *options, cchar *field)
{
    MprKey      *kp;

    if (options == 0) {
        return 0;
    }
    if ((kp = mprLookupKeyEntry(options, field)) == 0) {
        return 0;
    }
    if (kp->type != MPR_JSON_ARRAY && kp->type != MPR_JSON_OBJ) {
        return 0;
    }
    return (MprHash*) kp->data;
}


void httpAddOption(MprHash *options, cchar *field, cchar *value)
{
    MprKey      *kp;

    if (options == 0) {
        mprAssert(options);
        return;
    }
    if ((kp = mprLookupKeyEntry(options, field)) != 0) {
        kp = mprAddKey(options, field, sjoin(kp->data, " ", value, NULL));
    } else {
        kp = mprAddKey(options, field, value);
    }
    kp->type = MPR_JSON_STRING;
}


void httpRemoveOption(MprHash *options, cchar *field)
{
    if (options == 0) {
        mprAssert(options);
        return;
    }
    mprRemoveKey(options, field);
}


void httpSetOption(MprHash *options, cchar *field, cchar *value)
{
    MprKey  *kp;

    if (value == 0) {
        return;
    }
    if (options == 0) {
        mprAssert(options);
        return;
    }
    if ((kp = mprAddKey(options, field, value)) != 0) {
        kp->type = MPR_JSON_STRING;
    }
}


HttpLimits *httpGraduateLimits(HttpRoute *route, HttpLimits *limits)
{
    if (route->parent && route->limits == route->parent->limits) {
        if (limits == 0) {
            limits = ((Http*) MPR->httpService)->serverLimits;
        }
        route->limits = mprMemdup(limits, sizeof(HttpLimits));
    }
    return route->limits;
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
