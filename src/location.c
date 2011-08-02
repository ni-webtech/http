/*
    location.c -- Server configuration for portions of the server (Location blocks).

    Location directives provide authorization and handler matching based on URL prefixes.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

static void defineKeywords(HttpLoc *loc);
static void manageLoc(HttpLoc *loc, int flags);

/************************************ Code ************************************/

HttpLoc *httpCreateLocation()
{
    HttpLoc  *loc;

    if ((loc = mprAllocObj(HttpLoc, manageLoc)) == 0) {
        return 0;
    }
    loc->http = MPR->httpService;
    loc->errorDocuments = mprCreateHash(HTTP_SMALL_HASH_SIZE, 0);
    loc->handlers = mprCreateList(-1, 0);
    loc->extensions = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS);
    loc->expires = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_STATIC_VALUES);
    loc->expiresByType = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_STATIC_VALUES);
    loc->keywords = mprCreateHash(HTTP_SMALL_HASH_SIZE, MPR_HASH_CASELESS);
    loc->inputStages = mprCreateList(-1, 0);
    loc->outputStages = mprCreateList(-1, 0);
    loc->prefix = mprEmptyString();
    loc->prefixLen = (int) strlen(loc->prefix);
    loc->auth = httpCreateAuth(0);
    loc->flags = HTTP_LOC_SMART;
    loc->workers = -1;
    defineKeywords(loc);
    return loc;
}


static void manageLoc(HttpLoc *loc, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(loc->auth);
        mprMark(loc->http);
        mprMark(loc->prefix);
        mprMark(loc->connector);
        mprMark(loc->handler);
        mprMark(loc->extensions);
        mprMark(loc->expires);
        mprMark(loc->expiresByType);
        mprMark(loc->keywords);
        mprMark(loc->handlers);
        mprMark(loc->inputStages);
        mprMark(loc->outputStages);
        mprMark(loc->errorDocuments);
        mprMark(loc->parent);
        mprMark(loc->context);
        mprMark(loc->uploadDir);
        mprMark(loc->searchPath);
        mprMark(loc->script);
        mprMark(loc->scriptPath);
        mprMark(loc->ssl);
        mprMark(loc->data);
    }
}

/*  
    Create a new location block. Inherit from the parent. We use a copy-on-write scheme if these are modified later.
 */
HttpLoc *httpCreateInheritedLocation(HttpLoc *parent)
{
    HttpLoc  *loc;

    if (parent == 0) {
        return httpCreateLocation();
    }
    if ((loc = mprAllocObj(HttpLoc, manageLoc)) == 0) {
        return 0;
    }
    loc->http = MPR->httpService;
    loc->prefix = sclone(parent->prefix);
    loc->parent = parent;
    loc->prefixLen = parent->prefixLen;
    loc->flags = parent->flags;
    loc->inputStages = parent->inputStages;
    loc->outputStages = parent->outputStages;
    loc->handlers = parent->handlers;
    loc->extensions = parent->extensions;
    loc->expires = parent->expires;
    loc->expiresByType = parent->expiresByType;
    loc->connector = parent->connector;
    loc->errorDocuments = parent->errorDocuments;
    loc->auth = httpCreateAuth(parent->auth);
    loc->uploadDir = parent->uploadDir;
    loc->autoDelete = parent->autoDelete;
    loc->script = parent->script;
    loc->scriptPath = parent->scriptPath;
    loc->searchPath = parent->searchPath;
    loc->ssl = parent->ssl;
    loc->workers = parent->workers;
    return loc;
}


/*
    Separate from parent. Clone own configuration
 */
static void graduate(HttpLoc *loc) 
{
    if (loc->parent) {
        loc->errorDocuments = mprCloneHash(loc->parent->errorDocuments);
        loc->expires = mprCloneHash(loc->parent->expires);
        loc->expiresByType = mprCloneHash(loc->parent->expiresByType);
        loc->extensions = mprCloneHash(loc->parent->extensions);
        loc->keywords = mprCloneHash(loc->parent->keywords);
        loc->handlers = mprCloneList(loc->parent->handlers);
        loc->inputStages = mprCloneList(loc->parent->inputStages);
        loc->outputStages = mprCloneList(loc->parent->outputStages);
        loc->parent = 0;
    }
}

void httpFinalizeLocation(HttpLoc *loc)
{
#if BLD_FEATURE_SSL
    if (loc->ssl) {
        mprConfigureSsl(loc->ssl);
    }
#endif
}


/*  
    Add a handler. This adds a handler to the set of possible handlers for a set of file extensions.
 */
int httpAddHandler(HttpLoc *loc, cchar *name, cchar *extensions)
{
    Http        *http;
    HttpStage   *handler;
    char        *extlist, *word, *tok;

    mprAssert(loc);

    http = loc->http;
    graduate(loc);

    if ((handler = httpLookupStage(http, name)) == 0) {
        mprError("Can't find stage %s", name); 
        return MPR_ERR_CANT_FIND;
    }
    if (extensions && *extensions) {
        mprLog(MPR_CONFIG, "Add handler \"%s\" for extensions: %s", name, extensions);
    } else {
        mprLog(MPR_CONFIG, "Add handler \"%s\" for prefix: \"%s\"", name, loc->prefix);
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
            mprAddKey(loc->extensions, word, handler);
            word = stok(0, " \t\r\n", &tok);
        }

    } else {
        if (handler->match == 0) {
            /*
                Only match by extensions if no-match routine provided.
             */
            mprAddKey(loc->extensions, "", handler);
        }
        if (mprLookupItem(loc->handlers, handler) < 0) {
            mprAddItem(loc->handlers, handler);
        }
    }
    return 0;
}


/*  
    Set a handler to apply to requests in this location block.
 */
int httpSetHandler(HttpLoc *loc, cchar *name)
{
    HttpStage     *handler;

    mprAssert(loc);
    
    graduate(loc);
    handler = httpLookupStage(loc->http, name);
    if (handler == 0) {
        mprError("Can't find handler %s", name); 
        return MPR_ERR_CANT_FIND;
    }
    loc->handler = handler;
    return 0;
}


/*  
    Add a filter. Direction defines what direction the stage filter be defined.
 */
int httpAddFilter(HttpLoc *loc, cchar *name, cchar *extensions, int direction)
{
    HttpStage   *stage;
    HttpStage   *filter;
    char        *extlist, *word, *tok;

    mprAssert(loc);
    
    stage = httpLookupStage(loc->http, name);
    if (stage == 0) {
        mprError("Can't find filter %s", name); 
        return MPR_ERR_CANT_FIND;
    }
    /*
        Clone an existing stage because each filter stores its own set of extensions to match against
     */
    filter = httpCloneStage(loc->http, stage);

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
    graduate(loc);
    if (direction & HTTP_STAGE_RX) {
        mprAddItem(loc->inputStages, filter);
    }
    if (direction & HTTP_STAGE_TX) {
        mprAddItem(loc->outputStages, filter);
    }
    return 0;
}


void httpAddLocationExpiry(HttpLoc *loc, MprTime when, cchar *extensions)
{
    char    *types, *ext, *tok;

    if (extensions && *extensions) {
        graduate(loc);
        types = sclone(extensions);
        ext = stok(types, " ,\t\r\n", &tok);
        while (ext) {
            mprAddKey(loc->expires, ext, ITOP(when));
            ext = stok(0, " \t\r\n", &tok);
        }
    }
}


void httpAddLocationExpiryByType(HttpLoc *loc, MprTime when, cchar *mimeTypes)
{
    char    *types, *mime, *tok;

    if (mimeTypes && *mimeTypes) {
        graduate(loc);
        types = sclone(mimeTypes);
        mime = stok(types, " ,\t\r\n", &tok);
        while (mime) {
            mprAddKey(loc->expiresByType, mime, ITOP(when));
            mime = stok(0, " \t\r\n", &tok);
        }
    }
}


void httpClearStages(HttpLoc *loc, int direction)
{
    if (direction & HTTP_STAGE_RX) {
        loc->inputStages = mprCreateList(-1, 0);
    }
    if (direction & HTTP_STAGE_TX) {
        loc->outputStages = mprCreateList(-1, 0);
    }
}


/* 
   Set the network connector
 */
int httpSetConnector(HttpLoc *loc, cchar *name)
{
    HttpStage     *stage;

    mprAssert(loc);
    
    stage = httpLookupStage(loc->http, name);
    if (stage == 0) {
        mprError("Can't find connector %s", name); 
        return MPR_ERR_CANT_FIND;
    }
    loc->connector = stage;
    mprLog(MPR_CONFIG, "Set connector \"%s\"", name);
    return 0;
}


void httpResetPipeline(HttpLoc *loc)
{
    if (loc->parent == 0) {
        loc->errorDocuments = mprCreateHash(HTTP_SMALL_HASH_SIZE, 0);
        loc->expires = mprCreateHash(0, MPR_HASH_STATIC_VALUES);
        loc->extensions = mprCreateHash(0, MPR_HASH_CASELESS);
        loc->handlers = mprCreateList(-1, 0);
        loc->inputStages = mprCreateList(-1, 0);
        loc->inputStages = mprCreateList(-1, 0);
    }
    loc->outputStages = mprCreateList(-1, 0);
}


void httpResetHandlers(HttpLoc *loc)
{
    graduate(loc);
    loc->handlers = mprCreateList(-1, 0);
}


HttpStage *httpGetHandlerByExtension(HttpLoc *loc, cchar *ext)
{
    return (HttpStage*) mprLookupKey(loc->extensions, ext);
}


void httpSetLocationAlias(HttpLoc *loc, HttpAlias *alias)
{
    mprAssert(loc);
    mprAssert(alias);

    loc->alias = alias;
}


void httpSetLocationAuth(HttpLoc *loc, HttpAuth *auth)
{
    loc->auth = auth;
}


void httpSetLocationPrefix(HttpLoc *loc, cchar *uri)
{
    mprAssert(loc);

    loc->prefix = sclone(uri);
    loc->prefixLen = (int) strlen(loc->prefix);
}


void httpSetLocationFlags(HttpLoc *loc, int flags)
{
    loc->flags = flags;
}


void httpSetLocationAutoDelete(HttpLoc *loc, int enable)
{
    loc->autoDelete = enable;
}


void httpSetLocationScript(HttpLoc *loc, cchar *script, cchar *scriptPath)
{
    if (script) {
        loc->script = sclone(script);
    }
    if (scriptPath) {
        loc->scriptPath = sclone(scriptPath);
    }
}


void httpSetLocationWorkers(HttpLoc *loc, int workers)
{
    loc->workers = workers;
}


void httpAddErrorDocument(HttpLoc *loc, cchar *code, cchar *url)
{
    graduate(loc);
    mprAddKey(loc->errorDocuments, code, sclone(url));
}


cchar *httpLookupErrorDocument(HttpLoc *loc, int code)
{
    char        numBuf[16];

    if (loc->errorDocuments == 0) {
        return 0;
    }
    itos(numBuf, sizeof(numBuf), code, 10);
    return (cchar*) mprLookupKey(loc->errorDocuments, numBuf);
}


void httpSetLocationData(HttpLoc *loc, cchar *key, void *data)
{
    if (loc->data == 0) {
        loc->data = mprCreateHash(-1, 0);
    }
    mprAddKey(loc->data, key, data);
}


void *httpGetLocationData(HttpLoc *loc, cchar *key)
{
    if (!loc->data) {
        return 0;
    }
    return mprLookupKey(loc->data, key);
}


static void defineKeywords(HttpLoc *loc)
{
    mprAddKey(loc->keywords, "PRODUCT", BLD_PRODUCT);
    mprAddKey(loc->keywords, "OS", BLD_OS);
    mprAddKey(loc->keywords, "VERSION", BLD_VERSION);
    mprAddKey(loc->keywords, "DOCUMENT_ROOT", 0);
    mprAddKey(loc->keywords, "SERVER_ROOT", 0);
    mprAddKey(loc->keywords, "DOCUMENT_ROOT", 0);
}


void httpAddLocationKey(HttpLoc *loc, cchar *key, cchar *value)
{
    mprAssert(key);
    mprAssert(value);
    mprAssert(MPR->httpService);

    mprAddKey(loc->keywords, key, value);
}


/*
    Make a path name. This replaces $references, converts to an absolute path name, cleans the path and maps delimiters.
 */
char *httpMakePath(HttpLoc *loc, cchar *file)
{
    HttpHost    *host;
    char        *result, *path;

    mprAssert(file);
    host = loc->host;

    if ((path = httpReplaceReferences(loc, file)) == 0) {
        /*  Overflow */
        return 0;
    }
    if (mprIsRelPath(path)) {
        result = mprJoinPath(host->serverRoot, path);
    } else {
        result = mprGetAbsPath(path);
    }
    return result;
}


/*
    Replace a limited set of $VAR references. Currently support DOCUMENT_ROOT, SERVER_ROOT and PRODUCT, OS and VERSION.
 */
char *httpReplaceReferences(HttpLoc *loc, cchar *str)
{
    Http    *http;
    MprBuf  *buf;
    char    *src, *result, *value, *cp, *tok;

    http = MPR->httpService;
    buf = mprCreateBuf(0, 0);
    if (str) {
        for (src = (char*) str; *src; ) {
            if (*src == '$') {
                ++src;
                for (cp = src; *cp && !isspace((int) *cp); cp++) ;
                tok = snclone(src, cp - src);
                if ((value = mprLookupKey(loc->keywords, src)) != 0) {
                    if (value == 0) {
                        if (scmp(tok, "DOCUMENT_ROOT")) {
                            mprPutStringToBuf(buf, loc->host->documentRoot);
                            src = cp;
                            continue;
                        } else if (scmp(tok, "SERVER_ROOT")) {
                            mprPutStringToBuf(buf, loc->host->serverRoot);
                            src = cp;
                            continue;
                        }
                    } else {
                        mprPutStringToBuf(buf, value);
                        src = cp;
                        continue;
                    }
                }
            }
            mprPutCharToBuf(buf, *src++);
        }
    }
    mprAddNullToBuf(buf);
    result = sclone(mprGetBufStart(buf));
    return result;
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
