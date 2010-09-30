/*
    location.c -- Server configuration for portions of the server (Location blocks).

    Location directives provide authorization and handler matching based on URL prefixes.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/************************************ Code ************************************/

HttpLoc *httpCreateLocation(Http *http)
{
    HttpLoc  *loc;

    if ((loc = mprAllocObj(http, HttpLoc, NULL)) == 0) {
        return 0;
    }
    loc->http = http;
    loc->errorDocuments = mprCreateHash(loc, HTTP_SMALL_HASH_SIZE);
    loc->handlers = mprCreateList(loc);
    loc->extensions = mprCreateHash(loc, HTTP_SMALL_HASH_SIZE);
    loc->inputStages = mprCreateList(loc);
    loc->outputStages = mprCreateList(loc);
    loc->prefix = mprStrdup(loc, "");
    loc->prefixLen = (int) strlen(loc->prefix);
    loc->auth = httpCreateAuth(loc, 0);
    return loc;
}


/*  
    Create a new location block. Inherit from the parent. We use a copy-on-write scheme if these are modified later.
 */
HttpLoc *httpCreateInheritedLocation(Http *http, HttpLoc *parent)
{
    HttpLoc  *loc;

    if (parent == 0) {
        return httpCreateLocation(http);
    }
    if ((loc = mprAllocObj(http, HttpLoc, NULL)) == 0) {
        return 0;
    }
    loc->http = http;
    loc->prefix = mprStrdup(loc, parent->prefix);
    loc->parent = parent;
    loc->prefixLen = parent->prefixLen;
    loc->flags = parent->flags;
    loc->inputStages = parent->inputStages;
    loc->outputStages = parent->outputStages;
    loc->handlers = parent->handlers;
    loc->extensions = parent->extensions;
    loc->connector = parent->connector;
    loc->errorDocuments = parent->errorDocuments;
    loc->sessionTimeout = parent->sessionTimeout;
    loc->auth = httpCreateAuth(loc, parent->auth);
    loc->uploadDir = parent->uploadDir;
    loc->autoDelete = parent->autoDelete;
    loc->script = parent->script;
    loc->searchPath = parent->searchPath;
    loc->ssl = parent->ssl;
    return loc;
}


void httpFinalizeLocation(HttpLoc *loc)
{
#if BLD_FEATURE_SSL
    if (loc->ssl) {
        mprConfigureSsl(loc->ssl);
    }
#endif
}


void httpSetLocationAuth(HttpLoc *loc, HttpAuth *auth)
{
    loc->auth = auth;
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
    if (mprIsParent(loc->parent, loc->handlers)) {
        loc->extensions = mprCopyHash(loc, loc->parent->extensions);
        loc->handlers = mprDupList(loc, loc->parent->handlers);
    }
    handler = httpLookupStage(http, name);
    if (handler == 0) {
        mprError(loc, "Can't find stage %s", name); 
        return MPR_ERR_NOT_FOUND;
    }
    if (extensions && *extensions) {
        mprLog(loc, MPR_CONFIG, "Add handler \"%s\" for \"%s\"", name, extensions);
    } else {
        mprLog(loc, MPR_CONFIG, "Add handler \"%s\" for \"%s\"", name, loc->prefix);
    }
    if (extensions && *extensions) {
        /*
            Add to the handler extension hash. Skip over "*." and "."
         */ 
        extlist = mprStrdup(loc, extensions);
        word = mprStrTok(extlist, " \t\r\n", &tok);
        while (word) {
            if (*word == '*' && word[1] == '.') {
                word += 2;
            } else if (*word == '.') {
                word++;
            } else if (*word == '\"' && word[1] == '\"') {
                word = "";
            }
            mprAddHash(loc->extensions, word, handler);
            word = mprStrTok(0, " \t\r\n", &tok);
        }
        mprFree(extlist);

    } else {
        if (handler->match == 0) {
            /*
                If a handler provides a custom match() routine, then don't match by extension.
             */
            mprAddHash(loc->extensions, "", handler);
        }
        mprAddItem(loc->handlers, handler);
    }
    return 0;
}


/*  
    Set a handler to universally apply to requests in this location block.
 */
int httpSetHandler(HttpLoc *loc, cchar *name)
{
    HttpStage     *handler;

    mprAssert(loc);
    
    if (mprIsParent(loc->parent, loc->handlers)) {
        loc->extensions = mprCopyHash(loc, loc->parent->extensions);
        loc->handlers = mprDupList(loc, loc->parent->handlers);
    }
    handler = httpLookupStage(loc->http, name);
    if (handler == 0) {
        mprError(loc, "Can't find handler %s", name); 
        return MPR_ERR_NOT_FOUND;
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
        mprError(loc, "Can't find filter %s", name); 
        return MPR_ERR_NOT_FOUND;
    }
    /*
        Clone an existing stage because each filter stores its own set of extensions to match against
     */
    filter = httpCloneStage(loc->http, stage);

    if (extensions && *extensions) {
        filter->extensions = mprCreateHash(filter, 0);
        extlist = mprStrdup(loc, extensions);
        word = mprStrTok(extlist, " \t\r\n", &tok);
        while (word) {
            if (*word == '*' && word[1] == '.') {
                word += 2;
            } else if (*word == '.') {
                word++;
            } else if (*word == '\"' && word[1] == '\"') {
                word = "";
            }
            mprAddHash(filter->extensions, word, filter);
            word = mprStrTok(0, " \t\r\n", &tok);
        }
        mprFree(extlist);
    }

    if (direction & HTTP_STAGE_INCOMING) {
        if (mprIsParent(loc->parent, loc->inputStages)) {
            loc->inputStages = mprDupList(loc, loc->parent->inputStages);
        }
        mprAddItem(loc->inputStages, filter);
    }
    if (direction & HTTP_STAGE_OUTGOING) {
        if (mprIsParent(loc->parent, loc->outputStages)) {
            loc->outputStages = mprDupList(loc, loc->parent->outputStages);
        }
        mprAddItem(loc->outputStages, filter);
    }
    return 0;
}


void httpClearStages(HttpLoc *loc, int direction)
{
    if (direction & HTTP_STAGE_INCOMING) {
        loc->inputStages = mprCreateList(loc);
    }
    if (direction & HTTP_STAGE_OUTGOING) {
        loc->outputStages = mprCreateList(loc);
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
        mprError(loc, "Can't find connector %s", name); 
        return MPR_ERR_NOT_FOUND;
    }
    loc->connector = stage;
    mprLog(loc, MPR_CONFIG, "Set connector \"%s\"", name);
    return 0;
}


void httpResetPipeline(HttpLoc *loc)
{
    if (mprIsParent(loc, loc->extensions)) {
        mprFree(loc->extensions);
    }
    loc->extensions = mprCreateHash(loc, 0);
    
    if (mprIsParent(loc, loc->handlers)) {
        mprFree(loc->handlers);
    }
    loc->handlers = mprCreateList(loc);
    
    if (mprIsParent(loc, loc->inputStages)) {
        mprFree(loc->inputStages);
    }
    loc->inputStages = mprCreateList(loc);
    
    if (mprIsParent(loc, loc->outputStages)) {
        mprFree(loc->outputStages);
    }
    loc->outputStages = mprCreateList(loc);
}


HttpStage *httpGetHandlerByExtension(HttpLoc *loc, cchar *ext)
{
    return (HttpStage*) mprLookupHash(loc->extensions, ext);
}


void httpSetLocationPrefix(HttpLoc *loc, cchar *uri)
{
    mprAssert(loc);

    mprFree(loc->prefix);
    loc->prefix = mprStrdup(loc, uri);
    loc->prefixLen = (int) strlen(loc->prefix);

    /*
        Always strip trailing "/". Note this is a Uri and not a path.
     */
    if (loc->prefixLen > 0 && loc->prefix[loc->prefixLen - 1] == '/') {
        loc->prefix[--loc->prefixLen] = '\0';
    }
}


void httpSetLocationFlags(HttpLoc *loc, int flags)
{
    loc->flags = flags;
}


void httpSetLocationAutoDelete(HttpLoc *loc, int enable)
{
    loc->autoDelete = enable;
}


void httpSetLocationScript(HttpLoc *loc, cchar *script)
{
    mprFree(loc->script);
    loc->script = mprStrdup(loc, script);
}


void httpAddErrorDocument(HttpLoc *loc, cchar *code, cchar *url)
{
    if (mprIsParent(loc->parent, loc->errorDocuments)) {
        loc->errorDocuments = mprCopyHash(loc, loc->parent->errorDocuments);
    }
    mprAddHash(loc->errorDocuments, code, mprStrdup(loc, url));
}


cchar *httpLookupErrorDocument(HttpLoc *loc, int code)
{
    char        numBuf[16];

    if (loc->errorDocuments == 0) {
        return 0;
    }
    mprItoa(numBuf, sizeof(numBuf), code, 10);
    return (cchar*) mprLookupHash(loc->errorDocuments, numBuf);
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
