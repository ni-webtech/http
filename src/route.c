/*
    route.c -- Request routing

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "http.h"
#include    "pcre.h"

/************************************* Local **********************************/

static HttpRouteItem *createRouteItem(cchar *key, cchar *details, void *data, cchar *path, int flags);
static char *expandTargetTokens(HttpConn *conn, char *targetKey);
static void manageRoute(HttpRoute *route, int flags);
static void manageRouteItem(HttpRouteItem *item, int flags);
static int modifyRequest(HttpConn *conn, HttpRoute *route, HttpRouteItem *modification);
static char *replace(cchar *str, cchar *replacement, int *matches, int matchCount);
static int testCondition(HttpConn *conn, HttpRoute *route, HttpRouteItem *condition);

/************************************* Code ***********************************/

HttpRoute *httpCreateRoute(cchar *name)
{
    HttpRoute    *route;

    mprAssert(name && *name);
    if ((route = mprAllocObj(HttpRoute, manageRoute)) == 0) {
        return 0;
    }
    route->name = sclone(name);
    return route;
}


HttpRoute *httpCreateDefaultRoute()
{
    HttpRoute   *route;

    if ((route = httpCreateRoute("--default--")) == 0) {
        return 0;
    }
    httpSetRouteTarget(route, "accept", 0);
    return route;
}


#if FUTURE && UNUSED
HttpRoute *httpCreateRouteWithTarget(cchar *name, cchar *targetName, cchar *target)
{
    HttpRoute       *route;

    mprAssert(name && *name);
    mprAssert(targetName && *targetName);

    if ((route = httpCreateRoute(name)) == 0) {
        return 0;
    }
    httpSetRouteTarget(route, targetName, target);
    httpFinalizeRoute(route);
    return route;
}
#endif


static void manageRoute(HttpRoute *route, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
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

        mprMark(route->targetName);
        mprMark(route->targetDetails);
        mprMark(route->targetDest);

    } else if (flags & MPR_MANAGE_FREE) {
        if (route->patternCompiled) {
            free(route->patternCompiled);
        }
    }
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
    if (route->conditions) {
        for (next = 0; (condition = mprGetNextItem(route->conditions, &next)) != 0; ) {
            rc = testCondition(conn, route, condition);
            if (condition->flags & HTTP_ROUTE_NOT) {
                rc = !rc;
            }
            if (!rc) {
                return 0;
            }
        }
    }
    if (route->extensions) {
        mprLookup(route->extensions
        for (next = 0; (condition = mprGetNextItem(route->conditions, &next)) != 0; ) {
            rc = testCondition(conn, route, condition);
            if (condition->flags & HTTP_ROUTE_NOT) {
                rc = !rc;
            }
            if (!rc) {
                return 0;
            }
    }

    //  Point of no return
    if (route->modifications) {
        for (next = 0; (modification = mprGetNextItem(route->modifications, &next)) != 0; ) {
            modifyRequest(conn, route, modification);
        }
    }
    if (route->params) {
        for (next = 0; (token = mprGetNextItem(route->tokens, &next)) != 0; ) {
            value = snclone(&rx->pathInfo[rx->matches[next * 2]], rx->matches[(next * 2) + 1] - rx->matches[(next * 2)]);
            httpSetFormVar(conn, token, value);
        }
    }
    if ((proc = mprLookupKey(conn->http->routeTargets, route->targetName)) == 0) {
        httpError(conn, -1, "Can't find route target name %s", route->targetName);
        return 0;
    }
    mprLog(0, "Run route \"%s\" target \"%s\"", route->name, route->targetName);
    return (*proc)(conn, route, 0);
}


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
    (*proc)(conn, route, modification);
    return 0;
}




static int missingCondition(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    HttpTx  *tx;

    tx = conn->tx;
    if (!tx->filename) {
        httpMapToStorage(conn);
    }
    if (!tx->fileInfo.checked) {
        mprGetPathInfo(tx->filename, &tx->fileInfo);
    }
    return tx->fileInfo.valid;
}


static int directoryCondition(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    HttpTx  *tx;

    tx = conn->tx;
    if (!tx->fileInfo.checked) {
        mprGetPathInfo(tx->filename, &tx->fileInfo);
    }
    return tx->fileInfo.isDir;
}


static int fieldModification(HttpConn *conn, HttpRoute *route, HttpRouteItem *modification)
{
#if FUTURE
    httpSetFormVar(conn, modification->name, momdification->value);
#endif
    return 0;
}


static int addHandler(HttpConn *conn, HttpRoute *route, HttpRouteItem *modification)
{
    HttpStage   *handler;
    HttpTx      *tx;

    tx = conn->tx;
    tx->handler = mprLookupKey(route->extensions, tx->ext);
    return 0;
}


static int setHandler(HttpConn *conn, HttpRoute *route, HttpRouteItem *modification)
{
    conn->tx->handler = route->handler;
    return 0;
}


static int acceptTarget(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    return HTTP_ROUTE_COMPLETE;
}


#if UNUSED
static int aliasTarget(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    HttpRx      *rx;

    rx = conn->rx;
    if (rx->alias->redirectCode) {
        httpRedirect(conn, rx->alias->redirectCode, rx->alias->uri);
        return HTTP_ROUTE_COMPLETE;
    }
    return 0;
}
#endif


static int closeTarget(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    httpError(conn, HTTP_CODE_RESET, "Route target \"close\" is closing request");
    if (scmp(route->targetDetails, "immediate") == 0) {
        httpDisconnect(conn);
    }
    return HTTP_ROUTE_COMPLETE;
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
    HttpUri     *dest, *prior;
    cchar       *query, *reference, *uri;

    if (route->targetStatus) {
        httpRedirect(conn, route->targetStatus, route->targetDest);
        return HTTP_ROUTE_COMPLETE;
    }
    prior = conn->rx->parsedUri;
    dest = httpCreateUri(route->targetDest, 0);
    query = dest->query ? dest->query : prior->query;
    reference = dest->reference ? dest->reference : prior->reference;
    uri = httpFormatUri(prior->scheme, prior->host, prior->port, route->targetDest, prior->reference, prior->query, 0);
    httpSetUri(conn, uri, 0);
    return HTTP_ROUTE_REROUTE;
}


static int routeTarget(HttpConn *conn, HttpRoute *route, HttpRouteItem *unused)
{
    HttpRx  *rx;

    rx = conn->rx;
    if (route->targetDest) {
        rx->targetKey = expandTargetTokens(conn, replace(rx->pathInfo, route->targetDest, rx->matches, rx->matchCount));
    } else {
        rx->targetKey = sclone(&rx->pathInfo[1]);
    }
    return HTTP_ROUTE_COMPLETE;
}


void httpDefineRouteBuiltins()
{
    httpDefineRouteCondition("missing", missingCondition);
    httpDefineRouteCondition("directory", directoryCondition);

    httpDefineRouteModification("handler", handlerSelection);
    httpDefineRouteModification("field", fieldModification);

    httpDefineRouteTarget("accept", acceptTarget);
    httpDefineRouteTarget("close", closeTarget);
    httpDefineRouteTarget("redirect", redirectTarget);
    httpDefineRouteTarget("route", routeTarget);
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
    return 0;
}


/*
    Expand the target tokens "head" and "field"
    WARNING: targetKey is modified. Result is allocated string
 */
static char *expandTargetTokens(HttpConn *conn, char *targetKey)
{
    MprBuf  *buf;
    char    *tok, *cp, *key, *value;

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


void httpSetRouteTarget(HttpRoute *route, cchar *name, cchar *details)
{
    char        *status;

    mprAssert(route);
    mprAssert(name && *name);

    route->targetName = sclone(name);
    if (scmp(name, "redirect") == 0) {
        if (isdigit(details[0])) {
            status = stok(sclone(details), " \t", (char**) &details);
            route->targetStatus = atoi(status);
        }
    }
    route->targetDest = sclone(details);
}


void httpSetRouteSource(HttpRoute *route, cchar *source)
{
    mprAssert(route);
    mprAssert(source && *source);

    route->sourceName = sclone(source);
}


void httpSetRouteMethods(HttpRoute *route, cchar *methods)
{
    mprAssert(route);
    mprAssert(methods && methods);

    route->methods = sclone(methods);
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
    }
    if ((mdata = pcre_compile2(value, 0, 0, &errMsg, &column, NULL)) == 0) {
        mprError("Can't compile header pattern. Error %s at column %d", errMsg, column); 
    } else {
        mprAddItem(route->headers, createRouteItem(header, 0, mdata, 0, flags));
    }
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
    }
    if ((mdata = pcre_compile2(value, 0, 0, &errMsg, &column, NULL)) == 0) {
        mprError("Can't compile field pattern. Error %s at column %d", errMsg, column); 
    } else {
        mprAddItem(route->formFields, createRouteItem(field, 0, mdata, 0, flags));
    }
}


void httpAddRouteCondition(HttpRoute *route, cchar *name, int flags)
{
    mprAddItem(route->conditions, createRouteItem(name, 0, 0, 0, flags));
}


void httpAddRouteModification(HttpRoute *route, cchar *name, int flags)
{
    mprAddItem(route->modifications, createRouteItem(name, 0, 0, 0, flags));
}


void httpAddRouteLoad(HttpRoute *route, cchar *module, cchar *path)
{
    //  MOB - implement via modifications
    mprAddItem(route->modifications, createRouteItem("--load--", module, 0, path, 0));
}


void httpSetRoutePattern(HttpRoute *route, cchar *pattern)
{
    route->pattern = sclone(pattern);
}


void httpAddRouteHandler(HttpRoute *route, cchar *field, cchar *value, int flags)
{
}

void httpAddRouteHandler(HttpRoute *route, cchar *handler, cchar *value, int flags)
{
    httpAddRouteModification(route, "addHandler", 0, 0);
}


#if UNUSED
static char *trimSlashes(cchar *str)
{
    ssize   len;

    if (str == 0) {
        return MPR->emptyString;
    }
    if (*str == '/') {
        str++;
    }
    len = slen(str);
    if (str[len - 1] == '/') {
        return snclone(str, len - 1);
    } else {
        return sclone(str);
    }
}
#endif


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
