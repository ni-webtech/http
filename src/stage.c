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
/*
    Invoked for all stages
 */

static void defaultOpen(HttpQueue *q)
{
    HttpTx      *tx;

    tx = q->conn->tx;
    q->packetSize = (tx->chunkSize > 0) ? min(q->max, tx->chunkSize): q->max;
}


/*
    Invoked for all stages
 */
static void defaultClose(HttpQueue *q)
{
}


/*  
    Put packets on the service queue.
 */
static void outgoing(HttpQueue *q, HttpPacket *packet)
{
    int     enableService;

    /*  
        Handlers service routines must only be auto-enabled if in the running state.
     */
    mprAssert(httpVerifyQueue(q));
    enableService = !(q->stage->flags & HTTP_STAGE_HANDLER) || (q->conn->state >= HTTP_STATE_READY) ? 1 : 0;
    httpPutForService(q, packet, enableService);
    mprAssert(httpVerifyQueue(q));
}


/*  
    Default incoming data routine.  Simply transfer the data upstream to the next filter or handler.
 */
static void incoming(HttpQueue *q, HttpPacket *packet)
{
    mprAssert(q);
    mprAssert(packet);
    mprAssert(httpVerifyQueue(q));
    
    if (q->nextQ->put) {
        httpPutPacketToNext(q, packet);
    } else {
        /* This queue is the last queue in the pipeline */
        //  MOB - should this call WillAccept?
        if (httpGetPacketLength(packet) > 0) {
            httpJoinPacketForService(q, packet, 0);
            HTTP_NOTIFY(q->conn, 0, HTTP_NOTIFY_READABLE);
        } else {
            /* Zero length packet means eof */
            httpPutForService(q, packet, HTTP_DELAY_SERVICE);
            HTTP_NOTIFY(q->conn, 0, HTTP_NOTIFY_READABLE);
        }
    }
    mprAssert(httpVerifyQueue(q));
}


void httpDefaultOutgoingServiceStage(HttpQueue *q)
{
    HttpPacket    *packet;

    for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
        if (!httpWillNextQueueAcceptPacket(q, packet)) {
            httpPutBackPacket(q, packet);
            return;
        }
        httpPutPacketToNext(q, packet);
    }
}


static void incomingService(HttpQueue *q)
{
}


HttpStage *httpCreateStage(Http *http, cchar *name, int flags, MprModule *module)
{
    HttpStage     *stage;

    mprAssert(http);
    mprAssert(name && *name);

    if ((stage = httpLookupStage(http, name)) != 0) {
        if (!(stage->flags & HTTP_STAGE_UNLOADED)) {
            mprError("Stage %s already exists", name);
            return 0;
        }
    } else if ((stage = mprAllocObj(HttpStage, manageStage)) == 0) {
        return 0;
    }
    if ((flags & HTTP_METHOD_MASK) == 0) {
        flags |= HTTP_STAGE_METHODS;
    }
    stage->flags = flags;
    stage->name = sclone(name);
    stage->open = defaultOpen;
    stage->close = defaultClose;
    stage->incoming = incoming;
    stage->incomingService = incomingService;
    stage->outgoing = outgoing;
    stage->outgoingService = httpDefaultOutgoingServiceStage;
    stage->module = module;
    httpAddStage(http, stage);
    return stage;
}


static void manageStage(HttpStage *stage, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(stage->name);
        mprMark(stage->path);
        mprMark(stage->stageData);
        mprMark(stage->module);
        mprMark(stage->extensions);
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


HttpStage *httpCreateHandler(Http *http, cchar *name, int flags, MprModule *module)
{
    return httpCreateStage(http, name, flags | HTTP_STAGE_HANDLER, module);
}


HttpStage *httpCreateFilter(Http *http, cchar *name, int flags, MprModule *module)
{
    return httpCreateStage(http, name, flags | HTTP_STAGE_FILTER, module);
}


HttpStage *httpCreateConnector(Http *http, cchar *name, int flags, MprModule *module)
{
    return httpCreateStage(http, name, flags | HTTP_STAGE_CONNECTOR, module);
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
