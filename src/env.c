/*
    env.c -- Manage the request environment
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/*********************************** Code *************************************/
/*
    Define standard CGI environment variables
 */
void httpCreateEnvVars(HttpConn *conn)
{
    HttpRx          *rx;
    HttpTx          *tx;
    MprSocket       *sock;
    MprHashTable    *vars;
    MprHash         *hp;
    HttpUploadFile  *up;
    HttpServer      *server;
    char            port[16], size[16];
    int             index;

    rx = conn->rx;
    tx = conn->tx;
    vars = rx->formVars;
    server = conn->server;

    //  TODO - Vars for COOKIEs
    
    /*  Alias for REMOTE_USER. Define both for broader compatibility with CGI */
    mprAddHash(vars, "AUTH_TYPE", rx->authType);
    mprAddHash(vars, "AUTH_USER", (conn->authUser && *conn->authUser) ? conn->authUser : 0);
    mprAddHash(vars, "AUTH_GROUP", conn->authGroup);
    mprAddHash(vars, "AUTH_ACL", "");
    mprAddHash(vars, "CONTENT_LENGTH", rx->contentLength);
    mprAddHash(vars, "CONTENT_TYPE", rx->mimeType);
    mprAddHash(vars, "GATEWAY_INTERFACE", "CGI/1.1");
    mprAddHash(vars, "QUERY_STRING", rx->parsedUri->query);

    if (conn->sock) {
        mprAddHash(vars, "REMOTE_ADDR", conn->ip);
    }
    itos(port, sizeof(port) - 1, conn->port, 10);
    mprAddHash(vars, "REMOTE_PORT", sclone(port));

    /*  Same as AUTH_USER (yes this is right) */
    mprAddHash(vars, "REMOTE_USER", (conn->authUser && *conn->authUser) ? conn->authUser : 0);
    mprAddHash(vars, "REQUEST_METHOD", rx->method);
    mprAddHash(vars, "REQUEST_TRANSPORT", sclone((char*) ((conn->secure) ? "https" : "http")));
    
    sock = conn->sock;
    mprAddHash(vars, "SERVER_ADDR", sock->acceptIp);
    mprAddHash(vars, "SERVER_NAME", server->name);
    itos(port, sizeof(port) - 1, sock->acceptPort, 10);
    mprAddHash(vars, "SERVER_PORT", sclone(port));

    /*  HTTP/1.0 or HTTP/1.1 */
    mprAddHash(vars, "SERVER_PROTOCOL", conn->protocol);
    mprAddHash(vars, "SERVER_SOFTWARE", server->software);

    /*  This is the complete URI before decoding */ 
    mprAddHash(vars, "REQUEST_URI", rx->uri);

    /*  URLs are broken into the following: http://{SERVER_NAME}:{SERVER_PORT}{SCRIPT_NAME}{PATH_INFO} */
    mprAddHash(vars, "PATH_INFO", rx->pathInfo);
    mprAddHash(vars, "SCRIPT_NAME", rx->scriptName);
    mprAddHash(vars, "SCRIPT_FILENAME", tx->filename);

    if (rx->pathTranslated) {
        /*  Only set PATH_TRANSLATED if PATH_INFO is set (CGI spec) */
        mprAddHash(vars, "PATH_TRANSLATED", rx->pathTranslated);
    }
    //  MOB -- how do these relate to MVC apps and non-mvc apps
    mprAddHash(vars, "DOCUMENT_ROOT", conn->documentRoot);
    mprAddHash(vars, "SERVER_ROOT", server->serverRoot);

    if (rx->files) {
        for (index = 0, hp = 0; (hp = mprGetNextHash(conn->rx->files, hp)) != 0; index++) {
            up = (HttpUploadFile*) hp->data;
            mprAddHash(vars, mprAsprintf("FILE_%d_FILENAME", index), up->filename);
            mprAddHash(vars, mprAsprintf("FILE_%d_CLIENT_FILENAME", index), up->clientFilename);
            mprAddHash(vars, mprAsprintf("FILE_%d_CONTENT_TYPE", index), up->contentType);
            mprAddHash(vars, mprAsprintf("FILE_%d_NAME", index), hp->key);
            itos(size, sizeof(size) - 1, up->size, 10);
            mprAddHash(vars, mprAsprintf("FILE_%d_SIZE", index), size);
        }
    }
}


/*  
    Add variables to the vars environment store. This comes from the query string and urlencoded post data.
    Make variables for each keyword in a query string. The buffer must be url encoded (ie. key=value&key2=value2..., 
    spaces converted to '+' and all else should be %HEX encoded).
 */
void httpAddVars(HttpConn *conn, cchar *buf, ssize len)
{
    HttpRx          *rx;
    MprHashTable    *vars;
    cchar           *oldValue;
    char            *newValue, *decoded, *keyword, *value, *tok;

    rx = conn->rx;
    vars = rx->formVars;
    if (vars == 0) {
        return;
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
                Append to existing keywords.
             */
            oldValue = mprLookupHash(vars, keyword);
            if (oldValue != 0 && *oldValue) {
                if (*value) {
                    newValue = sjoin(oldValue, " ", value, NULL);
                    mprAddHash(vars, keyword, newValue);
                }
            } else {
                mprAddHash(vars, keyword, value);
            }
        }
        keyword = stok(0, "&", &tok);
    }
    /*  Must not free "decoded". This will be freed when the response completes */
}


void httpAddVarsFromQueue(HttpQueue *q)
{
    HttpConn        *conn;
    MprBuf          *content;

    mprAssert(q);
    
    conn = q->conn;
    if (conn->rx->form && q->first && q->first->content) {
        content = q->first->content;
        mprAddNullToBuf(content);
        mprLog(3, "Form body data: length %d, \"%s\"", mprGetBufLength(content), mprGetBufStart(content));
        httpAddVars(conn, mprGetBufStart(content), mprGetBufLength(content));
    }
}


int httpTestFormVar(HttpConn *conn, cchar *var)
{
    MprHashTable    *vars;
    
    vars = conn->rx->formVars;
    if (vars == 0) {
        return 0;
    }
    return vars && mprLookupHash(vars, var) != 0;
}


cchar *httpGetFormVar(HttpConn *conn, cchar *var, cchar *defaultValue)
{
    MprHashTable    *vars;
    cchar           *value;
    
    vars = conn->rx->formVars;
    if (vars) {
        value = mprLookupHash(vars, var);
        return (value) ? value : defaultValue;
    }
    return defaultValue;
}


int httpGetIntFormVar(HttpConn *conn, cchar *var, int defaultValue)
{
    MprHashTable    *vars;
    cchar           *value;
    
    vars = conn->rx->formVars;
    if (vars) {
        value = mprLookupHash(vars, var);
        return (value) ? (int) stoi(value, 10, NULL) : defaultValue;
    }
    return defaultValue;
}


//  MOB - need formatted version
void httpSetFormVar(HttpConn *conn, cchar *var, cchar *value) 
{
    MprHashTable    *vars;
    
    vars = conn->rx->formVars;
    if (vars == 0) {
        /* This is allowed. Upload filter uses this when uploading to the file handler */
        return;
    }
    //  MOB -- DUP?
    mprAddHash(vars, var, (void*) value);
}


void httpSetIntFormVar(HttpConn *conn, cchar *var, int value) 
{
    MprHashTable    *vars;
    
    vars = conn->rx->formVars;
    if (vars == 0) {
        /* This is allowed. Upload filter uses this when uploading to the file handler */
        return;
    }
    mprAddHash(vars, var, mprAsprintf("%d", value));
}


int httpCompareFormVar(HttpConn *conn, cchar *var, cchar *value)
{
    MprHashTable    *vars;
    
    vars = conn->rx->formVars;
    
    if (vars == 0) {
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
    mprAddHash(rx->files, id, upfile);
}


void httpRemoveUploadFile(HttpConn *conn, cchar *id)
{
    HttpRx    *rx;
    HttpUploadFile  *upfile;

    rx = conn->rx;

    upfile = (HttpUploadFile*) mprLookupHash(rx->files, id);
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

    for (hp = 0; rx->files && (hp = mprGetNextHash(rx->files, hp)) != 0; ) {
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
