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
void httpCreatePipeline(HttpConn *conn, HttpLocation *location, HttpStage *proposedHandler)
{
    Http                *http;
    HttpTransmitter     *trans;
    HttpReceiver        *rec;
    HttpQueue           *q, *qhead, *rq, *rqhead;
    HttpStage           *stage, *filter;
    int                 next;

    http = conn->http;
    rec = conn->receiver;
    trans = conn->transmitter;

    location = (location) ? location : http->clientLocation;

    trans->outputPipeline = mprCreateList(trans);
    trans->handler = proposedHandler ? proposedHandler : http->passHandler;

    if (trans->handler) {
        mprAddItem(trans->outputPipeline, trans->handler);
    }
    if (location->outputStages) {
        for (next = 0; (filter = mprGetNextItem(location->outputStages, &next)) != 0; ) {
            if (matchFilter(conn, filter)) {
                mprAddItem(trans->outputPipeline, filter);
            }
        }
    }
    if (trans->connector == 0) {
        trans->connector = location->connector;
    }
#if FUTURE
    if (trans->connector == 0) {
        if (location && location->connector) {
            trans->connector = location->connector;
        } else if (trans->handler == http->fileHandler && !rec->ranges && !conn->secure && 
                trans->chunkSize <= 0 && !conn->traceMask) {
            trans->connector = http->sendConnector;
        } else {
            trans->connector = http->netConnector;
        }
    }
#endif
    mprAddItem(trans->outputPipeline, trans->connector);

    /*  
        Create the receive pipeline for this request
     */
    if (rec->needInputPipeline) {
        rec->inputPipeline = mprCreateList(trans);
        mprAddItem(rec->inputPipeline, http->netConnector);
        if (location) {
            for (next = 0; (filter = mprGetNextItem(location->inputStages, &next)) != 0; ) {
                if (!matchFilter(conn, filter)) {
                    continue;
                }
                mprAddItem(rec->inputPipeline, filter);
            }
        }
        mprAddItem(rec->inputPipeline, trans->handler);
    }
    /* Incase a filter changed the handler */
    mprSetItem(trans->outputPipeline, 0, trans->handler);
    if (trans->handler->flags & HTTP_STAGE_THREAD && !conn->threaded) {
        /* Start with dispatcher disabled. Conn.c will enable */
        conn->dispatcher = mprCreateDispatcher(conn, trans->handler->name, 0);
    }

    /*  Create the outgoing queue heads and open the queues */
    q = &trans->queue[HTTP_QUEUE_TRANS];
    for (next = 0; (stage = mprGetNextItem(trans->outputPipeline, &next)) != 0; ) {
        q = httpCreateQueue(conn, stage, HTTP_QUEUE_TRANS, q);
    }

    /*  Create the incoming queue heads and open the queues.  */
    q = &trans->queue[HTTP_QUEUE_RECEIVE];
    for (next = 0; (stage = mprGetNextItem(rec->inputPipeline, &next)) != 0; ) {
        q = httpCreateQueue(conn, stage, HTTP_QUEUE_RECEIVE, q);
    }

    setEnvironment(conn);

    conn->writeq = conn->transmitter->queue[HTTP_QUEUE_TRANS].nextQ;
    conn->readq = conn->transmitter->queue[HTTP_QUEUE_RECEIVE].prevQ;

    httpPutForService(conn->writeq, httpCreateHeaderPacket(conn->writeq), 0);

    /*  
        Pair up the send and receive queues
     */
    qhead = &trans->queue[HTTP_QUEUE_TRANS];
    rqhead = &trans->queue[HTTP_QUEUE_RECEIVE];
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
     */
    qhead = &trans->queue[HTTP_QUEUE_TRANS];
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        if (q->open && !(q->flags & HTTP_QUEUE_OPEN)) {
            q->flags |= HTTP_QUEUE_OPEN;
            httpOpenQueue(q, conn->transmitter->chunkSize);
        }
    }

    if (rec->needInputPipeline) {
        qhead = &trans->queue[HTTP_QUEUE_RECEIVE];
        for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
            if (q->open && !(q->flags & HTTP_QUEUE_OPEN)) {
                if (q->pair == 0 || !(q->pair->flags & HTTP_QUEUE_OPEN)) {
                    q->flags |= HTTP_QUEUE_OPEN;
                    httpOpenQueue(q, conn->transmitter->chunkSize);
                }
            }
        }
    }
    conn->flags |= HTTP_CONN_PIPE_CREATED;
}


void httpSetPipeHandler(HttpConn *conn, HttpStage *handler)
{
    conn->transmitter->handler = (handler) ? handler : conn->http->passHandler;
}


void httpSetSendConnector(HttpConn *conn, cchar *path)
{
    HttpTransmitter     *trans;
    HttpQueue           *q, *qhead;
    int                 max;

    trans = conn->transmitter;
    trans->flags |= HTTP_TRANS_SENDFILE;
    trans->filename = mprStrdup(trans, path);
    max = conn->limits->transmissionBodySize;

    qhead = &trans->queue[HTTP_QUEUE_TRANS];
    for (q = conn->writeq; q != qhead; q = q->nextQ) {
        q->max = max;
        q->packetSize = max;
    }
}


void httpDestroyPipeline(HttpConn *conn)
{
    HttpTransmitter *trans;
    HttpQueue       *q, *qhead;
    int             i;

    if (conn->flags & HTTP_CONN_PIPE_CREATED && conn->transmitter) {
        trans = conn->transmitter;
        for (i = 0; i < HTTP_MAX_QUEUE; i++) {
            qhead = &trans->queue[i];
            for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
                if (q->close && q->flags & HTTP_QUEUE_OPEN) {
                    q->flags &= ~HTTP_QUEUE_OPEN;
                    q->close(q);
                }
            }
        }
        conn->flags &= ~HTTP_CONN_PIPE_CREATED;
    }
}


void httpStartPipeline(HttpConn *conn)
{
    HttpQueue       *qhead, *q;
    HttpTransmitter *trans;
    
#if OLD
    //  MOB -- should this run all the start entry points in the pipeline?
    q = conn->transmitter->queue[HTTP_QUEUE_TRANS].nextQ;
    if (q->stage->start) {
        q->stage->start(q);
    }
#endif
    trans = conn->transmitter;
    qhead = &trans->queue[HTTP_QUEUE_TRANS];
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        if (q->start && !(q->flags & HTTP_QUEUE_STARTED)) {
            q->flags |= HTTP_QUEUE_STARTED;
            q->stage->start(q);
        }
    }

    if (conn->receiver->needInputPipeline) {
        qhead = &trans->queue[HTTP_QUEUE_RECEIVE];
        for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
            if (q->start && !(q->flags & HTTP_QUEUE_STARTED)) {
                if (q->pair == 0 || !(q->pair->flags & HTTP_QUEUE_STARTED)) {
                    q->flags |= HTTP_QUEUE_STARTED;
                    q->stage->start(q);
                }
            }
        }
    }
}


void httpProcessPipeline(HttpConn *conn)
{
    HttpQueue           *q;
    
    q = conn->transmitter->queue[HTTP_QUEUE_TRANS].nextQ;
    if (q->stage->process) {
        q->stage->process(q);
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
        if (!q->servicing) {
            httpServiceQueue(q);
            workDone = 1;
        }
    }
    return workDone;
}


void httpDiscardTransmitData(HttpConn *conn)
{
    HttpTransmitter     *trans;
    HttpQueue           *q, *qhead;

    trans = conn->transmitter;
    if (trans == 0) {
        return;
    }
    qhead = &trans->queue[HTTP_QUEUE_TRANS];
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        httpDiscardData(q, 1);
    }
}


/*
    Create the form variables based on the URI query. Also create formVars for CGI style programs (cgi | egi)
 */
static void setEnvironment(HttpConn *conn)
{
    HttpReceiver    *rec;
    HttpTransmitter *trans;

    rec = conn->receiver;
    trans = conn->transmitter;

    if (trans->handler->flags & (HTTP_STAGE_VARS | HTTP_STAGE_ENV_VARS)) {
        rec->formVars = mprCreateHash(rec, HTTP_MED_HASH_SIZE);
        if (rec->parsedUri->query) {
            httpAddVars(conn, rec->parsedUri->query, (int) strlen(rec->parsedUri->query));
        }
    }
    if (trans->handler && (trans->handler->flags & HTTP_STAGE_ENV_VARS)) {
        httpCreateEnvVars(conn);
    }
}


/*
    Match a filter by extension
 */
static bool matchFilter(HttpConn *conn, HttpStage *filter)
{
    HttpReceiver    *rec;
    HttpTransmitter *trans;

    rec = conn->receiver;
    trans = conn->transmitter;
    if (filter->match) {
        return filter->match(conn, filter);
    }
    if (filter->extensions && *trans->extension) {
        return mprLookupHash(filter->extensions, trans->extension) != 0;
    }
    return 1;
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
