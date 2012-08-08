/**
    testHttpGen.c - tests for HTTP
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************** Includes **********************************/

#include    "http.h"

/*********************************** Locals ***********************************/

typedef struct TestHttp {
    Http        *http;
    HttpConn    *conn;
} TestHttp;

static void manageTestHttp(TestHttp *th, int flags);

/************************************ Code ************************************/

static int initHttp(MprTestGroup *gp)
{
    MprSocket   *sp;
    TestHttp     *th;

    gp->data = th = mprAllocObj(TestHttp, manageTestHttp);
    
    if (getenv("NO_INTERNET")) {
        gp->skip = 1;
        return 0;
    }
    sp = mprCreateSocket(NULL);
    
    /*
        Test if we have network connectivity. If not, then skip these tests.
     */
    if (mprConnectSocket(sp, "www.google.com", 80, 0) < 0) {
        static int once = 0;
        if (once++ == 0) {
            mprPrintf("%12s Disabling tests %s.*: no internet connection. %d\n", "[Notice]", gp->fullName, once);
        }
        gp->skip = 1;
    }
    mprCloseSocket(sp, 0);
    return 0;
}


static void manageTestHttp(TestHttp *th, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(th->http);
        mprMark(th->conn);
    }
}


static void testCreateHttp(MprTestGroup *gp)
{
    TestHttp    *th;
    Http        *http;

    th = gp->data;
    th->http = http = httpCreate(gp);
    assert(http != 0);
}


static void testBasicHttpGet(MprTestGroup *gp)
{
    TestHttp    *th;
    Http        *http;
    HttpConn    *conn;
    MprOff      length;
    int         rc, status;

    th = gp->data;
    th->http = http = httpCreate(gp);
    assert(http != 0);

    th->conn = conn = httpCreateConn(http, NULL, gp->dispatcher);

    rc = httpConnect(conn, "GET", "http://embedthis.com/index.html");
    assert(rc >= 0);
    if (rc >= 0) {
        httpFinalize(conn);
        httpWait(conn, HTTP_STATE_COMPLETE, MPR_TIMEOUT_SOCKETS);
        status = httpGetStatus(conn);
        assert(status == 200 || status == 302);
        if (status != 200 && status != 302) {
            mprLog(0, "HTTP response status %d", status);
        }
        assert(httpGetError(conn) != 0);
        length = httpGetContentLength(conn);
        assert(length != 0);
    }
    httpDestroy(http);
}


#if BIT_FEATURE_SSL && (BIT_FEATURE_MATRIXSSL || BIT_FEATURE_OPENSSL)
static void testSecureHttpGet(MprTestGroup *gp)
{
    TestHttp    *th;
    Http        *http;
    HttpConn    *conn;
    int         rc, status;

    th = gp->data;
    th->http = http = httpCreate(gp);
    assert(http != 0);
    th->conn = conn = httpCreateConn(http, NULL, gp->dispatcher);
    assert(conn != 0);

    rc = httpConnect(conn, "GET", "https://www.ibm.com/");
    assert(rc >= 0);
    if (rc >= 0) {
        httpFinalize(conn);
        httpWait(conn, HTTP_STATE_COMPLETE, MPR_TIMEOUT_SOCKETS);
        status = httpGetStatus(conn);
        assert(status == 200 || status == 301 || status == 302);
        if (status != 200 && status != 301 && status != 302) {
            mprLog(0, "HTTP response status %d", status);
        }
    }
    httpDestroy(http);
}
#endif


#if FUTURE && TODO
    mprSetHttpTimeout(http, timeout);
    mprSetHttpRetries(http, retries);
    mprSetHttpKeepAlive(http, on);
    mprSetHttpAuth(http, authType, realm, username, password);
    mprSetHttpHeader(http, "MyHeader: value");
    mprSetHttpDefaultHost(http, "localhost");
    mprSetHttpDefaultPort(http, 80);
    mprSetHttpBuffer(http, initial, max);
    mprSetHttpCallback(http, fn, arg);
    mprGetHttpHeader(http);
    url = mprGetHttpParsedUrl(http);
#endif


MprTestDef testHttpGen = {
    "http", 0, initHttp, 0,
    {
        MPR_TEST(0, testCreateHttp),
        MPR_TEST(0, testBasicHttpGet),
#if BIT_FEATURE_SSL && (BIT_FEATURE_MATRIXSSL || BIT_FEATURE_OPENSSL)
        MPR_TEST(0, testSecureHttpGet),
#endif
        MPR_TEST(0, 0),
    },
};

/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2012. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.md distributed with 
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
