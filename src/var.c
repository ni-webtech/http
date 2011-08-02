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
void httpCreateCGIVars(HttpConn *conn)
{
    HttpRx          *rx;
    HttpTx          *tx;
    HttpHost        *host;
    HttpUploadFile  *up;
    MprSocket       *sock;
    MprHashTable    *table;
    MprHash         *hp;
    int             index;

    rx = conn->rx;
    tx = conn->tx;
    host = conn->host;
    sock = conn->sock;

    if ((table = rx->formVars) == 0) {
        table = rx->formVars = mprCreateHash(HTTP_MED_HASH_SIZE, 0);
    }
    mprAddKey(table, "AUTH_TYPE", rx->authType);
    mprAddKey(table, "AUTH_USER", conn->authUser);
    mprAddKey(table, "AUTH_GROUP", conn->authGroup);
    mprAddKey(table, "AUTH_ACL", MPR->emptyString);
    mprAddKey(table, "CONTENT_LENGTH", rx->contentLength);
    mprAddKey(table, "CONTENT_TYPE", rx->mimeType);
    mprAddKey(table, "DOCUMENT_ROOT", host->documentRoot);
    mprAddKey(table, "GATEWAY_INTERFACE", sclone("CGI/1.1"));
    mprAddKey(table, "QUERY_STRING", rx->parsedUri->query);
    mprAddKey(table, "REMOTE_ADDR", conn->ip);
    mprAddKeyFmt(table, "REMOTE_PORT", "%d", conn->port);
    mprAddKey(table, "REMOTE_USER", conn->authUser);
    mprAddKey(table, "REQUEST_METHOD", rx->method);
    mprAddKey(table, "REQUEST_TRANSPORT", sclone((char*) ((conn->secure) ? "https" : "http")));
    mprAddKey(table, "SERVER_ADDR", sock->acceptIp);
    mprAddKey(table, "SERVER_NAME", host->name);
    mprAddKeyFmt(table, "SERVER_PORT", "%d", sock->acceptPort);
    mprAddKey(table, "SERVER_PROTOCOL", conn->protocol);
    mprAddKey(table, "SERVER_ROOT", host->serverRoot);
    mprAddKey(table, "SERVER_SOFTWARE", conn->http->software);
    mprAddKey(table, "REQUEST_URI", rx->originalUri);
    /*  
        URIs are broken into the following: http://{SERVER_NAME}:{SERVER_PORT}{SCRIPT_NAME}{PATH_INFO} 
        NOTE: For CGI|PHP, scriptName is empty and pathInfo has the script. PATH_INFO is stored in extraPath.
     */
    mprAddKey(table, "PATH_INFO", rx->extraPath);
    mprAddKey(table, "SCRIPT_NAME", rx->pathInfo);
    mprAddKey(table, "SCRIPT_FILENAME", tx->filename);
    if (rx->extraPath) {
        /*  
            Only set PATH_TRANSLATED if extraPath is set (CGI spec) 
         */
        mprAddKey(table, "PATH_TRANSLATED", httpMakeFilename(conn, rx->alias, rx->extraPath, 0));
    }
    if (rx->files) {
        for (index = 0, hp = 0; (hp = mprGetNextKey(conn->rx->files, hp)) != 0; index++) {
            up = (HttpUploadFile*) hp->data;
            mprAddKey(table, mprAsprintf("FILE_%d_FILENAME", index), up->filename);
            mprAddKey(table, mprAsprintf("FILE_%d_CLIENT_FILENAME", index), up->clientFilename);
            mprAddKey(table, mprAsprintf("FILE_%d_CONTENT_TYPE", index), up->contentType);
            mprAddKey(table, mprAsprintf("FILE_%d_NAME", index), hp->key);
            mprAddKeyFmt(table, mprAsprintf("FILE_%d_SIZE", index), "%d", up->size);
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
MprHashTable *httpAddVars(MprHashTable *table, cchar *buf, ssize len)
{
    cchar   *oldValue;
    char    *newValue, *decoded, *keyword, *value, *tok;

    if (table == 0) {
        table = mprCreateHash(HTTP_MED_HASH_SIZE, 0);
    }
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
            oldValue = mprLookupKey(table, keyword);
            if (oldValue != 0 && *oldValue) {
                if (*value) {
                    newValue = sjoin(oldValue, " ", value, NULL);
                    mprAddKey(table, keyword, newValue);
                }
            } else {
                mprAddKey(table, keyword, sclone(value));
            }
        }
        keyword = stok(0, "&", &tok);
    }
    return table;
}


MprHashTable *httpAddVarsFromQueue(MprHashTable *table, HttpQueue *q)
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
        table = httpAddVars(table, mprGetBufStart(content), mprGetBufLength(content));
    }
    return table;
}


void httpAddFormVars(HttpConn *conn)
{
    HttpRx      *rx;

    rx = conn->rx;
    if (rx->form && rx->formVars == 0) {
        rx->formVars = httpAddVarsFromQueue(rx->formVars, conn->readq);
    }
}


int httpTestFormVar(HttpConn *conn, cchar *var)
{
    MprHashTable    *vars;
    
    if ((vars = conn->rx->formVars) == 0) {
        return 0;
    }
    return vars && mprLookupKey(vars, var) != 0;
}


cchar *httpGetFormVar(HttpConn *conn, cchar *var, cchar *defaultValue)
{
    MprHashTable    *vars;
    cchar           *value;
    
    if ((vars = conn->rx->formVars) == 0) {
        value = mprLookupKey(vars, var);
        return (value) ? value : defaultValue;
    }
    return defaultValue;
}


int httpGetIntFormVar(HttpConn *conn, cchar *var, int defaultValue)
{
    MprHashTable    *vars;
    cchar           *value;
    
    if ((vars = conn->rx->formVars) == 0) {
        value = mprLookupKey(vars, var);
        return (value) ? (int) stoi(value, 10, NULL) : defaultValue;
    }
    return defaultValue;
}


void httpSetFormVar(HttpConn *conn, cchar *var, cchar *value) 
{
    MprHashTable    *vars;
    
    if ((vars = conn->rx->formVars) == 0) {
        /* This is allowed. Upload filter uses this when uploading to the file handler */
        return;
    }
    mprAddKey(vars, var, sclone(value));
}


void httpSetIntFormVar(HttpConn *conn, cchar *var, int value) 
{
    MprHashTable    *vars;
    
    if ((vars = conn->rx->formVars) == 0) {
        /* This is allowed. Upload filter uses this when uploading to the file handler */
        return;
    }
    mprAddKey(vars, var, mprAsprintf("%d", value));
}


int httpCompareFormVar(HttpConn *conn, cchar *var, cchar *value)
{
    MprHashTable    *vars;
    
    if ((vars = conn->rx->formVars) == 0) {
        return 0;
    }
    if (strcmp(value, httpGetFormVar(conn, var, " __UNDEF__ ")) == 0) {
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
