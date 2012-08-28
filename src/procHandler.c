/*
    procHandler.c -- Procedure callback handler

    This handler maps URIs to procction names that have been registered via httpDefineProc.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/*********************************** Code *************************************/

static void startProc(HttpQueue *q)
{
    HttpConn    *conn;
    HttpProc     proc;
    cchar       *name;

    mprLog(5, "Start procHandler");
    conn = q->conn;
    name = conn->rx->pathInfo;
    if ((proc = mprLookupKey(conn->tx->handler->stageData, name)) == 0) {
        mprError("Can't find procction callback %s", name);
    } else {
        (*proc)(conn);
    }
}


void httpDefineProc(cchar *name, HttpProc proc)
{
    HttpStage   *stage;

    if ((stage = httpLookupStage(MPR->httpService, "procHandler")) == 0) {
        mprError("Can't find procHandler");
        return;
    }
    mprAddKey(stage->stageData, name, proc);
}


int httpOpenProcHandler(Http *http)
{
    HttpStage     *stage;

    if ((stage = httpCreateHandler(http, "procHandler", HTTP_STAGE_ALL, NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    http->procHandler = stage;
    if ((stage->stageData = mprCreateHash(0, MPR_HASH_STATIC_VALUES)) == 0) {
        return MPR_ERR_MEMORY;
    }
    stage->start = startProc;
    return 0;
}


/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
