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
    MprHash         *vars, *svars;
    MprKey          *kp;
    int             index;

    rx = conn->rx;
    if ((svars = rx->svars) != 0) {
        /* Do only once */
        return;
    }
    svars = rx->svars = mprCreateHash(HTTP_MED_HASH_SIZE, 0);
    tx = conn->tx;
    host = conn->host;
    sock = conn->sock;

    mprAddKey(svars, "AUTH_TYPE", rx->authType);
    mprAddKey(svars, "AUTH_USER", conn->authUser);
    mprAddKey(svars, "AUTH_ACL", MPR->emptyString);
    mprAddKey(svars, "CONTENT_LENGTH", rx->contentLength);
    mprAddKey(svars, "CONTENT_TYPE", rx->mimeType);
    mprAddKey(svars, "DOCUMENT_ROOT", rx->route->dir);
    mprAddKey(svars, "GATEWAY_INTERFACE", sclone("CGI/1.1"));
    mprAddKey(svars, "QUERY_STRING", rx->parsedUri->query);
    mprAddKey(svars, "REMOTE_ADDR", conn->ip);
    mprAddKeyFmt(svars, "REMOTE_PORT", "%d", conn->port);
    mprAddKey(svars, "REMOTE_USER", conn->authUser);
    mprAddKey(svars, "REQUEST_METHOD", rx->method);
    mprAddKey(svars, "REQUEST_TRANSPORT", sclone((char*) ((conn->secure) ? "https" : "http")));
    mprAddKey(svars, "SERVER_ADDR", sock->acceptIp);
    mprAddKey(svars, "SERVER_NAME", host->name);
    mprAddKeyFmt(svars, "SERVER_PORT", "%d", sock->acceptPort);
    mprAddKey(svars, "SERVER_PROTOCOL", conn->protocol);
    mprAddKey(svars, "SERVER_ROOT", host->home);
    mprAddKey(svars, "SERVER_SOFTWARE", conn->http->software);
    /*
        For PHP, REQUEST_URI must be the original URI. The SCRIPT_NAME will refer to the new pathInfo
     */
    mprAddKey(svars, "REQUEST_URI", rx->originalUri);
    /*  
        URIs are broken into the following: http://{SERVER_NAME}:{SERVER_PORT}{SCRIPT_NAME}{PATH_INFO} 
        NOTE: Appweb refers to pathInfo as the app relative URI and scriptName as the app address before the pathInfo.
        In CGI|PHP terms, the scriptName is the appweb rx->pathInfo and the PATH_INFO is the extraPath. 
     */
    mprAddKey(svars, "PATH_INFO", rx->extraPath);
    mprAddKeyFmt(svars, "SCRIPT_NAME", "%s%s", rx->scriptName, rx->pathInfo);
    mprAddKey(svars, "SCRIPT_FILENAME", tx->filename);
    if (rx->extraPath) {
        /*  
            Only set PATH_TRANSLATED if extraPath is set (CGI spec) 
         */
        mprAssert(rx->extraPath[0] == '/');
        mprAddKey(svars, "PATH_TRANSLATED", mprNormalizePath(sfmt("%s%s", rx->route->dir, rx->extraPath)));
    }

    if (rx->files) {
        vars = httpGetParams(conn);
        mprAssert(vars);
        for (index = 0, kp = 0; (kp = mprGetNextKey(conn->rx->files, kp)) != 0; index++) {
            up = (HttpUploadFile*) kp->data;
            mprAddKey(vars, sfmt("FILE_%d_FILENAME", index), up->filename);
            mprAddKey(vars, sfmt("FILE_%d_CLIENT_FILENAME", index), up->clientFilename);
            mprAddKey(vars, sfmt("FILE_%d_CONTENT_TYPE", index), up->contentType);
            mprAddKey(vars, sfmt("FILE_%d_NAME", index), kp->key);
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
static void addParamsFromBuf(HttpConn *conn, cchar *buf, ssize len)
{
    MprHash     *vars;
    cchar       *oldValue;
    char        *newValue, *decoded, *keyword, *value, *tok;

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


static void addParamsFromQueue(HttpQueue *q)
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
        addParamsFromBuf(conn, mprGetBufStart(content), mprGetBufLength(content));
    }
}


static void addQueryParams(HttpConn *conn) 
{
    HttpRx      *rx;

    rx = conn->rx;
    if (rx->parsedUri->query && !(rx->flags & HTTP_ADDED_QUERY_PARAMS)) {
        addParamsFromBuf(conn, rx->parsedUri->query, slen(rx->parsedUri->query));
        rx->flags |= HTTP_ADDED_QUERY_PARAMS;
    }
}


static void addBodyParams(HttpConn *conn)
{
    HttpRx      *rx;

    rx = conn->rx;
    if (rx->form) {
        if (!(rx->flags & HTTP_ADDED_FORM_PARAMS)) {
            conn->readq = conn->tx->queue[HTTP_QUEUE_RX]->prevQ;
            addParamsFromQueue(conn->readq);
            rx->flags |= HTTP_ADDED_FORM_PARAMS;
        }
    }
}


void httpAddParams(HttpConn *conn)
{
    addQueryParams(conn);
    addBodyParams(conn);
}


MprHash *httpGetParams(HttpConn *conn)
{ 
    if (conn->rx->params == 0) {
        conn->rx->params = mprCreateHash(HTTP_MED_HASH_SIZE, 0);
    }
    return conn->rx->params;
}


int httpTestParam(HttpConn *conn, cchar *var)
{
    MprHash    *vars;
    
    vars = httpGetParams(conn);
    return vars && mprLookupKey(vars, var) != 0;
}


cchar *httpGetParam(HttpConn *conn, cchar *var, cchar *defaultValue)
{
    MprHash     *vars;
    cchar       *value;
    
    vars = httpGetParams(conn);
    value = mprLookupKey(vars, var);
    return (value) ? value : defaultValue;
}


int httpGetIntParam(HttpConn *conn, cchar *var, int defaultValue)
{
    MprHash     *vars;
    cchar       *value;
    
    vars = httpGetParams(conn);
    value = mprLookupKey(vars, var);
    return (value) ? (int) stoi(value) : defaultValue;
}


static int sortParam(MprKey **h1, MprKey **h2)
{
    return scmp((*h1)->key, (*h2)->key);
}


/*
    Return the request parameters as a string. 
    This will return the exact same string regardless of the order of form parameters.
 */
char *httpGetParamsString(HttpConn *conn)
{
    HttpRx      *rx;
    MprHash     *params;
    MprKey      *kp;
    MprList     *list;
    char        *buf, *cp;
    ssize       len;
    int         next;

    mprAssert(conn);

    rx = conn->rx;

    if (rx->paramString == 0) {
        if ((params = conn->rx->params) != 0) {
            if ((list = mprCreateList(mprGetHashLength(params), 0)) != 0) {
                len = 0;
                for (kp = 0; (kp = mprGetNextKey(params, kp)) != NULL; ) {
                    mprAddItem(list, kp);
                    len += slen(kp->key) + slen(kp->data) + 2;
                }
                if ((buf = mprAlloc(len + 1)) != 0) {
                    mprSortList(list, sortParam);
                    cp = buf;
                    for (next = 0; (kp = mprGetNextItem(list, &next)) != 0; ) {
                        strcpy(cp, kp->key); cp += slen(kp->key);
                        *cp++ = '=';
                        strcpy(cp, kp->data); cp += slen(kp->data);
                        *cp++ = '&';
                    }
                    cp[-1] = '\0';
                    rx->paramString = buf;
                }
            }
        }
    }
    return rx->paramString;
}


void httpSetParam(HttpConn *conn, cchar *var, cchar *value) 
{
    MprHash     *vars;

    vars = httpGetParams(conn);
    mprAddKey(vars, var, sclone(value));
}


void httpSetIntParam(HttpConn *conn, cchar *var, int value) 
{
    MprHash     *vars;
    
    vars = httpGetParams(conn);
    mprAddKey(vars, var, sfmt("%d", value));
}


bool httpMatchParam(HttpConn *conn, cchar *var, cchar *value)
{
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
    MprKey          *kp;

    rx = conn->rx;

    for (kp = 0; rx->files && (kp = mprGetNextKey(rx->files, kp)) != 0; ) {
        upfile = (HttpUploadFile*) kp->data;
        if (upfile->filename) {
            mprDeletePath(upfile->filename);
            upfile->filename = 0;
        }
    }
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
