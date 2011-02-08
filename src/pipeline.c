/*
    pipeline.c -- HTTP pipeline processing.
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forward ***********************************/

static bool matchFilter(HttpConn *conn, HttpStage *filter);
static void setEnvironment(HttpConn *conn);

/*********************************** Code *************************************/
/*  
    Create processing pipeline
 */
void httpCreatePipeline(HttpConn *conn, HttpLoc *loc, HttpStage *proposedHandler)
{
    Http        *http;
    HttpTx      *tx;
    HttpRx      *rx;
    HttpQueue   *q, *qhead, *rq, *rqhead;
    HttpStage   *stage, *filter;
    int         next;

    http = conn->http;
    rx = conn->rx;
    tx = conn->tx;

    loc = (loc) ? loc : http->clientLocation;

    tx->outputPipeline = mprCreateList(-1, 0);
    tx->handler = proposedHandler ? proposedHandler : http->passHandler;

    if (tx->handler) {
        mprAddItem(tx->outputPipeline, tx->handler);
    }
    if (loc->outputStages) {
        for (next = 0; (filter = mprGetNextItem(loc->outputStages, &next)) != 0; ) {
            if (matchFilter(conn, filter)) {
                mprAddItem(tx->outputPipeline, filter);
            }
        }
    }
    if (tx->connector == 0) {
        tx->connector = loc->connector;
    }
#if FUTURE
    if (tx->connector == 0) {
        if (loc && loc->connector) {
            tx->connector = loc->connector;
        } else if (tx->handler == http->fileHandler && !rx->ranges && !conn->secure && 
                tx->chunkSize <= 0 && !conn->traceMask) {
            tx->connector = http->sendConnector;
        } else {
            tx->connector = http->netConnector;
        }
    }
#endif
    mprAddItem(tx->outputPipeline, tx->connector);

    /*  
        Create the receive pipeline for this request
     */
    if (rx->needInputPipeline) {
        rx->inputPipeline = mprCreateList(-1, 0);
        mprAddItem(rx->inputPipeline, http->netConnector);
        if (loc) {
            for (next = 0; (filter = mprGetNextItem(loc->inputStages, &next)) != 0; ) {
                if (!matchFilter(conn, filter)) {
                    continue;
                }
                mprAddItem(rx->inputPipeline, filter);
            }
        }
        mprAddItem(rx->inputPipeline, tx->handler);
    }
    
    /* Incase a filter changed the handler */
    mprSetItem(tx->outputPipeline, 0, tx->handler);
#if UNUSED
    if (tx->handler->flags & HTTP_STAGE_THREAD && !conn->threaded) {
        /* Start with dispatcher disabled. Conn.c will enable */
        tx->dispatcher = mprCreateDispatcher(tx->handler->name, 0);
        conn->dispatcher = tx->dispatcher;
    }
#endif
    /*  Create the outgoing queue heads and open the queues */
    q = &tx->queue[HTTP_QUEUE_TRANS];
    for (next = 0; (stage = mprGetNextItem(tx->outputPipeline, &next)) != 0; ) {
        q = httpCreateQueue(conn, stage, HTTP_QUEUE_TRANS, q);
    }

    /*  Create the incoming queue heads and open the queues.  */
    q = &tx->queue[HTTP_QUEUE_RECEIVE];
    for (next = 0; (stage = mprGetNextItem(rx->inputPipeline, &next)) != 0; ) {
        q = httpCreateQueue(conn, stage, HTTP_QUEUE_RECEIVE, q);
    }
    setEnvironment(conn);

    conn->writeq = conn->tx->queue[HTTP_QUEUE_TRANS].nextQ;
    conn->readq = conn->tx->queue[HTTP_QUEUE_RECEIVE].prevQ;
    httpPutForService(conn->writeq, httpCreateHeaderPacket(), 0);

    /*  
        Pair up the send and receive queues
     */
    qhead = &tx->queue[HTTP_QUEUE_TRANS];
    rqhead = &tx->queue[HTTP_QUEUE_RECEIVE];
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        for (rq = rqhead->nextQ; rq != rqhead; rq = rq->nextQ) {
            if (q->stage == rq->stage) {
                q->pair = rq;
                rq->pair = q;
            }
        }
    }

    /*  
        Open the queues (keep going on errors)
        Open in reverse order so the handler is opened last. This lets authFilter go early.
     */
    qhead = &tx->queue[HTTP_QUEUE_TRANS];
    for (q = qhead->prevQ; q != qhead; q = q->prevQ) {
        if (q->open && !(q->flags & HTTP_QUEUE_OPEN)) {
            q->flags |= HTTP_QUEUE_OPEN;
            httpOpenQueue(q, conn->tx->chunkSize);
        }
    }

    if (rx->needInputPipeline) {
        qhead = &tx->queue[HTTP_QUEUE_RECEIVE];
        for (q = qhead->prevQ; q != qhead; q = q->prevQ) {
            if (q->open && !(q->flags & HTTP_QUEUE_OPEN)) {
                if (q->pair == 0 || !(q->pair->flags & HTTP_QUEUE_OPEN)) {
                    q->flags |= HTTP_QUEUE_OPEN;
                    httpOpenQueue(q, conn->tx->chunkSize);
                }
            }
        }
    }
    conn->flags |= HTTP_CONN_PIPE_CREATED;
}


void httpSetPipeHandler(HttpConn *conn, HttpStage *handler)
{
    conn->tx->handler = (handler) ? handler : conn->http->passHandler;
}


void httpSetSendConnector(HttpConn *conn, cchar *path)
{
    HttpTx      *tx;
    HttpQueue   *q, *qhead;
    ssize       maxBody;

    tx = conn->tx;
    tx->flags |= HTTP_TX_SENDFILE;
    tx->filename = sclone(path);
    maxBody = conn->limits->transmissionBodySize;

    qhead = &tx->queue[HTTP_QUEUE_TRANS];
    for (q = conn->writeq; q != qhead; q = q->nextQ) {
        q->max = maxBody;
        q->packetSize = maxBody;
    }
}


void httpDestroyPipeline(HttpConn *conn)
{
    HttpTx      *tx;
    HttpQueue   *q, *qhead;
    int         i;

    if (conn->flags & HTTP_CONN_PIPE_CREATED && conn->tx) {
        tx = conn->tx;
        for (i = 0; i < HTTP_MAX_QUEUE; i++) {
            qhead = &tx->queue[i];
            for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
                if (q->close && q->flags & HTTP_QUEUE_OPEN) {
                    q->flags &= ~HTTP_QUEUE_OPEN;
                    q->stage->close(q);
                }
            }
        }
        conn->flags &= ~HTTP_CONN_PIPE_CREATED;
    }
}


void httpStartPipeline(HttpConn *conn)
{
    HttpQueue   *qhead, *q;
    HttpTx      *tx;
    
    tx = conn->tx;
    qhead = &tx->queue[HTTP_QUEUE_TRANS];
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        if (q->start && !(q->flags & HTTP_QUEUE_STARTED)) {
            q->flags |= HTTP_QUEUE_STARTED;
            HTTP_TIME(conn, q->stage->name, "start", q->stage->start(q));
        }
    }

    if (conn->rx->needInputPipeline) {
        qhead = &tx->queue[HTTP_QUEUE_RECEIVE];
        for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
            if (q->start && !(q->flags & HTTP_QUEUE_STARTED)) {
                if (q->pair == 0 || !(q->pair->flags & HTTP_QUEUE_STARTED)) {
                    q->flags |= HTTP_QUEUE_STARTED;
                    HTTP_TIME(conn, q->stage->name, "start", q->stage->start(q));
                }
            }
        }
    }
}


void httpProcessPipeline(HttpConn *conn)
{
    HttpQueue   *q;
    
    q = conn->tx->queue[HTTP_QUEUE_TRANS].nextQ;
    if (q->stage->process) {
        HTTP_TIME(conn, q->stage->name, "process", q->stage->process(q));
    }
}


/*  
    Run the queue service routines until there is no more work to be done. NOTE: all I/O is non-blocking.
 */
bool httpServiceQueues(HttpConn *conn)
{
    HttpQueue   *q;
    int         workDone;

    workDone = 0;
    while (conn->state < HTTP_STATE_COMPLETE && (q = httpGetNextQueueForService(&conn->serviceq)) != NULL) {
        if (q->servicing) {
            q->flags |= HTTP_QUEUE_RESERVICE;
        } else {
            mprAssert(q->schedulePrev == q->scheduleNext);
            httpServiceQueue(q);
            workDone = 1;
        }
    }
    return workDone;
}


void httpDiscardTransmitData(HttpConn *conn)
{
    HttpTx      *tx;
    HttpQueue   *q, *qhead;

    tx = conn->tx;
    if (tx == 0) {
        return;
    }
    qhead = &tx->queue[HTTP_QUEUE_TRANS];
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        httpDiscardData(q, 1);
    }
}


/*
    Create the form variables based on the URI query. Also create formVars for CGI style programs (cgi | egi)
 */
static void setEnvironment(HttpConn *conn)
{
    HttpRx      *rx;
    HttpTx      *tx;

    rx = conn->rx;
    tx = conn->tx;

    if (tx->handler->flags & (HTTP_STAGE_VARS | HTTP_STAGE_ENV_VARS)) {
        rx->formVars = mprCreateHash(HTTP_MED_HASH_SIZE, 0);
        if (rx->parsedUri->query) {
            httpAddVars(conn, rx->parsedUri->query, slen(rx->parsedUri->query));
        }
    }
    if (tx->handler && (tx->handler->flags & HTTP_STAGE_ENV_VARS)) {
        httpCreateEnvVars(conn);
    }
}


/*
    Match a filter by extension
 */
static bool matchFilter(HttpConn *conn, HttpStage *filter)
{
    HttpTx      *tx;

    tx = conn->tx;
    if (filter->match) {
        return filter->match(conn, filter);
    }
    if (filter->extensions && *tx->extension) {
        return mprLookupHash(filter->extensions, tx->extension) != 0;
    }
    return 1;
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
