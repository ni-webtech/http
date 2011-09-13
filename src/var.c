/*
    var.c -- Manage the request variables
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/*********************************** Code *************************************/
/*
    Define standard CGI variables
 */
void httpCreateCGIParams(HttpConn *conn)
{
    HttpRx          *rx;
    HttpTx          *tx;
    HttpHost        *host;
    HttpUploadFile  *up;
    MprSocket       *sock;
    MprHashTable    *vars;
    MprHash         *hp;
    int             index;

    rx = conn->rx;
    tx = conn->tx;
    host = conn->host;
    sock = conn->sock;
    vars = httpGetParams(conn);
    mprAssert(vars);

    mprAddKey(vars, "AUTH_TYPE", rx->authType);
    mprAddKey(vars, "AUTH_USER", conn->authUser);
#if UNUSED
    mprAddKey(vars, "AUTH_GROUP", conn->authGroup);
#endif
    mprAddKey(vars, "AUTH_ACL", MPR->emptyString);
    mprAddKey(vars, "CONTENT_LENGTH", rx->contentLength);
    mprAddKey(vars, "CONTENT_TYPE", rx->mimeType);
    mprAddKey(vars, "DOCUMENT_ROOT", rx->route->dir);
    mprAddKey(vars, "GATEWAY_INTERFACE", sclone("CGI/1.1"));
    mprAddKey(vars, "QUERY_STRING", rx->parsedUri->query);
    mprAddKey(vars, "REMOTE_ADDR", conn->ip);
    mprAddKeyFmt(vars, "REMOTE_PORT", "%d", conn->port);
    mprAddKey(vars, "REMOTE_USER", conn->authUser);
    mprAddKey(vars, "REQUEST_METHOD", rx->method);
    mprAddKey(vars, "REQUEST_TRANSPORT", sclone((char*) ((conn->secure) ? "https" : "http")));
    mprAddKey(vars, "SERVER_ADDR", sock->acceptIp);
    mprAddKey(vars, "SERVER_NAME", host->name);
    mprAddKeyFmt(vars, "SERVER_PORT", "%d", sock->acceptPort);
    mprAddKey(vars, "SERVER_PROTOCOL", conn->protocol);
    mprAddKey(vars, "SERVER_ROOT", host->home);
    mprAddKey(vars, "SERVER_SOFTWARE", conn->http->software);
    mprAddKey(vars, "REQUEST_URI", rx->originalUri);
    /*  
        URIs are broken into the following: http://{SERVER_NAME}:{SERVER_PORT}{SCRIPT_NAME}{PATH_INFO} 
        NOTE: Appweb refers to pathInfo as the app relative URI and scriptName as the app address before the pathInfo.
        In CGI|PHP terms, the scriptName is the appweb rx->pathInfo and the PATH_INFO is the extraPath. 
     */
    mprAddKey(vars, "PATH_INFO", rx->extraPath);
    mprAddKey(vars, "SCRIPT_NAME", rx->pathInfo);
    mprAddKey(vars, "SCRIPT_FILENAME", tx->filename);
    if (rx->extraPath) {
        /*  
            Only set PATH_TRANSLATED if extraPath is set (CGI spec) 
         */
        mprAssert(rx->extraPath[0] == '/');
        mprAddKey(vars, "PATH_TRANSLATED", mprGetNormalizedPath(sfmt("%s%s", rx->route->dir, rx->extraPath)));
    }
    if (rx->files) {
        for (index = 0, hp = 0; (hp = mprGetNextKey(conn->rx->files, hp)) != 0; index++) {
            up = (HttpUploadFile*) hp->data;
            mprAddKey(vars, sfmt("FILE_%d_FILENAME", index), up->filename);
            mprAddKey(vars, sfmt("FILE_%d_CLIENT_FILENAME", index), up->clientFilename);
            mprAddKey(vars, sfmt("FILE_%d_CONTENT_TYPE", index), up->contentType);
            mprAddKey(vars, sfmt("FILE_%d_NAME", index), hp->key);
            mprAddKeyFmt(vars, sfmt("FILE_%d_SIZE", index), "%d", up->size);
        }
    }
    if (conn->http->envCallback) {
        conn->http->envCallback(conn);
    }
}


/*  
    Add variables to the vars environment store. This comes from the query string and urlencoded post data.
    Make variables for each keyword in a query string. The buffer must be url encoded (ie. key=value&key2=value2..., 
    spaces converted to '+' and all else should be %HEX encoded).
 */
//  MOB - rename and remove http
static void httpAddParamsFromBuf(HttpConn *conn, cchar *buf, ssize len)
{
    MprHashTable    *vars;
    cchar           *oldValue;
    char            *newValue, *decoded, *keyword, *value, *tok;

    mprAssert(conn);
    vars = httpGetParams(conn);
    decoded = mprAlloc(len + 1);
    decoded[len] = '\0';
    memcpy(decoded, buf, len);

    keyword = stok(decoded, "&", &tok);
    while (keyword != 0) {
        if ((value = strchr(keyword, '=')) != 0) {
            *value++ = '\0';
            value = mprUriDecode(value);
        } else {
            value = "";
        }
        keyword = mprUriDecode(keyword);

        if (*keyword) {
            /*  
                Append to existing keywords
             */
            oldValue = mprLookupKey(vars, keyword);
            if (oldValue != 0 && *oldValue) {
                if (*value) {
                    newValue = sjoin(oldValue, " ", value, NULL);
                    mprAddKey(vars, keyword, newValue);
                }
            } else {
                mprAddKey(vars, keyword, sclone(value));
            }
        }
        keyword = stok(0, "&", &tok);
    }
}


//  MOB - rename and remove http
static void httpAddParamsFromQueue(HttpQueue *q)
{
    HttpConn    *conn;
    HttpRx      *rx;
    MprBuf      *content;

    mprAssert(q);
    
    conn = q->conn;
    rx = conn->rx;

    if ((rx->form || rx->upload) && q->first && q->first->content) {
        httpJoinPackets(q, -1);
        content = q->first->content;
        mprAddNullToBuf(content);
        mprLog(6, "Form body data: length %d, \"%s\"", mprGetBufLength(content), mprGetBufStart(content));
        httpAddParamsFromBuf(conn, mprGetBufStart(content), mprGetBufLength(content));
    }
}


//  MOB - rename and remove http
static void httpAddQueryParams(HttpConn *conn) 
{
    HttpRx      *rx;

    rx = conn->rx;
    if (rx->parsedUri->query && !(rx->flags & HTTP_ADDED_QUERY_PARAMS)) {
        httpAddParamsFromBuf(conn, rx->parsedUri->query, slen(rx->parsedUri->query));
        rx->flags |= HTTP_ADDED_QUERY_PARAMS;
    }
}


//  MOB - rename and remove http
static void httpAddBodyParams(HttpConn *conn)
{
    HttpRx      *rx;

    rx = conn->rx;
    if (rx->form) {
        if (!(rx->flags & HTTP_ADDED_FORM_PARAMS)) {
            conn->readq = conn->tx->queue[HTTP_QUEUE_RX]->prevQ;
            httpAddParamsFromQueue(conn->readq);
            rx->flags |= HTTP_ADDED_FORM_PARAMS;
        }
    }
}


void httpAddParams(HttpConn *conn)
{
    httpAddQueryParams(conn);
    httpAddBodyParams(conn);
}


MprHashTable *httpGetParams(HttpConn *conn)
{ 
    if (conn->rx->params == 0) {
        conn->rx->params = mprCreateHash(HTTP_MED_HASH_SIZE, 0);
    }
    return conn->rx->params;
}


int httpTestParam(HttpConn *conn, cchar *var)
{
    MprHashTable    *vars;
    
    vars = httpGetParams(conn);
    return vars && mprLookupKey(vars, var) != 0;
}


cchar *httpGetParam(HttpConn *conn, cchar *var, cchar *defaultValue)
{
    MprHashTable    *vars;
    cchar           *value;
    
    vars = httpGetParams(conn);
    value = mprLookupKey(vars, var);
    return (value) ? value : defaultValue;
}


int httpGetIntParam(HttpConn *conn, cchar *var, int defaultValue)
{
    MprHashTable    *vars;
    cchar           *value;
    
    vars = httpGetParams(conn);
    value = mprLookupKey(vars, var);
    return (value) ? (int) stoi(value, 10, NULL) : defaultValue;
}


void httpSetParam(HttpConn *conn, cchar *var, cchar *value) 
{
    MprHashTable    *vars;

    vars = httpGetParams(conn);
    mprAddKey(vars, var, sclone(value));
}


void httpSetIntParam(HttpConn *conn, cchar *var, int value) 
{
    MprHashTable    *vars;
    
    vars = httpGetParams(conn);
    mprAddKey(vars, var, sfmt("%d", value));
}


bool httpMatchParam(HttpConn *conn, cchar *var, cchar *value)
{
    MprHashTable    *vars;
    
    vars = httpGetParams(conn);
    if (strcmp(value, httpGetParam(conn, var, " __UNDEF__ ")) == 0) {
        return 1;
    }
    return 0;
}


void httpAddUploadFile(HttpConn *conn, cchar *id, HttpUploadFile *upfile)
{
    HttpRx   *rx;

    rx = conn->rx;
    if (rx->files == 0) {
        rx->files = mprCreateHash(-1, 0);
    }
    mprAddKey(rx->files, id, upfile);
}


void httpRemoveUploadFile(HttpConn *conn, cchar *id)
{
    HttpRx    *rx;
    HttpUploadFile  *upfile;

    rx = conn->rx;

    upfile = (HttpUploadFile*) mprLookupKey(rx->files, id);
    if (upfile) {
        mprDeletePath(upfile->filename);
        upfile->filename = 0;
    }
}


void httpRemoveAllUploadedFiles(HttpConn *conn)
{
    HttpRx          *rx;
    HttpUploadFile  *upfile;
    MprHash         *hp;

    rx = conn->rx;

    for (hp = 0; rx->files && (hp = mprGetNextKey(rx->files, hp)) != 0; ) {
        upfile = (HttpUploadFile*) hp->data;
        if (upfile->filename) {
            mprDeletePath(upfile->filename);
            upfile->filename = 0;
        }
    }
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
