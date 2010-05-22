/*
    location.c -- Server configuration for portions of the server (Location blocks).

    Location directives provide authorization and handler matching based on URL prefixes.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/************************************ Code ************************************/

HttpLocation *httpCreateLocation(Http *http)
{
    HttpLocation  *location;

    location = mprAllocObjZeroed(http, HttpLocation);
    if (location == 0) {
        return 0;
    }
    location->http = http;
    location->errorDocuments = mprCreateHash(location, HTTP_SMALL_HASH_SIZE);
    location->handlers = mprCreateList(location);
    location->extensions = mprCreateHash(location, HTTP_SMALL_HASH_SIZE);
    location->inputStages = mprCreateList(location);
    location->outputStages = mprCreateList(location);
    location->prefix = mprStrdup(location, "");
    location->prefixLen = (int) strlen(location->prefix);
    location->auth = httpCreateAuth(location, 0);
    return location;
}


/*  
    Create a new location block. Inherit from the parent. We use a copy-on-write scheme if these are modified later.
 */
HttpLocation *httpCreateInheritedLocation(Http *http, HttpLocation *parent)
{
    HttpLocation  *location;

    if (parent == 0) {
        return httpCreateLocation(http);
    }
    location = mprAllocObjZeroed(http, HttpLocation);
    if (location == 0) {
        return 0;
    }
    location->http = http;
    location->prefix = mprStrdup(location, parent->prefix);
    location->parent = parent;
    location->prefixLen = parent->prefixLen;
    location->flags = parent->flags;
    location->inputStages = parent->inputStages;
    location->outputStages = parent->outputStages;
    location->handlers = parent->handlers;
    location->extensions = parent->extensions;
    location->connector = parent->connector;
    location->errorDocuments = parent->errorDocuments;
    location->sessionTimeout = parent->sessionTimeout;
    location->auth = httpCreateAuth(location, parent->auth);
    location->uploadDir = parent->uploadDir;
    location->autoDelete = parent->autoDelete;
    location->script = parent->script;
    location->searchPath = parent->searchPath;
    location->ssl = parent->ssl;
    return location;
}


void httpFinalizeLocation(HttpLocation *location)
{
#if BLD_FEATURE_SSL
    if (location->ssl) {
        mprConfigureSsl(location->ssl);
    }
#endif
}


void httpSetLocationAuth(HttpLocation *location, HttpAuth *auth)
{
    location->auth = auth;
}


/*  
    Add a handler. This adds a handler to the set of possible handlers for a set of file extensions.
 */
int httpAddHandler(HttpLocation *location, cchar *name, cchar *extensions)
{
    Http        *http;
    HttpStage   *handler;
    char        *extlist, *word, *tok;

    mprAssert(location);

    http = location->http;
    if (mprGetParent(location->handlers) == location->parent) {
        location->extensions = mprCopyHash(location, location->parent->extensions);
        location->handlers = mprDupList(location, location->parent->handlers);
    }
    handler = httpLookupStage(http, name);
    if (handler == 0) {
        mprError(location, "Can't find stage %s", name); 
        return MPR_ERR_NOT_FOUND;
    }
    if (extensions && *extensions) {
        mprLog(location, MPR_CONFIG, "Add handler \"%s\" for \"%s\"", name, extensions);
    } else {
        mprLog(location, MPR_CONFIG, "Add handler \"%s\" for \"%s\"", name, location->prefix);
    }
    if (extensions && *extensions) {
        /*
            Add to the handler extension hash. Skip over "*." and "."
         */ 
        extlist = mprStrdup(location, extensions);
        word = mprStrTok(extlist, " \t\r\n", &tok);
        while (word) {
            if (*word == '*' && word[1] == '.') {
                word += 2;
            } else if (*word == '.') {
                word++;
            } else if (*word == '\"' && word[1] == '\"') {
                word = "";
            }
            mprAddHash(location->extensions, word, handler);
            word = mprStrTok(0, " \t\r\n", &tok);
        }
        mprFree(extlist);

    } else {
        if (handler->match == 0) {
            /*
                If a handler provides a custom match() routine, then don't match by extension.
             */
            mprAddHash(location->extensions, "", handler);
        }
        mprAddItem(location->handlers, handler);
    }
    return 0;
}


/*  
    Set a handler to universally apply to requests in this location block.
 */
int httpSetHandler(HttpLocation *location, cchar *name)
{
    HttpStage     *handler;

    mprAssert(location);
    
    if (mprGetParent(location->handlers) == location->parent) {
        location->extensions = mprCopyHash(location, location->parent->extensions);
        location->handlers = mprDupList(location, location->parent->handlers);
    }
    handler = httpLookupStage(location->http, name);
    if (handler == 0) {
        mprError(location, "Can't find handler %s", name); 
        return MPR_ERR_NOT_FOUND;
    }
    location->handler = handler;
    return 0;
}


/*  
    Add a filter. Direction defines what direction the stage filter be defined.
 */
int httpAddFilter(HttpLocation *location, cchar *name, cchar *extensions, int direction)
{
    HttpStage   *stage;
    HttpStage   *filter;
    char        *extlist, *word, *tok;

    mprAssert(location);
    
    stage = httpLookupStage(location->http, name);
    if (stage == 0) {
        mprError(location, "Can't find filter %s", name); 
        return MPR_ERR_NOT_FOUND;
    }
    /*
        Clone an existing stage because each filter stores its own set of extensions to match against
     */
    filter = httpCloneStage(location->http, stage);

    if (extensions && *extensions) {
        filter->extensions = mprCreateHash(filter, 0);
        extlist = mprStrdup(location, extensions);
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
        if (mprGetParent(location->inputStages) == location->parent) {
            location->inputStages = mprDupList(location, location->parent->inputStages);
        }
        mprAddItem(location->inputStages, filter);
    }
    if (direction & HTTP_STAGE_OUTGOING) {
        if (mprGetParent(location->outputStages) == location->parent) {
            location->outputStages = mprDupList(location, location->parent->outputStages);
        }
        mprAddItem(location->outputStages, filter);
    }
    return 0;
}


/* 
   Set the network connector
 */
int httpSetConnector(HttpLocation *location, cchar *name)
{
    HttpStage     *stage;

    mprAssert(location);
    
    stage = httpLookupStage(location->http, name);
    if (stage == 0) {
        mprError(location, "Can't find connector %s", name); 
        return MPR_ERR_NOT_FOUND;
    }
    location->connector = stage;
    mprLog(location, MPR_CONFIG, "Set connector \"%s\"", name);
    return 0;
}


void httpResetPipeline(HttpLocation *location)
{
    if (mprGetParent(location->extensions) == location) {
        mprFree(location->extensions);
    }
    location->extensions = mprCreateHash(location, 0);
    
    if (mprGetParent(location->handlers) == location) {
        mprFree(location->handlers);
    }
    location->handlers = mprCreateList(location);
    
    if (mprGetParent(location->inputStages) == location) {
        mprFree(location->inputStages);
    }
    location->inputStages = mprCreateList(location);
    
    if (mprGetParent(location->outputStages) == location) {
        mprFree(location->outputStages);
    }
    location->outputStages = mprCreateList(location);
}


HttpStage *httpGetHandlerByExtension(HttpLocation *location, cchar *ext)
{
    return (HttpStage*) mprLookupHash(location->extensions, ext);
}


void httpSetLocationPrefix(HttpLocation *location, cchar *uri)
{
    mprAssert(location);

    mprFree(location->prefix);
    location->prefix = mprStrdup(location, uri);
    location->prefixLen = (int) strlen(location->prefix);

    /*
        Always strip trailing "/". Note this is a Uri and not a path.
     */
    if (location->prefixLen > 0 && location->prefix[location->prefixLen - 1] == '/') {
        location->prefix[--location->prefixLen] = '\0';
    }
}


void httpSetLocationFlags(HttpLocation *location, int flags)
{
    location->flags = flags;
}


void httpSetLocationAutoDelete(HttpLocation *location, int enable)
{
    location->autoDelete = enable;
}


void httpSetLocationScript(HttpLocation *location, cchar *script)
{
    mprFree(location->script);
    location->script = mprStrdup(location, script);
}


void httpAddErrorDocument(HttpLocation *location, cchar *code, cchar *url)
{
    if (mprGetParent(location->errorDocuments) == location->parent) {
        location->errorDocuments = mprCopyHash(location, location->parent->errorDocuments);
    }
    mprAddHash(location->errorDocuments, code, mprStrdup(location, url));
}


cchar *httpLookupErrorDocument(HttpLocation *location, int code)
{
    char        numBuf[16];

    if (location->errorDocuments == 0) {
        return 0;
    }
    mprItoa(numBuf, sizeof(numBuf), code, 10);
    return (cchar*) mprLookupHash(location->errorDocuments, numBuf);
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
