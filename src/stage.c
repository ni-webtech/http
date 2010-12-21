/*
    stage.c -- Stages are the building blocks of the Http request pipeline.

    Stages support the extensible and modular processing of HTTP requests. Handlers are a kind of stage that are the 
    first line processing of a request. Connectors are the last stage in a chain to send/receive data over a network.

    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************* Forwards ***********************************/

static void manageStage(HttpStage *stage, int flags);

/*********************************** Code *************************************/

static void defaultOpen(HttpQueue *q)
{
    HttpTx      *tx;

    tx = q->conn->tx;
    q->packetSize = (tx->chunkSize > 0) ? min(q->max, tx->chunkSize): q->max;
}


static void defaultClose(HttpQueue *q)
{
}


/*  
    The default put will put the packet on the service queue.
 */
static void outgoingData(HttpQueue *q, HttpPacket *packet)
{
    int     enableService;

    /*  
        Handlers service routines must only be auto-enabled if in the running state.
     */
    enableService = !(q->stage->flags & HTTP_STAGE_HANDLER) || (q->conn->state == HTTP_STATE_RUNNING) ? 1 : 0;
    httpPutForService(q, packet, enableService);
}


/*  
    Default incoming data routine.  Simply transfer the data upstream to the next filter or handler.
 */
static void incomingData(HttpQueue *q, HttpPacket *packet)
{
    mprAssert(q);
    mprAssert(packet);
    
    if (q->nextQ->put) {
        httpSendPacketToNext(q, packet);
    } else {
        /* This queue is the last queue in the pipeline */
        if (httpGetPacketLength(packet) > 0) {
            httpJoinPacketForService(q, packet, 0);
            HTTP_NOTIFY(q->conn, 0, HTTP_NOTIFY_READABLE);
        } else {
            /* Zero length packet means eof */
            httpPutForService(q, packet, 0);
            HTTP_NOTIFY(q->conn, 0, HTTP_NOTIFY_READABLE);
        }
    }
}


void httpDefaultOutgoingServiceStage(HttpQueue *q)
{
    HttpPacket    *packet;

    for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
        if (!httpWillNextQueueAcceptPacket(q, packet)) {
            httpPutBackPacket(q, packet);
            return;
        }
        httpSendPacketToNext(q, packet);
    }
}


static void incomingService(HttpQueue *q)
{
}


HttpStage *httpCreateStage(Http *http, cchar *name, int flags)
{
    HttpStage     *stage;

    mprAssert(http);
    mprAssert(name && *name);

    if ((stage = mprAllocObj(HttpStage, manageStage)) == 0) {
        return 0;
    }
    stage->flags = flags;
    stage->name = sclone(name);
    stage->open = defaultOpen;
    stage->close = defaultClose;
    stage->incomingData = incomingData;
    stage->incomingService = incomingService;
    stage->outgoingData = outgoingData;
    stage->outgoingService = httpDefaultOutgoingServiceStage;
    httpRegisterStage(http, stage);
    return stage;
}


static void manageStage(HttpStage *stage, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(stage->name);
        mprMark(stage->stageData);
        mprMarkHash(stage->extensions);

    } else if (flags & MPR_MANAGE_FREE) {
    }
}


HttpStage *httpCloneStage(Http *http, HttpStage *stage)
{
    HttpStage   *clone;

    if ((clone = mprAllocObj(HttpStage, manageStage)) == 0) {
        return 0;
    }
    *clone = *stage;
    return clone;
}


HttpStage *httpCreateHandler(Http *http, cchar *name, int flags)
{
    HttpStage     *stage;
    
    stage = httpCreateStage(http, name, flags);
    stage->flags |= HTTP_STAGE_HANDLER;
    return stage;
}


HttpStage *httpCreateFilter(Http *http, cchar *name, int flags)
{
    HttpStage     *stage;
    
    stage = httpCreateStage(http, name, flags);
    stage->flags |= HTTP_STAGE_FILTER;
    return stage;
}


HttpStage *httpCreateConnector(Http *http, cchar *name, int flags)
{
    HttpStage     *stage;
    
    stage = httpCreateStage(http, name, flags);
    stage->flags |= HTTP_STAGE_CONNECTOR;
    return stage;
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
    vim: sw=8 ts=8 expandtab

    @end
 */
