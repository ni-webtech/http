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
static char *expandPath(HttpConn *conn, cchar *path);
static char *expandPatternTokens(cchar *str, cchar *replacement, int *matches, int matchCount);
static char *expandRequestTokens(HttpConn *conn, char *targetKey);
static void finalizeMethods(HttpRoute *route);
static void finalizePattern(HttpRoute *route);
static char *finalizeReplacement(HttpRoute *route, cchar *str);
static bool opPresent(MprList *list, HttpRouteOp *op);
static void manageRoute(HttpRoute *route, int flags);
static void manageLang(HttpLang *lang, int flags);
static void manageRouteOp(HttpRouteOp *op, int flags);
static int mapFile(HttpConn *conn, HttpRoute *route);
static int matchRoute(HttpConn *conn, HttpRoute *route);
static int processDirectories(HttpConn *conn, HttpRoute *route);
static int selectHandler(HttpConn *conn, HttpRoute *route);
static int testCondition(HttpConn *conn, HttpRoute *route, HttpRouteOp *condition);
static void trimExtraPath(HttpConn *conn);
static char *trimQuotes(char *str);
static int updateRequest(HttpConn *conn, HttpRoute *route, HttpRouteOp *update);

/************************************ Code ************************************/

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
    route->expires = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_STATIC_VALUES);
    route->expiresByType = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_STATIC_VALUES);
    route->extensions = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS);
    route->flags = HTTP_ROUTE_HANDLER_SMART | HTTP_ROUTE_GZIP;
    route->handlers = mprCreateList(-1, 0);
    route->host = host;
    route->http = MPR->httpService;
    route->index = sclone("index.html");
    route->inputStages = mprCreateList(-1, 0);
    route->outputStages = mprCreateList(-1, 0);
    route->pathVars = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS);
    route->pattern = MPR->emptyString;
    route->targetOp = sclone("file");
    route->workers = -1;
    definePathVars(route);
    return route;
}


HttpRoute *httpCreateDefaultRoute(HttpHost *host)
{
    HttpRoute   *route;

    if ((route = httpCreateRoute(host)) == 0) {
        return 0;
    }
    httpSetRouteName(route, "default");
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
    route->closeTarget = parent->closeTarget;
    route->conditions = parent->conditions;
    route->connector = parent->connector;
    route->defaultLanguage = parent->defaultLanguage;
    route->dir = parent->dir;
    route->data = parent->data;
    route->errorDocuments = parent->errorDocuments;
    route->expires = parent->expires;
    route->expiresByType = parent->expiresByType;
    route->extensions = parent->extensions;
    route->fileTarget = parent->fileTarget;
    route->flags = parent->flags;
    route->handler = parent->handler;
    route->handlers = parent->handlers;
    route->headers = parent->headers;
    route->http = MPR->httpService;
    route->host = parent->host;
    route->inputStages = parent->inputStages;
    route->index = parent->index;
    route->languages = parent->languages;
    route->targetOp = parent->targetOp;
    route->methodHash = parent->methodHash;
    route->methods = parent->methods;
    route->outputStages = parent->outputStages;
    route->params = parent->params;
    route->parent = parent;
    route->pathVars = parent->pathVars;
    route->pattern = parent->pattern;
    route->patternCompiled = parent->patternCompiled;
    route->processedPattern = parent->processedPattern;
    route->queryFields = parent->queryFields;
    route->redirectTarget = parent->redirectTarget;
    route->responseStatus = parent->responseStatus;
    route->script = parent->script;
    route->scriptName = parent->scriptName;
    route->scriptNameLen = parent->scriptNameLen;
    route->scriptPath = parent->scriptPath;
#if UNUSED
    route->searchPath = parent->searchPath;
#endif
    route->sourceName = parent->sourceName;
    route->sourcePath = parent->sourcePath;
    route->ssl = parent->ssl;
    route->target = parent->target;
    route->template = parent->template;
    route->tokens = parent->tokens;
    route->updates = parent->updates;
    route->uploadDir = parent->uploadDir;
    route->virtualTarget = parent->virtualTarget;
    route->writeTarget = parent->writeTarget;
    route->workers = parent->workers;
    return route;
}


HttpRoute *httpCreateAliasRoute(HttpRoute *parent, cchar *prefix, cchar *path, int status)
{
    HttpRoute   *route;

    if ((route = httpCreateInheritedRoute(parent)) == 0) {
        return 0;
    }
    httpSetRoutePattern(route, prefix, 0);
    httpSetRouteDir(route, path);
    route->responseStatus = status;
    return route;
}


static void manageRoute(HttpRoute *route, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(route->auth);
        mprMark(route->closeTarget);
        mprMark(route->conditions);
        mprMark(route->connector);
        mprMark(route->context);
        mprMark(route->data);
        mprMark(route->defaultLanguage);
        mprMark(route->dir);
        mprMark(route->errorDocuments);
        mprMark(route->expires);
        mprMark(route->expiresByType);
        mprMark(route->extensions);
        mprMark(route->fileTarget);
        mprMark(route->handler);
        mprMark(route->handlers);
        mprMark(route->headers);
        mprMark(route->http);
        mprMark(route->index);
        mprMark(route->inputStages);
        mprMark(route->languages);
        mprMark(route->methodHash);
        mprMark(route->methods);
        mprMark(route->name);
        mprMark(route->outputStages);
        mprMark(route->params);
        mprMark(route->parent);
        mprMark(route->pathVars);
        mprMark(route->pattern);
        mprMark(route->prefix);
        mprMark(route->processedPattern);
        mprMark(route->queryFields);
        mprMark(route->redirectTarget);
        mprMark(route->script);
        mprMark(route->scriptName);
        mprMark(route->scriptPath);
#if UNUSED
        mprMark(route->searchPath);
#endif
        mprMark(route->sourceName);
        mprMark(route->sourcePath);
        mprMark(route->ssl);
        mprMark(route->target);
        mprMark(route->targetOp);
        mprMark(route->redirectTarget);
        mprMark(route->tokens);
        mprMark(route->updates);
        mprMark(route->uploadDir);
        mprMark(route->virtualTarget);
        mprMark(route->writeTarget);

    } else if (flags & MPR_MANAGE_FREE) {
        if (route->patternCompiled) {
            free(route->patternCompiled);
        }
    }
}


int httpMatchRoute(HttpConn *conn, HttpRoute *route)
{
    HttpRx      *rx;
    char        *savePathInfo, *pathInfo;
    ssize       len;
    int         rc;

    rx = conn->rx;
    savePathInfo = rx->pathInfo;

    if (route->scriptName) {
        mprAssert(rx->pathInfo[0] == '/');
        len = route->scriptNameLen;
        if (strncmp(&rx->pathInfo[1], route->scriptName, len) != 0) {
            return 0;
        }
        len++;
        if (rx->pathInfo[len] && rx->pathInfo[len] != '/') {
            return 0;
        }

        pathInfo = &rx->pathInfo[1 + slen(route->scriptName)];
        if (*pathInfo == '\0') {
            pathInfo = "/";
        }
        rx->pathInfo = sclone(pathInfo);
        rx->scriptName = route->scriptName;
        mprLog(5, "Route for script name: \"%s\", pathInfo: \"%s\"", rx->scriptName, rx->pathInfo);
    }
    if ((rc = matchRoute(conn, route)) != HTTP_ROUTE_ACCEPTED) {
        rx->pathInfo = savePathInfo;
        rx->scriptName = 0;
    }
    return rc;
}


static int matchRoute(HttpConn *conn, HttpRoute *route)
{
    HttpRouteOp     *op, *condition, *update;
    HttpRouteProc   *proc;
    HttpRx          *rx;
    HttpTx          *tx;
    cchar           *token, *value, *header, *field;
    int             next, rc, matched[HTTP_MAX_ROUTE_MATCHES * 2], count;

    rx = conn->rx;
    tx = conn->tx;
    route->flags &= ~HTTP_ROUTE_MAPPED;

    if (route->patternCompiled) {
        if (route->prefix && strncmp(rx->pathInfo, route->prefix, route->prefixLen) != 0) {
            return 0;
        }
        rx->matchCount = pcre_exec(route->patternCompiled, NULL, rx->pathInfo, (int) slen(rx->pathInfo), 0, 0, 
                rx->matches, sizeof(rx->matches) / sizeof(int));
        mprLog(6, "Test route pattern \"%s\", regexp %s", route->name, route->processedPattern);
        if (route->flags & HTTP_ROUTE_NOT) {
            if (rx->matchCount > 0) {
                return 0;
            }
            rx->matchCount = 1;
            rx->matches[0] = 0;
            rx->matches[1] = (int) slen(rx->pathInfo);
        } else {
            if (rx->matchCount <= 0) {
                return 0;
            }
        }
    }
    rx->route = route;

    mprLog(6, "Test route methods \"%s\"", route->name);
    if (route->methodHash && !mprLookupKey(route->methodHash, rx->method)) {
        return 0;
    }
    if (route->headers) {
        for (next = 0; (op = mprGetNextItem(route->headers, &next)) != 0; ) {
            mprLog(6, "Test route \"%s\" header \"%s\"", route->name, op->name);
            if ((header = httpGetHeader(conn, op->name)) != 0) {
                count = pcre_exec(op->mdata, NULL, header, (int) slen(header), 0, 0, 
                    matched, sizeof(matched) / sizeof(int)); 
                rc = count > 0;
                if (op->flags & HTTP_ROUTE_NOT) {
                    rc = !rc;
                }
                if (!rc) {
                    return 0;
                }
            }
        }
    }
    if (route->queryFields) {
        httpAddQueryVars(conn);
        for (next = 0; (op = mprGetNextItem(route->queryFields, &next)) != 0; ) {
            mprLog(6, "Test route \"%s\" field \"%s\"", route->name, op->name);
            if ((field = httpGetFormVar(conn, op->name, "")) != 0) {
                count = pcre_exec(op->mdata, NULL, field, (int) slen(field), 0, 0, 
                    matched, sizeof(matched) / sizeof(int)); 
                rc = count > 0;
                if (op->flags & HTTP_ROUTE_NOT) {
                    rc = !rc;
                }
                if (!rc) {
                    return 0;
                }
            }
        }
    }
    if (!selectHandler(conn, route)) {
        return 0;
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
            if (!rc) {
                return 0;
            }
        }
    }
    //  Point of no return

    if (route->updates) {
        for (next = 0; (update = mprGetNextItem(route->updates, &next)) != 0; ) {
            mprLog(6, "Run route \"%s\" update \"%s\"", route->name, update->name);
            if ((rc = updateRequest(conn, route, update)) == HTTP_ROUTE_REROUTE) {
                return rc;
            }
        }
    }
    if (route->params) {
        for (next = 0; (token = mprGetNextItem(route->tokens, &next)) != 0; ) {
            value = snclone(&rx->pathInfo[rx->matches[next * 2]], rx->matches[(next * 2) + 1] - rx->matches[(next * 2)]);
            httpSetFormVar(conn, token, value);
        }
    }
    if ((proc = mprLookupKey(conn->http->routeTargets, route->targetOp)) == 0) {
        httpError(conn, -1, "Can't find route target name \"%s\"", route->targetOp);
        return 0;
    }
    mprLog(0, "Run route target \"%s\" target \"%s\"", route->name, route->targetOp);
    return (*proc)(conn, route, 0);
}


static int selectHandler(HttpConn *conn, HttpRoute *route)
{
    HttpTx      *tx;

    tx = conn->tx;
    if (route->handler) {
        tx->handler = route->handler;
    } else {
        if (!tx->ext || (tx->handler = mprLookupKey(route->extensions, tx->ext)) == 0) {
            tx->handler = mprLookupKey(route->extensions, "");
        }
        if (tx->handler && tx->handler->match) {
            if (!tx->handler->match(conn, tx->handler, 0)) {
                tx->handler = conn->http->passHandler;
            }
        }
    }
    return tx->handler ? HTTP_ROUTE_ACCEPTED : 0;
}


/*
    Map the fileTarget to physical storage. Sets tx->filename, tx->ext, tx->etag.
    Also handles GZIP alternative content, trims extra path and language roots.
 */
static int mapFile(HttpConn *conn, HttpRoute *route)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpHost    *host;
    HttpLang    *lang;
    MprPath     *info, ginfo;
    cchar       *ext;
    char        *gfile, *filename;
    int         rc;

    rx = conn->rx;
    tx = conn->tx;
    host = conn->host;
    mprAssert(tx->handler);

    if (route->flags & HTTP_ROUTE_MAPPED) {
        return HTTP_ROUTE_ACCEPTED;
    }
    route->flags |= HTTP_ROUTE_MAPPED;

    filename = route->fileTarget ? expandPath(conn, route->fileTarget) : rx->pathInfo;
    if (filename && filename[0] == '/') {
        filename = sclone(&filename[1]);
    }
    if (route->languages) {
        if ((lang = httpGetLanguage(conn, route->languages)) != 0) {
            if (lang->path) {
                tx->filename = mprJoinPath(lang->path, filename);
            } else if (lang->suffix) {
                if (lang->flags & HTTP_LANG_AFTER) {
                    filename = sjoin(filename, ".", lang->suffix, 0);
                } else if (lang->flags & HTTP_LANG_BEFORE) {
                    ext = mprGetPathExt(filename);
                    if (ext && *ext) {
                        filename = sjoin(mprJoinPathExt(mprTrimPathExt(filename), lang->suffix), ".", ext, 0);
                    } else {
                        filename = mprJoinPathExt(mprTrimPathExt(filename), lang->suffix);
                    }
                }
            }
        }
    }
    tx->filename = mprJoinPath(route->dir, filename);
    tx->ext = httpGetExt(conn);
    info = &tx->fileInfo;
    mprGetPathInfo(tx->filename, info);

    if (info->isDir && (rc = processDirectories(conn, route)) != 0) {
        return rc;
    }
    if (route->flags & HTTP_ROUTE_GZIP) {
        if (!info->valid && rx->acceptEncoding && strstr(rx->acceptEncoding, "gzip") != 0) {
            gfile = sfmt("%s.gz", tx->filename);
            if (mprGetPathInfo(gfile, &ginfo) == 0) {
                tx->filename = gfile;
                tx->fileInfo = ginfo;
                httpSetHeader(conn, "Content-Encoding", "gzip");
            }
        }
    }
//  MOB - where should this be placed?
    if (tx->handler->flags & HTTP_STAGE_EXTRA_PATH) {
        trimExtraPath(conn);
    }
    if (info->valid && !info->isDir) {
        tx->etag = sfmt("\"%x-%Lx-%Lx\"", info->inode, info->size, info->mtime);
    }
    mprLog(5, "mapFile uri \"%s\", filename: \"%s\", extension: \"%s\"", rx->uri, tx->filename, tx->ext);
    return HTTP_ROUTE_ACCEPTED;
}


/*
    Manage file requests that resolve to directories. This will either do an external redirect back to the browser 
    or do an internal (transparent) redirection and serve different content back to the browser.
 */
static int processDirectories(HttpConn *conn, HttpRoute *route)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpUri     *prior;
    char        *path, *pathInfo, *uri;

    tx = conn->tx;
    rx = conn->rx;
    prior = rx->parsedUri;

    mprAssert(route == rx->route);
    mprAssert(route->index);
    mprAssert(rx->pathInfo);
    mprAssert(tx->fileInfo.checked);

    if (sends(rx->pathInfo, "/")) {
        /*  
            Internal directory redirections. Append index transparently to the client.
         */
        if (route->index && *route->index) {
            path = mprJoinPath(tx->filename, route->index);
            if (mprPathExists(path, R_OK)) {
                /*  
                    Index file exists, so do an internal redirect to it. Client will not be aware of this happening.
                 */
                pathInfo = mprJoinPath(rx->pathInfo, route->index);
                uri = httpFormatUri(prior->scheme, prior->host, prior->port, pathInfo, prior->reference, prior->query, 0);
                httpSetUri(conn, uri, 0);
                return HTTP_ROUTE_REROUTE;
            }
        }
    } else {
        /* Must not append the index for the external redirect */
        pathInfo = sjoin(rx->pathInfo, "/", NULL);
        uri = httpFormatUri(prior->scheme, prior->host, prior->port, pathInfo, prior->reference, prior->query, 0);
        httpRedirect(conn, HTTP_CODE_MOVED_PERMANENTLY, uri);
        return HTTP_ROUTE_ACCEPTED;
    }
    return 0;
}


/*
    Trim extra path information after the uri/filename. This is used by CGI (only).
    Strategy is to heuristically find the script name in the uri. This is assumed to be either:
        - the original uri up to and including first path component containing a ".", or
        - the entire original uri
    Once found, set the scriptName and trim the extraPath from pathInfo.
    WARNING: ExtraPath is an old, unreliable, CGI specific technique. Directories with "." will thwart this code.
 */
static void trimExtraPath(HttpConn *conn)
{
    HttpRx      *rx;
    cchar       *seps;
    char        *cp, *extra, *start;
    ssize       len;

    rx = conn->rx;

    //  MOB - this is not right. Circular logic. How can filename be right if it has extra path?
    start = conn->tx->filename;
    seps = mprGetPathSeparators(start);
    if ((cp = strchr(start, '.')) != 0 && (extra = strchr(cp, seps[0])) != 0) {
        len = extra - start;
        if (0 < len && len < slen(rx->pathInfo)) {
            *extra = '\0';
            rx->extraPath = sclone(&rx->pathInfo[len]);
            rx->pathInfo[len] = '\0';
            return;
        }
    }
}

/************************************ API *************************************/

int httpAddRouteCondition(HttpRoute *route, cchar *condition, int flags)
{
    HttpRouteOp *op;
    cchar       *errMsg;
    char        *name, *pattern, *details;
    int         column;

    GRADUATE_LIST(route, conditions);
    if (!httpTokenize(route, condition, "%S ?*", &name, &details)) {
        return MPR_ERR_BAD_SYNTAX;
    }
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
            Condition match pattern string 
            String can contain matching ${tokens} from the route->pattern and can contain request ${tokens}
         */
        if (!httpTokenize(route, details, "%S %S", &pattern, &details)) {
            return MPR_ERR_BAD_SYNTAX;
        }
        if ((op->mdata = pcre_compile2(pattern, 0, 0, &errMsg, &column, NULL)) == 0) {
            mprError("Can't compile condition match pattern. Error %s at column %d", errMsg, column); 
            return MPR_ERR_BAD_SYNTAX;
        }
        op->details = finalizeReplacement(route, details);
        op->flags |= HTTP_ROUTE_FREE;
    }
    addUniqueItem(route->conditions, op);
    return 0;
}


void httpAddRouteExpiry(HttpRoute *route, MprTime when, cchar *extensions)
{
    char    *types, *ext, *tok;

    if (extensions && *extensions) {
        GRADUATE_HASH(route, expires);
        types = sclone(extensions);
        ext = stok(types, " ,\t\r\n", &tok);
        while (ext) {
            mprAddKey(route->expires, ext, ITOP(when));
            ext = stok(0, " \t\r\n", &tok);
        }
    }
}


void httpAddRouteExpiryByType(HttpRoute *route, MprTime when, cchar *mimeTypes)
{
    char    *types, *mime, *tok;

    if (mimeTypes && *mimeTypes) {
        GRADUATE_HASH(route, expiresByType);
        types = sclone(mimeTypes);
        mime = stok(types, " ,\t\r\n", &tok);
        while (mime) {
            mprAddKey(route->expiresByType, mime, ITOP(when));
            mime = stok(0, " \t\r\n", &tok);
        }
    }
}


int httpAddRouteFilter(HttpRoute *route, cchar *name, cchar *extensions, int direction)
{
    HttpStage   *stage;
    HttpStage   *filter;
    char        *extlist, *word, *tok;

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
    if (direction & HTTP_STAGE_RX) {
        GRADUATE_LIST(route, inputStages);
        mprAddItem(route->inputStages, filter);
    }
    if (direction & HTTP_STAGE_TX) {
        GRADUATE_LIST(route, outputStages);
        mprAddItem(route->outputStages, filter);
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
    if (extensions && *extensions) {
        mprLog(MPR_CONFIG, "Add handler \"%s\" for extensions: %s", name, extensions);
    } else {
        mprLog(MPR_CONFIG, "Add handler \"%s\" for route: \"%s\"", name, route->pattern);
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
        if (handler->match == 0) {
            /*
                Only match by extensions if no-match routine provided.
             */
            mprAddKey(route->extensions, "", handler);
        }
        if (mprLookupItem(route->handlers, handler) < 0) {
            GRADUATE_LIST(route, handlers);
            mprAddItem(route->handlers, handler);
        }
    }
    return 0;
}


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


void httpAddRouteQuery(HttpRoute *route, cchar *field, cchar *value, int flags)
{
    HttpRouteOp     *op;
    cchar           *errMsg;
    int             column;

    mprAssert(route);
    mprAssert(field && *field);
    mprAssert(value && *value);

    GRADUATE_LIST(route, queryFields);
    if ((op = createRouteOp(field, flags | HTTP_ROUTE_FREE)) == 0) {
        return;
    }
    if ((op->mdata = pcre_compile2(value, 0, 0, &errMsg, &column, NULL)) == 0) {
        mprError("Can't compile field pattern. Error %s at column %d", errMsg, column); 
    } else {
        mprAddItem(route->queryFields, op);
    }
}


/*
    Add a route update record
    Update field var value
    kind == "cmd|field"
    details == "var value"
    Value can contain pattern and request tokens.
 */
int httpAddRouteUpdate(HttpRoute *route, cchar *kind, cchar *details, int flags)
{
    HttpRouteOp *op;
    char        *value;

    GRADUATE_LIST(route, updates);
    if ((op = createRouteOp(kind, flags)) == 0) {
        return MPR_ERR_MEMORY;
    }
    if (scasematch(kind, "cmd")) {
        op->details = sclone(details);

    } else if (scasematch(kind, "field")) {
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
    if (direction & HTTP_STAGE_RX) {
        route->inputStages = mprCreateList(-1, 0);
    }
    if (direction & HTTP_STAGE_TX) {
        route->outputStages = mprCreateList(-1, 0);
    }
}


void httpDefineRouteTarget(cchar *key, HttpRouteProc *proc)
{
    mprAddKey(((Http*) MPR->httpService)->routeTargets, key, proc);
}


void httpDefineRouteCondition(cchar *key, HttpRouteProc *proc)
{
    mprAddKey(((Http*) MPR->httpService)->routeConditions, key, proc);
}


void httpDefineRouteUpdate(cchar *key, HttpRouteProc *proc)
{
    mprAddKey(((Http*) MPR->httpService)->routeUpdates, key, proc);
}


void *httpGetRouteData(HttpRoute *route, cchar *key)
{
    if (!route->data) {
        return 0;
    }
    return mprLookupKey(route->data, key);
}


void httpResetRoutePipeline(HttpRoute *route)
{
    if (route->parent == 0) {
        route->errorDocuments = mprCreateHash(HTTP_SMALL_HASH_SIZE, 0);
        route->expires = mprCreateHash(0, MPR_HASH_STATIC_VALUES);
        route->extensions = mprCreateHash(0, MPR_HASH_CASELESS);
        route->handlers = mprCreateList(-1, 0);
        route->inputStages = mprCreateList(-1, 0);
        route->inputStages = mprCreateList(-1, 0);
    }
    route->outputStages = mprCreateList(-1, 0);
}


void httpResetHandlers(HttpRoute *route)
{
    route->handlers = mprCreateList(-1, 0);
}


void httpSetRouteAuth(HttpRoute *route, HttpAuth *auth)
{
    route->auth = auth;
}


void httpSetRouteAutoDelete(HttpRoute *route, int enable)
{
    route->autoDelete = enable;
}


void httpSetRouteCompression(HttpRoute *route, int flags)
{
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
    mprLog(MPR_CONFIG, "Set connector \"%s\"", name);
    return 0;
}


void httpSetRouteData(HttpRoute *route, cchar *key, void *data)
{
    if (route->data == 0) {
        route->data = mprCreateHash(-1, 0);
    }
    mprAddKey(route->data, key, data);
}


void httpSetRouteFlags(HttpRoute *route, int flags)
{
    route->flags = flags;
}


int httpSetRouteHandler(HttpRoute *route, cchar *name)
{
    HttpStage     *handler;

    mprAssert(route);
    
    if ((handler = httpLookupStage(route->http, name)) == 0) {
        mprError("Can't find handler %s", name); 
        return MPR_ERR_CANT_FIND;
    }
    route->handler = handler;
    return 0;
}


void httpSetRouteDir(HttpRoute *route, cchar *path)
{
    route->dir = httpMakePath(route, path);
    httpSetRoutePathVar(route, "DOCUMENT_ROOT", route->dir);
}


void httpSetRouteHost(HttpRoute *route, HttpHost *host)
{
    route->host = host;
    defineHostVars(route);
}


void httpSetRouteIndex(HttpRoute *route, cchar *index)
{
    route->index = sclone(index);
}


void httpSetRouteMethods(HttpRoute *route, cchar *methods)
{
    mprAssert(route);
    mprAssert(methods && methods);

    route->methods = sclone(methods);
    finalizeMethods(route);
}


void httpSetRouteName(HttpRoute *route, cchar *name)
{
    route->name = sclone(name);
}


void httpSetRoutePattern(HttpRoute *route, cchar *pattern, int flags)
{
    route->pattern = sclone(pattern);
    route->flags |= (flags & HTTP_ROUTE_NOT);
    finalizePattern(route);
}


void httpSetRouteScriptName(HttpRoute *route, cchar *scriptName)
{
    route->scriptName = sclone(scriptName);
    route->scriptNameLen = slen(scriptName);
}


void httpSetRouteSource(HttpRoute *route, cchar *source)
{
    mprAssert(route);
    mprAssert(source && *source);

    route->sourceName = sclone(source);
}


void httpSetRouteScript(HttpRoute *route, cchar *script, cchar *scriptPath)
{
    if (script) {
        route->script = sclone(script);
    }
    if (scriptPath) {
        route->scriptPath = sclone(scriptPath);
    }
}


/*
    Target names are extensible and hashed in http->routeTargets. 

        Target close [immediate]
        Target file ${DOCUMENT_ROOT}/${request:uri}.gz
        Target redirect [status] URI 
        Target virtual ${controller}-${name} 
 */
int httpSetRouteTarget(HttpRoute *route, cchar *kind, cchar *details)
{
    char    *target;

    mprAssert(route);
    mprAssert(kind && *kind);

    route->targetOp = sclone(kind);
    route->target = sclone(details);

    if (scasematch(kind, "close")) {
        route->closeTarget = route->target;

    } else if (scasematch(kind, "redirect")) {
        if (isdigit(route->target[0])) {
            if (!httpTokenize(route, route->target, "%N %S", &route->responseStatus, &target)) {
                return MPR_ERR_BAD_SYNTAX;
            }
        } else {
            if (!httpTokenize(route, route->target, "%S", &target)) {
                return MPR_ERR_BAD_SYNTAX;
            }
        }
        route->redirectTarget = finalizeReplacement(route, target);
        return 0;

    } else if (scasematch(kind, "file")) {
        route->fileTarget = finalizeReplacement(route, route->target);

    } else if (scasematch(kind, "virtual")) {
        route->virtualTarget = finalizeReplacement(route, route->target);

    } else if (scasematch(kind, "write")) {
        if (!httpTokenize(route, route->target, "%N %S", &route->responseStatus, &target)) {
            return MPR_ERR_BAD_SYNTAX;
        }
        route->writeTarget = finalizeReplacement(route, target);

    } else {
        return MPR_ERR_BAD_SYNTAX;
    }
    return 0;
}


void httpSetRouteWorkers(HttpRoute *route, int workers)
{
    route->workers = workers;
}


void httpAddRouteErrorDocument(HttpRoute *route, cchar *code, cchar *url)
{
    GRADUATE_HASH(route, errorDocuments);
    mprAddKey(route->errorDocuments, code, sclone(url));
}


cchar *httpLookupRouteErrorDocument(HttpRoute *route, int code)
{
    char        numBuf[16];

    if (route->errorDocuments == 0) {
        return 0;
    }
    itos(numBuf, sizeof(numBuf), code, 10);
    return (cchar*) mprLookupKey(route->errorDocuments, numBuf);
}

/********************************* Route Finalization *************************/

static void finalizeMethods(HttpRoute *route)
{
    char    *method, *methods, *tok;

    methods = route->methods;
    if (methods && *methods && !scasematch(route->methods, "ALL") && !smatch(route->methods, "*")) {
        if ((route->methodHash = mprCreateHash(-1, 0)) == 0) {
            return;
        }
        methods = sclone(methods);
        while ((method = stok(methods, ", \t\n\r", &tok)) != 0) {
            mprAddKey(route->methodHash, method, route);
            methods = 0;
        }
    } else {
        route->methods = sclone("*");
    }
}


/*
    Finalize the pattern. 
        - Change "\{n[:m]}" to "{n[:m]}"
        - Change "\~" to "~"
        - Change "(~ PAT ~)" to "(?: PAT )?"
        - Extract the tokens and change tokens: "{word}" to "([^/]*)"
        - Create a params RE replacement string of the form "$1:$2:$3" for the {tokens}
 */
static void finalizePattern(HttpRoute *route)
{
    MprBuf      *pattern, *params;
    cchar       *errMsg, *start;
    char        *cp, *ep, *token, *field;
    ssize       len;
    int         column, submatch;

    route->tokens = mprCreateList(-1, 0);
    pattern = mprCreateBuf(-1, -1);

    if (route->name == 0) {
        route->name = route->pattern;
    }
    params = mprCreateBuf(-1, -1);
    for (submatch = 0, cp = route->pattern; *cp; cp++) {
        /* Alias for optional, non-capturing pattern:  "(?: PAT )?" */
        if (*cp == '(' && cp[1] == '~') {
            mprPutStringToBuf(pattern, "(?:");
            cp++;

        } else if (*cp == '(') {
            mprPutCharToBuf(pattern, *cp);
            ++submatch;

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
                    } else {
                        field = "([^/]*)";
                    }
                    mprPutStringToBuf(pattern, field);
                    mprAddItem(route->tokens, token);
                    /* Params ends up looking like "$1:$2:$3:$4" */
                    mprPutCharToBuf(params, '$');
                    mprPutIntToBuf(params, ++submatch);
                    mprPutCharToBuf(params, ':');
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
    mprAddNullToBuf(params);
    route->processedPattern = sclone(mprGetBufStart(pattern));

    /*
        Create an optimized simple prefix to optimize route rejection
     */
    start = route->pattern[0] == '^' ? &route->pattern[1] : route->pattern;
    len = strcspn(start, "$*+?.(|{[\\");
    if (len) {
        route->prefixLen = len;
        route->prefix = snclone(start, len);
    }

    /* Trim last ":" from params */
    if (mprGetBufLength(params) > 0) {
        route->params = sclone(mprGetBufStart(params));
        route->params[slen(route->params) - 1] = '\0';
    }
    if ((route->patternCompiled = pcre_compile2(route->processedPattern, 0, 0, &errMsg, &column, NULL)) == 0) {
        mprError("Can't compile route. Error %s at column %d", errMsg, column); 
    }

    /*
        Create a template for links. Strip "()" and "/.*" from the pattern.
     */
    route->template = sreplace(route->pattern, "(", "");
    route->template = sreplace(route->template, ")", "");
    route->template = sreplace(route->template, "/.*", "");
}


static char *finalizeReplacement(HttpRoute *route, cchar *str)
{
    MprBuf      *buf;
    cchar       *item;
    cchar       *tok, *cp, *ep, *token;
    int         next, braced;

    /*
        Prepare a replacement string. Change $token to $N
     */
    buf = mprCreateBuf(-1, -1);
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
                    for (ep = tok; *ep && isdigit((int) *ep); ep++) ;
                }
                token = snclone(tok, ep - tok);
                if (schr(token, ':')) {
                    mprPutStringToBuf(buf, "$${");
                    mprPutStringToBuf(buf, token);
                    mprPutCharToBuf(buf, '}');
                } else {
                    for (next = 0; (item = mprGetNextItem(route->tokens, &next)) != 0; ) {
                        if (scmp(item, token) == 0) {
                            break;
                        }
                    }
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
    mprAddNullToBuf(buf);
    return sclone(mprGetBufStart(buf));
}


int httpFinalizeRoute(HttpRoute *route)
{
    /*
        Add the route to the owning host. When using an Appweb configuration file, the order of route finalization 
        will be from the inside out. This ensures that nested routes are defined BEFORE outer/enclosing routes.
        This is important as requests process routes in-order.
     */
    httpAddRoute(route->host, route);
    mprConfigureSsl(route->ssl);
    return 0;
}


/********************************* Path Expansion *****************************/

void httpSetRoutePathVar(HttpRoute *route, cchar *key, cchar *value)
{
    mprAssert(key);
    mprAssert(value);

    GRADUATE_HASH(route, pathVars);
    if (schr(value, '$')) {
        value = stemplate(value, route->pathVars);
    }
    mprAddKey(route->pathVars, key, sclone(value));
}


/*
    Make a path name. This replaces $references, converts to an absolute path name, cleans the path and maps delimiters.
 */
char *httpMakePath(HttpRoute *route, cchar *file)
{
    char    *result, *path;

    mprAssert(file);

    if ((path = stemplate(file, route->pathVars)) == 0) {
        return 0;
    }
    if (mprIsRelPath(path)) {
        result = mprJoinPath(route->host->home, path);
    } else {
        result = mprGetAbsPath(path);
    }
    return result;
}

/********************************* Language ***********************************/

void httpAddRouteLanguage(HttpRoute *route, cchar *lang, cchar *suffix, int flags)
{
    HttpLang    *lp;

    if (route->languages == 0) {
        route->languages = mprCreateHash(-1, 0);
    } else {
        GRADUATE_HASH(route, languages);
    }
    if ((lp = mprLookupKey(route->languages, lang)) != 0) {
        lp->suffix = sclone(suffix);
        lp->flags = flags;
    } else {
        mprAddKey(route->languages, lang, createLangDef(0, suffix, flags));
    }
}


void httpAddRouteLanguageRoot(HttpRoute *route, cchar *lang, cchar *path)
{
    HttpLang    *lp;

    if (route->languages == 0) {
        route->languages = mprCreateHash(-1, 0);
    } else {
        GRADUATE_HASH(route, languages);
    }
    if ((lp = mprLookupKey(route->languages, lang)) != 0) {
        lp->path = sclone(path);
    } else {
        mprAddKey(route->languages, lang, createLangDef(path, 0, 0));
    }
}


void httpSetRouteDefaultLanguage(HttpRoute *route, cchar *lang)
{
    route->defaultLanguage = sclone(lang);
}


/********************************* Conditions *********************************/

static int testCondition(HttpConn *conn, HttpRoute *route, HttpRouteOp *condition)
{
    HttpRouteProc   *proc;

    if ((proc = mprLookupKey(conn->http->routeConditions, condition->name)) == 0) {
        httpError(conn, -1, "Can't find route condition name %s", condition->name);
        return 0;
    }
    mprLog(0, "run condition on route %s condition %s", route->name, condition->name);
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

    rx = conn->rx;
    auth = rx->route->auth;
    if (auth == 0) {
        return HTTP_ROUTE_ACCEPTED;
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
            return 0;
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
            return 0;
        }
    }
    return HTTP_ROUTE_ACCEPTED;
}


static int authCondition(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpRx      *rx;

    rx = conn->rx;
    if (route->auth == 0 || route->auth->type == 0) {
        return HTTP_ROUTE_ACCEPTED;
    }
    return httpCheckAuth(conn);
}


static int directoryCondition(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpRx      *rx;
    MprPath     info;
    char        *path;

    rx = conn->rx;
    mapFile(conn, route);
    path = mprJoinPath(route->dir, expandPath(conn, op->details));
    mprGetPathInfo(path, &info);
    if (info.isDir) {
        return HTTP_ROUTE_ACCEPTED;
    }
    return 0;
}


/*
    Test if a file exists
 */
static int existsCondition(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpRx  *rx;
    char    *path;

    rx = conn->rx;
    /* Must have tx->filename set when expanding op->details, so map fileTarget now */
    mapFile(conn, route);
    path = mprJoinPath(route->dir, expandPath(conn, op->details));
    if (mprPathExists(path, R_OK)) {
        return HTTP_ROUTE_ACCEPTED;
    }
    return 0;
}


static int matchCondition(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpRx      *rx;
    char        *str;
    int         matched[HTTP_MAX_ROUTE_MATCHES * 2], count;

    rx = conn->rx;
    str = expandPath(conn, op->details);
    count = pcre_exec(op->mdata, NULL, str, (int) slen(str), 0, 0, matched, sizeof(matched) / sizeof(int)); 
    if (count > 0) {
        return HTTP_ROUTE_ACCEPTED;
    }
    return 0;
}


/********************************* Updates ******************************/

static int updateRequest(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpRouteProc   *proc;

    if ((proc = mprLookupKey(conn->http->routeUpdates, op->name)) == 0) {
        httpError(conn, -1, "Can't find route update name %s", op->name);
        return 0;
    }
    mprLog(0, "run update on route %s update %s", route->name, op->name);
    return (*proc)(conn, route, op);
}


static int cmdUpdate(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpRx  *rx;
    MprCmd  *cmd;
    char    *command, *out, *err;
    int     status;

    rx = conn->rx;
    command = expandPath(conn, op->details);
    cmd = mprCreateCmd(conn->dispatcher);
    if ((status = mprRunCmd(cmd, command, &out, &err, 0)) != 0) {
        mprError("Command failed: %s\nStatus: %d\n%s\n%s", command, status, out, err);
        /* This request will continue. Note: no error sent to the client. */
        return 0;
    }
    return HTTP_ROUTE_ACCEPTED;
}


static int fieldUpdate(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpRx  *rx;

    rx = conn->rx;
    httpSetFormVar(conn, op->var, expandPath(conn, op->value));
    return HTTP_ROUTE_ACCEPTED;
}

/*********************************** Targets **********************************/

static int closeTarget(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    httpError(conn, HTTP_CODE_RESET, "Route target \"close\" is closing request");
    if (scmp(route->closeTarget, "immediate") == 0) {
        httpDisconnect(conn);
    }
    return HTTP_ROUTE_ACCEPTED;
}


static int fileTarget(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    int     rc;

    rc = mapFile(conn, route);
    if (!conn->tx->fileInfo.valid && !(conn->rx->flags & HTTP_PUT)) {
        httpError(conn, HTTP_CODE_NOT_FOUND, "Can't open document: %s", conn->tx->filename);
    }
    return rc;
}


static int redirectTarget(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    HttpRx      *rx;
    HttpUri     *dest, *prior;
    cchar       *scheme, *host, *query, *reference, *uri, *target;
    int         port;

    rx = conn->rx;
    mprAssert(route->redirectTarget && *route->redirectTarget);

    target = expandPath(conn, route->redirectTarget);
    if (route->responseStatus) {
        httpRedirect(conn, route->responseStatus, target);
        return HTTP_ROUTE_ACCEPTED;
    }
    prior = rx->parsedUri;
    dest = httpCreateUri(route->redirectTarget, 0);
    scheme = dest->scheme ? dest->scheme : prior->scheme;
    host = dest->host ? dest->host : prior->host;
    port = dest->port ? dest->port : prior->port;
    query = dest->query ? dest->query : prior->query;
    reference = dest->reference ? dest->reference : prior->reference;
    uri = httpFormatUri(scheme, host, port, target, reference, query, 0);
    httpSetUri(conn, uri, 0);
    return HTTP_ROUTE_REROUTE;
}


static int virtualTarget(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    conn->rx->targetKey = route->virtualTarget ? expandPath(conn, route->virtualTarget) : sclone(&conn->rx->pathInfo[1]);
    return HTTP_ROUTE_ACCEPTED;
}


static int writeTarget(HttpConn *conn, HttpRoute *route, HttpRouteOp *op)
{
    char    *str;

    str = route->writeTarget ? expandPath(conn, route->writeTarget) : sclone(&conn->rx->pathInfo[1]);
    httpSetStatus(conn, route->responseStatus);
    httpFormatResponse(conn, "%s", str);
    return HTTP_ROUTE_ACCEPTED;
}


/*************************************************** Support Routines ****************************************************/
/*
    Route operations are used per-route for headers and fields
 */
static HttpRouteOp *createRouteOp(cchar *name, int flags)
{
    HttpRouteOp   *op;

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
    if (flags == 0) {
        flags |= HTTP_LANG_BEFORE;
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
    mprAddKey(route->pathVars, "PRODUCT", sclone(BLD_PRODUCT));
    mprAddKey(route->pathVars, "OS", sclone(BLD_OS));
    mprAddKey(route->pathVars, "VERSION", sclone(BLD_VERSION));
    mprAddKey(route->pathVars, "LIBDIR", mprGetNormalizedPath(sfmt("%s/../%s", mprGetAppDir(), BLD_LIB_NAME))); 
    if (route->host) {
        defineHostVars(route);
    }
}


static void defineHostVars(HttpRoute *route) 
{
    mprAddKey(route->pathVars, "DOCUMENT_ROOT", route->dir);
    mprAddKey(route->pathVars, "SERVER_ROOT", route->host->home);
}


static char *expandPath(HttpConn *conn, cchar *str)
{
    HttpRx      *rx;

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

        } else if (smatch(key, "field") || smatch(key, "query")) {
            /*
                MOB - this is permitting ${query} to match any post data var
             */
            field = stok(value, "=", &defaultValue);
            if (defaultValue == 0) {
                defaultValue = "";
            }
            mprPutStringToBuf(buf, httpGetFormVar(conn, field, defaultValue));

        } else if (smatch(key, "request")) {
            value = stok(value, "=", &defaultValue);
            if (defaultValue == 0) {
                defaultValue = "";
            }
            //  MOB - implement default value below for those that can be null
            //  MOB - OPT with switch on first char
            if (smatch(value, "clientAddress")) {
                mprPutStringToBuf(buf, conn->ip);

            } else if (smatch(value, "clientPort")) {
                mprPutIntToBuf(buf, conn->port);

            } else if (smatch(value, "ext")) {
                mprPutStringToBuf(buf, rx->parsedUri->ext);

            } else if (smatch(value, "extraPath")) {
                mprPutStringToBuf(buf, rx->extraPath);

            } else if (smatch(value, "filename")) {
                mprPutStringToBuf(buf, tx->filename);

            } else if (scasematch(value, "language")) {
                if (!*defaultValue) {
                    defaultValue = route->defaultLanguage;
                }
                lang = httpGetLanguage(conn, route->languages);
                mprPutStringToBuf(buf, (lang) ? lang->suffix : defaultValue);

            } else if (scasematch(value, "languageRoot")) {
                lang = httpGetLanguage(conn, route->languages);
                mprPutStringToBuf(buf, lang ? lang->path : ".");

            } else if (smatch(value, "host")) {
                mprPutStringToBuf(buf, rx->parsedUri->host);

            } else if (smatch(value, "method")) {
                mprPutStringToBuf(buf, rx->method);

            } else if (smatch(value, "originalUri")) {
                mprPutStringToBuf(buf, rx->originalUri);

            } else if (smatch(value, "pathInfo")) {
                mprPutStringToBuf(buf, rx->pathInfo);

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

            } else if (smatch(value, "serverAddress")) {
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
                if (isdigit((int) *cp)) {
                    submatch = (int) wtoi(cp, 10, NULL);
                    while (isdigit((int) *++cp))
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

    httpDefineRouteUpdate("field", fieldUpdate);
    httpDefineRouteUpdate("cmd", cmdUpdate);

    httpDefineRouteTarget("close", closeTarget);
    httpDefineRouteTarget("file", fileTarget);
    httpDefineRouteTarget("redirect", redirectTarget);
    httpDefineRouteTarget("virtual", virtualTarget);
    httpDefineRouteTarget("write", writeTarget);
}


/*
    Tokenizes a line using %formats. Mandatory tokens can be specified with %. Optional tokens are specified with ?. 
    Supported tokens:
        %B - Boolean. Parses: on/off, true/false, yes/no.
        %N - Number. Parses numbers in base 10.
        %S - String. Removes quotes.
        %P - Template path string. Removes quotes and expands ${PathVars}.
        %W - Parse words into a list
        %! - Optional negate. Set value to HTTP_ROUTE_NOT present, otherwise zero.
    Values wrapped in quotes will have the outermost quotes trimmed.
 */
bool httpTokenize(HttpRoute *route, cchar *line, cchar *fmt, ...)
{
    va_list     args;
    bool        rc;

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

    tok = sclone(line);
    end = &tok[slen(line)];

    for (f = fmt; *f && tok < end; f++) {
        for (; isspace((int) *tok); tok++) ;
        if (*tok == '\0' || *tok == '#') {
            break;
        }
        if (isspace((int) *f)) {
            continue;
        }
        if (*f == '%' || *f == '?') {
            f++;
            quote = 0;
            if (*tok == '"' || *tok == '\'') {
                quote = *tok++;
            }
            if (*f == '!') {
                etok = &tok[1];
            } else {
                if (quote && *f != '*') {
                    for (etok = tok; *etok && !(*etok == quote && etok[-1] != '\\'); etok++) ; 
                    *etok++ = '\0';
                } else if (*f == '*') {
                    for (etok = tok; *etok; etok++) {
                        if (*etok == '#') {
                            *etok = '\0';
                        }
                    }
                } else {
                    for (etok = tok; *etok && !isspace((int) *etok); etok++) ;
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
                *va_arg(args, int*) = (int) stoi(tok, 10, 0);
                break;
            case 'P':
                *va_arg(args, char**) = httpMakePath(route, strim(tok, "\"", MPR_TRIM_BOTH));
                break;
            case 'S':
                *va_arg(args, char**) = strim(tok, "\"", MPR_TRIM_BOTH);
                break;
            case 'T':
                value = strim(tok, "\"", MPR_TRIM_BOTH);
                *va_arg(args, char**) = stemplate(value, route->pathVars);
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
        for (; tok < end && isspace((int) *tok); tok++) ;
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

    len = slen(str);
    if (*str == '\"' && str[len - 1] == '\"' && len > 2 && str[1] != '\"') {
        return snclone(&str[1], len - 2);
    }
    return sclone(str);
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
