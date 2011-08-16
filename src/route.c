/*
    route.c -- Http request routing 

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"
#include    "pcre.h"

/********************************** Forwards **********************************/

static HttpLang *createLangDef(cchar *path, cchar *suffix, int flags);
static HttpRouteItem *createRouteItem(cchar *key, cchar *details, void *data, cchar *path, int flags);
static void definePathVars(HttpRoute *route);
static void defineHostVars(HttpRoute *route);
static char *expandTargetTokens(HttpConn *conn, char *targetKey);
static void manageRoute(HttpRoute *route, int flags);
static void manageLang(HttpLang *lang, int flags);
static void manageRouteItem(HttpRouteItem *item, int flags);
static int mapToStorage(HttpConn *conn, HttpRoute *route);
static int modifyRequest(HttpConn *conn, HttpRoute *route, HttpRouteItem *modification);
static int processDirectories(HttpConn *conn, HttpRoute *route);
static char *replace(cchar *str, cchar *replacement, int *matches, int matchCount);
static int selectHandler(HttpConn *conn, HttpRoute *route);
static int testCondition(HttpConn *conn, HttpRoute *route, HttpRouteItem *condition);
static void trimExtraPath(HttpConn *conn);

/************************************ Code ************************************/

HttpRoute *httpCreateRoute()
{
    HttpRoute  *route;

    if ((route = mprAllocObj(HttpRoute, manageRoute)) == 0) {
        return 0;
    }
    route->http = MPR->httpService;
    route->errorDocuments = mprCreateHash(HTTP_SMALL_HASH_SIZE, 0);
    route->handlers = mprCreateList(-1, 0);
    route->extensions = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS);
    route->expires = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_STATIC_VALUES);
    route->expiresByType = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_STATIC_VALUES);
    route->pathVars = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS);
    route->inputStages = mprCreateList(-1, 0);
    route->outputStages = mprCreateList(-1, 0);
    route->auth = httpCreateAuth(0);
    route->flags = HTTP_LOC_SMART;
    route->workers = -1;
    definePathVars(route);

#if UNUSED
    /*
        The selectHandler modification must be first, followed immediately by mapToStorage. User modifications come next.
     */
    route->modifications = mprCreateList(-1, 0);
#endif
    return route;
}


HttpRoute *httpCreateDefaultRoute()
{
    HttpRoute   *route;

    if ((route = httpCreateRoute("--default--")) == 0) {
        return 0;
    }
#if UNUSED
    httpSetRouteTarget(route, "accept", 0);
#endif
    return route;
}


/*  
    Create a new location block. Inherit from the parent. We use a copy-on-write scheme if these are modified later.
 */
HttpRoute *httpCreateInheritedRoute(HttpRoute *parent)
{
    HttpRoute  *route;

    if (parent == 0) {
        return httpCreateRoute();
    }
    if ((route = mprAllocObj(HttpRoute, manageRoute)) == 0) {
        return 0;
    }
    route->http = MPR->httpService;
    route->parent = parent;
    route->flags = parent->flags;
    route->inputStages = parent->inputStages;
    route->outputStages = parent->outputStages;
    route->handlers = parent->handlers;
    route->extensions = parent->extensions;
    route->expires = parent->expires;
    route->expiresByType = parent->expiresByType;
    route->pathVars = parent->pathVars;
    route->connector = parent->connector;
    route->errorDocuments = parent->errorDocuments;
    route->auth = httpCreateAuth(parent->auth);
    route->uploadDir = parent->uploadDir;
    route->autoDelete = parent->autoDelete;
    route->script = parent->script;
    route->scriptPath = parent->scriptPath;
    route->searchPath = parent->searchPath;
    route->ssl = parent->ssl;
    route->workers = parent->workers;

    route->methodHash = parent->methodHash;
    route->formFields = parent->formFields;
    route->headers = parent->headers;
    route->conditions = parent->conditions;
    route->modifications = parent->modifications;
    route->methods = parent->methods;
    route->pattern = parent->pattern;
    route->patternExpression = parent->patternExpression;
    route->patternCompiled = parent->patternCompiled;
    route->kind = parent->kind;
    route->targetDest = parent->targetDest;
    route->targetStatus = parent->targetStatus;
    route->sourceName = parent->sourceName;
    route->sourcePath = parent->sourcePath;
    route->template = parent->template;
    route->params = parent->params;
    route->tokens = parent->tokens;
    return route;
}


HttpRoute *httpCreateAliasRoute(HttpRoute *parent, cchar *prefix, cchar *path, int status)
{
    HttpRoute   *route;

    if ((route = httpCreateInheritedRoute(parent)) == 0) {
        return 0;
    }
    httpSetRoutePattern(route, prefix);
    httpSetRouteTarget(route, "fileTarget", sfmt("%s/%s", path, "$&"));
    route->targetStatus = status;
    return route;
}


static void manageRoute(HttpRoute *route, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(route->auth);
        mprMark(route->http);
        mprMark(route->connector);
        mprMark(route->handler);
        mprMark(route->extensions);
        mprMark(route->expires);
        mprMark(route->expiresByType);
        mprMark(route->pathVars);
        mprMark(route->handlers);
        mprMark(route->inputStages);
        mprMark(route->outputStages);
        mprMark(route->errorDocuments);
        mprMark(route->parent);
        mprMark(route->context);
        mprMark(route->uploadDir);
        mprMark(route->searchPath);
        mprMark(route->script);
        mprMark(route->scriptPath);
        mprMark(route->ssl);
        mprMark(route->data);
        mprMark(route->lang);
        mprMark(route->langPref);

        mprMark(route->name);
        mprMark(route->conditions);
        mprMark(route->modifications);
        mprMark(route->methods);
        mprMark(route->methodHash);
        mprMark(route->pattern);
        mprMark(route->sourceName);
        mprMark(route->sourcePath);
        mprMark(route->patternExpression);
        mprMark(route->params);
        mprMark(route->tokens);
        mprMark(route->formFields);
        mprMark(route->headers);

        mprMark(route->kind);
        mprMark(route->targetDetails);
        mprMark(route->targetDest);

    } else if (flags & MPR_MANAGE_FREE) {
        if (route->patternCompiled) {
            free(route->patternCompiled);
        }
    }
}


/*
    Compile the route into fast forms. This compiles the route->methods into a hash table, route->pattern into 
    a regular expression in route->patternCompiled.
 */
int httpFinalizeRoute(HttpRoute *route)
{
    MprBuf      *pattern, *params, *buf;
    cchar       *errMsg, *item;
    char        *methods, *method, *tok, *cp, *ep, *token, *field;
    int         column, errCode, index, next, braced;

    /*
        Convert the methods into a method hash
     */
    methods = route->methods;
    if (methods && *methods && scasecmp(route->methods, "ALL") != 0 && scmp(route->methods, "*") != 0) {
        if ((route->methodHash = mprCreateHash(-1, 0)) == 0) {
            return MPR_ERR_MEMORY;
        }
        methods = sclone(methods);
        while ((method = stok(methods, ", \t\n\r", &tok)) != 0) {
            mprAddKey(route->methodHash, method, route);
            methods = 0;
        }
    } else {
        route->methods = sclone("*");
    }
    /*
        Prepare the pattern. 
            - Extract the tokens and change tokens: "{word}" to "(word)"
            - Change optional sections: "(portion)" to "(?:portion)?"
            - Create a params RE replacement string of the form "$1:$2:$3" for the {tokens}
            - Wrap the pattern in "^" and "$"
     */
    route->tokens = mprCreateList(-1, 0);
    pattern = mprCreateBuf(-1, -1);

    if (route->pattern && *route->pattern) {
        if (route->pattern[0] == '%') {
            route->patternExpression = sclone(&route->pattern[1]);
            if (route->targetDetails) {
                route->targetDest = route->targetDetails;
            }
            /* Can't have a link template if using regular expressions */

        } else {
            params = mprCreateBuf(-1, -1);
            for (cp = route->pattern; *cp; cp++) {
                if (*cp == '(') {
                    mprPutStringToBuf(pattern, "(?:");

                } else if (*cp == ')') {
                    mprPutStringToBuf(pattern, ")?");

                } else if (*cp == '{' && (cp == route->pattern || cp[-1] != '\\')) {
                    if ((ep = schr(cp, '}')) != 0) {
                        /* Trim {} off the token and replace in pattern with "([^/]*)"  */
                        token = snclone(&cp[1], ep - cp - 1);
                        if ((field = schr(token, '=')) != 0) {
                            *field++ = '\0';
                        } else {
                            field = "([^/]*)";
                        }
                        mprPutStringToBuf(pattern, field);

                        index = mprAddItem(route->tokens, token);
                        /* Params ends up looking like "$1:$2:$3:$4" */
                        mprPutCharToBuf(params, '$');
                        mprPutIntToBuf(params, index + 1);
                        mprPutCharToBuf(params, ':');
                        cp = ep;
                    }
                } else {
                    mprPutCharToBuf(pattern, *cp);
                }
            }
            mprAddNullToBuf(pattern);
            mprAddNullToBuf(params);
            route->patternExpression = sclone(mprGetBufStart(pattern));

            /* Trim last ":" from params */
            if (mprGetBufLength(params) > 0) {
                route->params = sclone(mprGetBufStart(params));
                route->params[slen(route->params) - 1] = '\0';
            }

            /*
                Prepare the target. Change $token to $N
             */
            if (route->targetDetails) {
                buf = mprCreateBuf(-1, -1);
                for (cp = route->targetDetails; *cp; cp++) {
                    if ((tok = schr(cp, '$')) != 0 && (tok == route->targetDetails || tok[-1] != '\\')) {
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
                            for (ep = tok; *ep && *ep != '}'; ep++) ;
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
                        mprPutCharToBuf(buf, *cp);
                    }
                }
                mprAddNullToBuf(buf);
                route->targetDest = sclone(mprGetBufStart(buf));
            }
            /*
                Create a template for links. Strip "()" and "/.*" from the pattern.
             */
            route->template = sreplace(route->pattern, "(", "");
            route->template = sreplace(route->template, ")", "");
            route->template = sreplace(route->template, "/.*", "");
        }
        if ((route->patternCompiled = pcre_compile2(route->patternExpression, 0, &errCode, &errMsg, &column, NULL)) == 0) {
            mprError("Can't compile route. Error %s at column %d", errMsg, column); 
        }
    }
#if BLD_FEATURE_SSL
    if (route->ssl) {
        mprConfigureSsl(route->ssl);
    }
#endif
    return 0;
}


int httpMatchRoute(HttpConn *conn, HttpRoute *route)
{
    HttpRouteItem   *item, *condition, *modification;
    HttpRouteProc   *proc;
    HttpRx          *rx;
    cchar           *token, *value, *header, *field;
    int             next, rc, matched[10 * 3], count;

    rx = conn->rx;

    if (route->patternCompiled) {
        if ((rx->matchCount = pcre_exec(route->patternCompiled, NULL, rx->pathInfo, (int) slen(rx->pathInfo), 0, 0, 
                rx->matches, sizeof(rx->matches) / sizeof(int))) < 0) {
            return 0;
        }
    }
    if (route->methodHash && !mprLookupKey(route->methodHash, rx->method)) {
        return 0;
    }
    if (route->headers) {
        for (next = 0; (item = mprGetNextItem(route->headers, &next)) != 0; ) {
            if ((header = httpGetHeader(conn, item->name)) != 0) {
                count = pcre_exec(item->mdata, NULL, header, (int)slen(header), 0, 0, matched, sizeof(matched) / sizeof(int)); 
                rc = count > 0;
                if (item->flags & HTTP_ROUTE_NOT) {
                    rc = !rc;
                }
                if (!rc) {
                    return 0;
                }
            }
        }
    }
    if (route->formFields) {
        for (next = 0; (item = mprGetNextItem(route->formFields, &next)) != 0; ) {
            if ((field = httpGetFormVar(conn, item->name, "")) != 0) {
                count = pcre_exec(item->mdata, NULL, field, (int)slen(field), 0, 0, matched, sizeof(matched) / sizeof(int)); 
                rc = count > 0;
                if (item->flags & HTTP_ROUTE_NOT) {
                    rc = !rc;
                }
                if (!rc) {
                    return 0;
                }
            }
        }
    }

    selectHandler(conn, route);

    if (route->conditions) {
        for (next = 0; (condition = mprGetNextItem(route->conditions, &next)) != 0; ) {
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

    if (route->modifications) {
        for (next = 0; (modification = mprGetNextItem(route->modifications, &next)) != 0; ) {
            if ((rc = modifyRequest(conn, route, modification)) == HTTP_ROUTE_REROUTE) {
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
    if ((proc = mprLookupKey(conn->http->routeTargets, route->kind)) == 0) {
        httpError(conn, -1, "Can't find route target name %s", route->kind);
        return 0;
    }
    mprLog(0, "Run route \"%s\" target \"%s\"", route->name, route->kind);
    return (*proc)(conn, route, 0);
}


static int selectHandler(HttpConn *conn, HttpRoute *route)
{
    HttpTx      *tx;

    tx = conn->tx;
    if (route->handler) {
        tx->handler = route->handler;
    } else {
        if ((tx->handler = mprLookupKey(route->extensions, tx->ext)) == 0) {
            tx->handler = mprLookupKey(route->extensions, "");
        }
        if (tx->handler->match) {
            if (!tx->handler->match(conn, tx->handler, 0)) {
                tx->handler = conn->http->passHandler;
            }
        }
    }
    return HTTP_ROUTE_ACCEPTED;
}


static int mapToStorage(HttpConn *conn, HttpRoute *route)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpHost    *host;
    MprPath     *info, ginfo;
    char        *gfile;

    rx = conn->rx;
    tx = conn->tx;
    host = conn->host;
    mprAssert(tx->handler);

    if (tx->handler->flags & HTTP_STAGE_VIRTUAL) {
        return HTTP_ROUTE_ACCEPTED;
    }
    mprAssert(route->targetDest && *route->targetDest);

    //  MOB - what about: seps = mprGetPathSeparators(path);

    if (route->targetDest) {
        tx->filename = expandTargetTokens(conn, replace(rx->pathInfo, route->targetDest, rx->matches, rx->matchCount));
    } else {
        tx->filename = sclone(&rx->pathInfo[1]);
    }
    tx->ext = httpGetExtension(conn);
    if ((rx->dir = httpLookupBestDir(host, tx->filename)) == 0) {
        httpError(conn, HTTP_CODE_NOT_FOUND, "Missing directory block for \"%s\"", tx->filename);
        return 0;
    }
    info = &tx->fileInfo;
    mprGetPathInfo(tx->filename, info);
    if (!info->valid && rx->acceptEncoding && strstr(rx->acceptEncoding, "gzip") != 0) {
        gfile = sfmt("%s.gz", tx->filename);
        if (mprGetPathInfo(gfile, &ginfo) == 0) {
            tx->filename = gfile;
            tx->fileInfo = ginfo;
            httpSetHeader(conn, "Content-Encoding", "gzip");
        }
    }
    if (info->valid) {
        //  MOB - should this be moved to handlers ... yes
        tx->etag = sfmt("\"%x-%Lx-%Lx\"", info->inode, info->size, info->mtime);
    }
    if (tx->handler->flags & HTTP_STAGE_EXTRA_PATH) {
        trimExtraPath(conn);
    }
    mprLog(5, "mapToStorage uri \"%s\", filename: \"%s\", extension: \"%s\"", rx->uri, tx->filename, tx->ext);
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
    if (tx->handler->flags & HTTP_STAGE_VIRTUAL) {
        return HTTP_ROUTE_ACCEPTED;
    }
    rx = conn->rx;
    prior = rx->parsedUri;

    mprAssert(rx->dir);
    mprAssert(rx->dir->indexName);
    mprAssert(rx->pathInfo);
    mprAssert(tx->fileInfo.checked);

    if (!tx->fileInfo.isDir) {
        return HTTP_ROUTE_ACCEPTED;
    }
    if (sends(rx->pathInfo, "/")) {
        /*  
            Internal directory redirections
         */
        path = mprJoinPath(tx->filename, rx->dir->indexName);
        if (mprPathExists(path, R_OK)) {
            /*  
                Index file exists, so do an internal redirect to it. Client will not be aware of this happening.
             */
            pathInfo = mprJoinPath(rx->pathInfo, rx->dir->indexName);
            uri = httpFormatUri(prior->scheme, prior->host, prior->port, pathInfo, prior->reference, prior->query, 0);
            httpSetUri(conn, uri, 0);
            return HTTP_ROUTE_REROUTE;
        }
    } else {
        /* Must not append the index for the external redirect */
        pathInfo = sjoin(rx->pathInfo, "/", NULL);
        uri = httpFormatUri(prior->scheme, prior->host, prior->port, pathInfo, prior->reference, prior->query, 0);
        httpRedirect(conn, HTTP_CODE_MOVED_PERMANENTLY, uri);
    }
    return HTTP_ROUTE_ACCEPTED;
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

void httpAddRouteAuth(HttpRoute *route)
{
    httpAddRouteCondition(route, "auth", 0);
}


void httpAddRouteCondition(HttpRoute *route, cchar *name, int flags)
{
    if (route->conditions == 0) {
        route->conditions = mprCreateList(0, -1);
    } else if (route->parent && route->conditions == route->parent->conditions) {
        route->conditions = mprCloneList(route->parent->conditions);
    }
    mprAddItem(route->conditions, createRouteItem(name, 0, 0, 0, flags));
}


void httpAddRouteExpiry(HttpRoute *route, MprTime when, cchar *extensions)
{
    char    *types, *ext, *tok;

    if (extensions && *extensions) {
        if (route->parent && route->expires == route->parent->expires) {
            route->expires = mprCloneHash(route->parent->expires);
        }
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
        if (route->parent && route->expires == route->parent->expires) {
            route->expiresByType = mprCloneHash(route->parent->expiresByType);
        }
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
        if (route->parent && route->inputStages == route->parent->inputStages) {
            route->inputStages = mprCloneList(route->parent->inputStages);
        }
        mprAddItem(route->inputStages, filter);
    }
    if (direction & HTTP_STAGE_TX) {
        if (route->parent && route->outputStages == route->parent->outputStages) {
            route->outputStages = mprCloneList(route->parent->outputStages);
        }
        mprAddItem(route->outputStages, filter);
    }
    return 0;
}


void httpAddRouteField(HttpRoute *route, cchar *field, cchar *value, int flags)
{
    cchar   *errMsg;
    void    *mdata;
    int     column;

    mprAssert(route);
    mprAssert(field && *field);
    mprAssert(value && *value);

    if (route->formFields == 0) {
        route->formFields = mprCreateList(-1, 0);
    } else if  (route->parent && route->outputStages == route->parent->outputStages) {
        route->formFields = mprCloneList(route->parent->formFields);
    }
    if ((mdata = pcre_compile2(value, 0, 0, &errMsg, &column, NULL)) == 0) {
        mprError("Can't compile field pattern. Error %s at column %d", errMsg, column); 
    } else {
        mprAddItem(route->formFields, createRouteItem(field, 0, mdata, 0, flags));
    }
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
    if (route->parent && route->extensions == route->parent->extensions) {
        route->extensions = mprCloneHash(route->parent->extensions);
    }
    if (route->parent && route->handlers == route->parent->handlers) {
        route->handlers = mprCloneList(route->parent->handlers);
    }
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
            mprAddItem(route->handlers, handler);
        }
    }
#if UNUSED
    /*
        Optimize and eliminate adjacent addhandler mods
     */
    mods = route->modifications;
    len = mprGetListLength(mods);
    if (len > 0 && (item = mprGetItem(mods, len -1 )) && scmp(item->name, "selectHandler") == 0) {
        return 0;
    }
    httpAddRouteModification(route, "selectHandler", 0);
#endif
    return 0;
}


void httpAddRouteHeader(HttpRoute *route, cchar *header, cchar *value, int flags)
{
    cchar   *errMsg;
    void    *mdata;
    int     column;

    mprAssert(route);
    mprAssert(header && *header);
    mprAssert(value && *value);

    if (route->headers == 0) {
        route->headers = mprCreateList(-1, 0);
    } else if (route->parent && route->headers == route->parent->headers) {
        route->headers = mprCloneList(route->parent->headers);
    }
    if ((mdata = pcre_compile2(value, 0, 0, &errMsg, &column, NULL)) == 0) {
        mprError("Can't compile header pattern. Error %s at column %d", errMsg, column); 
    } else {
        mprAddItem(route->headers, createRouteItem(header, 0, mdata, 0, flags));
    }
}


void httpAddRouteLoad(HttpRoute *route, cchar *module, cchar *path)
{
    if (route->modifications == 0) {
        route->modifications = mprCreateList(0, -1);
    } else if (route->parent && route->modifications == route->parent->modifications) {
        route->modifications = mprCloneList(route->parent->modifications);
    }
    mprAddItem(route->modifications, createRouteItem("--load--", module, 0, path, 0));
}


void httpAddRouteModification(HttpRoute *route, cchar *name, int flags)
{
    mprAddItem(route->modifications, createRouteItem(name, 0, 0, 0, flags));
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


void httpDefineRouteModification(cchar *key, HttpRouteProc *proc)
{
    mprAddKey(((Http*) MPR->httpService)->routeModifications, key, proc);
}


void *httpGetRouteData(HttpRoute *route, cchar *key)
{
    if (!route->data) {
        return 0;
    }
    return mprLookupKey(route->data, key);
}


void httpSetRouteAuth(HttpRoute *route, HttpAuth *auth)
{
    route->auth = auth;
}


void httpSetRouteAutoDelete(HttpRoute *route, int enable)
{
    route->autoDelete = enable;
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
#if UNUSED
    httpAddRouteModification(route, "setHandlerModification", 0);
#endif
    return 0;
}


void httpSetRouteHost(HttpRoute *route, HttpHost *host)
{
    route->host = host;
    defineHostVars(route);
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


void httpSetRouteMethods(HttpRoute *route, cchar *methods)
{
    mprAssert(route);
    mprAssert(methods && methods);

    route->methods = sclone(methods);
}


void httpSetRoutePattern(HttpRoute *route, cchar *pattern)
{
    route->pattern = sclone(pattern);
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
    Internal names are: "close", "redirect", "file", "virt"

    Target alias "${DOCUMENT_ROOT}"
    Target close [immediate]
    Target redirect [status] URI            # Redirect to a new URI and re-route
    Target file "${DOCUMENT_ROOT}/${request:uri}"
    Target virt ${controller}-${name} 
    Target dest "${DOCUMENT_ROOT}/${request:rest}"
 */
void httpSetRouteTarget(HttpRoute *route, cchar *kind, cchar *details)
{
    char        *status;

    mprAssert(route);
    mprAssert(kind && *kind);

    route->kind = sclone(kind);
    if (scmp(kind, "redirect") == 0) {
        if (isdigit(details[0])) {
            status = stok(sclone(details), " \t", (char**) &details);
            route->targetStatus = atoi(status);
        }
    }
    route->targetDest = sclone(details);
}


void httpSetRouteWorkers(HttpRoute *route, int workers)
{
    route->workers = workers;
}


//  MOB - could this be done via routes?
void httpAddRouteErrorDocument(HttpRoute *route, cchar *code, cchar *url)
{
    if (route->parent && route->errorDocuments == route->parent->errorDocuments) {
        route->errorDocuments = mprCloneHash(route->parent->errorDocuments);
    }
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

/********************************* Path Expansion *****************************/

void httpSetRoutePathVar(HttpRoute *route, cchar *key, cchar *value)
{
    mprAssert(key);
    mprAssert(value);

    if (route->parent && route->pathVars == route->parent->pathVars) {
        route->pathVars = mprCloneHash(route->parent->pathVars);
    }
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
        result = mprJoinPath(route->host->serverRoot, path);
    } else {
        result = mprGetAbsPath(path);
    }
    return result;
}

/********************************* Language ***********************************/

void httpAddRouteLanguage(HttpRoute *route, cchar *lang, cchar *suffix, int flags)
{
    if (route->lang == 0) {
        route->lang = mprCreateHash(-1, 0);
    }
    mprAddKey(route->lang, lang, createLangDef(0, suffix, flags));
}


void httpAddRouteLanguageRoot(HttpRoute *route, cchar *lang, cchar *path)
{
    if (route->lang == 0) {
        route->lang = mprCreateHash(-1, 0);
    }
    mprAddKey(route->lang, lang, createLangDef(path, 0, 0));
}


static int compareLang(char **s1, char **s2)
{
    return scmp(*s1, *s2);
}


MprList *httpGetBestLanguage(HttpRoute *route, cchar *accept)
{
    MprList     *list;
    char        *lang, *next, *tok, *quality;

    if (route->langPref) {
        route->langPref = list = mprCreateList(-1, 0);
    }
    for (tok = stok(sclone(accept), ",", &next); tok; tok = stok(next, ",", &next)) {
        lang = stok(tok, ";", &quality);
        mprAddItem(list, sfmt("%d %s", (int) (atof(quality) * 100), lang));
    }
    mprSortList(list, compareLang);
    return list;
}

/********************************* Conditions *********************************/

static int testCondition(HttpConn *conn, HttpRoute *route, HttpRouteItem *condition)
{
    HttpRouteProc   *proc;

    if ((proc = mprLookupKey(conn->http->routeConditions, condition->name)) == 0) {
        httpError(conn, -1, "Can't find route condition name %s", condition->name);
        return 0;
    }
    mprLog(0, "run condition on route %s condition %s", route->name, condition->name);
    return (*proc)(conn, route, condition);
}


static int modifyRequest(HttpConn *conn, HttpRoute *route, HttpRouteItem *modification)
{
    HttpRouteProc   *proc;

    if ((proc = mprLookupKey(conn->http->routeModifications, modification->name)) == 0) {
        httpError(conn, -1, "Can't find route modification name %s", modification->name);
        return 0;
    }
    mprLog(0, "run modification on route %s modification %s", route->name, modification->name);
    return (*proc)(conn, route, modification);
}


static int authCondition(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    HttpRx      *rx;
    HttpAuth    *auth;

    rx = conn->rx;
    auth = rx->dir->auth ? rx->dir->auth : rx->route->auth;
    if (conn->server || auth == 0 || auth->type == 0) {
        return HTTP_ROUTE_ACCEPTED;
    }
    return httpCheckAuth(conn);
}


static int fileMissingCondition(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    HttpTx  *tx;

    tx = conn->tx;
    if (!tx->filename) {
        mapToStorage(conn, conn->rx->route);
    }
    if (tx->fileInfo.valid) {
        return HTTP_ROUTE_ACCEPTED;
    }
    return 0;
}


static int directoryCondition(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    HttpTx  *tx;

    tx = conn->tx;
    if (!tx->fileInfo.checked) {
        mprGetPathInfo(tx->filename, &tx->fileInfo);
    }
    if (tx->fileInfo.isDir) {
        return HTTP_ROUTE_ACCEPTED;
    } 
    return 0;
}

/********************************* Modifications ******************************/

static int fieldModification(HttpConn *conn, HttpRoute *route, HttpRouteItem *modification)
{
#if FUTURE
    httpSetFormVar(conn, modification->name, momdification->value);
#endif
    return 0;
}


/*********************************** Targets **********************************/

#if UNUSED
static int acceptTarget(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    return HTTP_ROUTE_ACCEPTED;
}
#endif


static int closeTarget(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    httpError(conn, HTTP_CODE_RESET, "Route target \"close\" is closing request");
    if (scmp(route->targetDetails, "immediate") == 0) {
        httpDisconnect(conn);
    }
    return HTTP_ROUTE_ACCEPTED;
}


#if FUTURE
static int cmdTarget(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    //  MOB - is this terminal?
    //  MOB - what response should be sent to the client
    return 0;
}
#endif


static int redirectTarget(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    HttpRx      *rx;
    HttpUri     *dest, *prior;
    cchar       *query, *reference, *uri;

    rx = conn->rx;

    if (route->targetStatus) {
        httpRedirect(conn, route->targetStatus, route->targetDest);
        return HTTP_ROUTE_ACCEPTED;
    }
    dest = httpCreateUri(route->targetDest, 0);
    prior = rx->parsedUri;
    query = dest->query ? dest->query : prior->query;
    reference = dest->reference ? dest->reference : prior->reference;
    uri = httpFormatUri(prior->scheme, prior->host, prior->port, route->targetDest, prior->reference, prior->query, 0);
    httpSetUri(conn, uri, 0);
    return HTTP_ROUTE_REROUTE;
}


static int fileTarget(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    HttpRx  *rx;

    rx = conn->rx;
    if (route->targetDest) {
        rx->targetKey = expandTargetTokens(conn, replace(rx->pathInfo, route->targetDest, rx->matches, rx->matchCount));
    } else {
        rx->targetKey = sclone(&rx->pathInfo[1]);
    }
    mapToStorage(conn, route);
    return processDirectories(conn, route);
}


static int virtTarget(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    HttpRx  *rx;

    rx = conn->rx;
    if (route->targetDest) {
        rx->targetKey = expandTargetTokens(conn, replace(rx->pathInfo, route->targetDest, rx->matches, rx->matchCount));
    } else {
        rx->targetKey = sclone(&rx->pathInfo[1]);
    }
    return HTTP_ROUTE_ACCEPTED;
}


/*************************************************** Support Routines ****************************************************/
/*
    Route items are used per-route for headers and fields
    MOB _ not a great API
 */
static HttpRouteItem *createRouteItem(cchar *name, cchar *details, void *mdata, cchar *path, int flags)
{
    HttpRouteItem   *item;

    if ((item = mprAllocObj(HttpRouteItem, manageRouteItem)) == 0) {
        return 0;
    }
    item->name = sclone(name);
    item->details = sclone(details);
    item->path = sclone(path);
    item->mdata = mdata;
    item->flags = flags;
    return item;
}


static void manageRouteItem(HttpRouteItem *item, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(item->name);
        mprMark(item->path);
        mprMark(item->details);

    } else if (flags & MPR_MANAGE_FREE) {
        if (item->flags & HTTP_ROUTE_FREE) {
            free(item->mdata);
            item->mdata = 0;
        }
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

    //  MOB - is host ever set?
    if (route->host) {
        defineHostVars(route);
    }
}


static void defineHostVars(HttpRoute *route) 
{
    mprAddKey(route->pathVars, "DOCUMENT_ROOT", route->host->documentRoot);
    mprAddKey(route->pathVars, "SERVER_ROOT", route->host->serverRoot);
}


/*
    Expand the target tokens "head" and "field"
    WARNING: targetKey is modified. Result is allocated string
 */
static char *expandTargetTokens(HttpConn *conn, char *targetKey)
{
    HttpRx  *rx;
    HttpTx  *tx;
    MprBuf  *buf;
    char    *tok, *cp, *key, *value;

    rx = conn->rx;
    tx = conn->tx;
    buf = mprCreateBuf(-1, -1);
    for (cp = targetKey; cp && *cp; ) {
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

        if (scasecmp(key, "header") == 0) {
            mprPutStringToBuf(buf, httpGetHeader(conn, value));

        } else if (scasecmp(key, "field") == 0) {
            mprPutStringToBuf(buf, httpGetFormVar(conn, value, ""));

        } else if (scasecmp(key, "request") == 0) {

            if (scasecmp(value, "extraPath") == 0) {
                mprPutStringToBuf(buf, rx->extraPath);

            } else if (scasecmp(value, "filename") == 0) {
                mprPutStringToBuf(buf, tx->filename);

            } else if (scasecmp(value, "host") == 0) {
                mprPutStringToBuf(buf, rx->parsedUri->host);

            } else if (scasecmp(value, "ext") == 0) {
                mprPutStringToBuf(buf, rx->parsedUri->ext);

            } else if (scasecmp(value, "reference") == 0) {
                mprPutStringToBuf(buf, rx->parsedUri->reference);

            } else if (scasecmp(value, "query") == 0) {
                mprPutStringToBuf(buf, rx->parsedUri->query);

            } else if (scasecmp(value, "method") == 0) {
                mprPutStringToBuf(buf, rx->method);

            } else if (scasecmp(value, "originalUri") == 0) {
                mprPutStringToBuf(buf, rx->originalUri);

            } else if (scasecmp(value, "path") == 0) {
                mprPutStringToBuf(buf, rx->parsedUri->path);

            } else if (scasecmp(value, "pathInfo") == 0) {
                mprPutStringToBuf(buf, rx->pathInfo);

            } else if (scasecmp(value, "port") == 0) {
                mprPutIntToBuf(buf, conn->sock->acceptPort);

            } else if (scasecmp(value, "scheme") == 0) {
                mprPutStringToBuf(buf, rx->parsedUri->scheme);

            } else if (scasecmp(value, "scriptName") == 0) {
                mprPutStringToBuf(buf, rx->scriptName);

            } else if (scasecmp(value, "serverAddr") == 0) {
                mprPutStringToBuf(buf, conn->sock->acceptIp);

            } else if (scasecmp(value, "serverPort") == 0) {
                mprPutIntToBuf(buf, conn->sock->acceptPort);

            } else if (scasecmp(value, "uri") == 0) {
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
static char *replace(cchar *str, cchar *replacement, int *matches, int matchCount)
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
    httpDefineRouteCondition("auth", authCondition);
    httpDefineRouteCondition("missing", fileMissingCondition);
    httpDefineRouteCondition("directory", directoryCondition);

    httpDefineRouteModification("field", fieldModification);

    //  MOB - what about "alias"
    httpDefineRouteTarget("close", closeTarget);
    httpDefineRouteTarget("redirect", redirectTarget);
    httpDefineRouteTarget("file", fileTarget);
    httpDefineRouteTarget("virt", virtTarget);
}


#if UNUSED
HttpStage *httpGetHandlerByExtension(HttpRoute *route, cchar *ext)
{
    return (HttpStage*) mprLookupKey(route->extensions, ext);
}

char *httpMakeFilename(HttpConn *conn, cchar *url, bool skipAliasPrefix)
{
    cchar   *seps;
    char    *path;
    int     len;

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
#endif


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
