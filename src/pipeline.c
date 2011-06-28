/*
    pipeline.c -- HTTP pipeline processing. 
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forward ***********************************/

static bool matchFilter(HttpConn *conn, HttpStage *filter, int dir);
static void openQueues(HttpConn *conn);
static void pairQueues(HttpConn *conn);
static void setVars(HttpConn *conn);

/*********************************** Code *************************************/

void httpCreateTxPipeline(HttpConn *conn, HttpLoc *loc, HttpStage *proposedHandler)
{
    Http        *http;
    HttpTx      *tx;
    HttpRx      *rx;
    HttpQueue   *q;
    HttpStage   *stage, *filter;
    int         next;

    mprAssert(conn);
    mprAssert(loc);

    http = conn->http;
    rx = conn->rx;
    tx = conn->tx;

    tx->outputPipeline = mprCreateList(-1, 0);
    tx->handler = proposedHandler ? proposedHandler : http->passHandler;
    mprAddItem(tx->outputPipeline, tx->handler);

    if (loc->outputStages) {
        for (next = 0; (filter = mprGetNextItem(loc->outputStages, &next)) != 0; ) {
            if (matchFilter(conn, filter, HTTP_STAGE_TX)) {
                mprAddItem(tx->outputPipeline, filter);
            }
        }
    }
    if (tx->connector == 0) {
        if (tx->handler == http->fileHandler && rx->flags & HTTP_GET && !tx->outputRanges && 
                !conn->secure && tx->chunkSize <= 0 &&
                httpShouldTrace(conn, HTTP_TRACE_TX, HTTP_TRACE_BODY, tx->extension) < 0) {
            tx->connector = http->sendConnector;
        } else if (loc && loc->connector) {
            tx->connector = loc->connector;
        } else {
            tx->connector = http->netConnector;
        }
    }
    mprAddItem(tx->outputPipeline, tx->connector);

    /*  Create the outgoing queue heads and open the queues */
    q = tx->queue[HTTP_QUEUE_TX];
    for (next = 0; (stage = mprGetNextItem(tx->outputPipeline, &next)) != 0; ) {
        q = httpCreateQueue(conn, stage, HTTP_QUEUE_TX, q);
    }
    conn->writeq = tx->queue[HTTP_QUEUE_TX]->nextQ;
    conn->connq = tx->queue[HTTP_QUEUE_TX]->prevQ;

    if ((tx->handler->flags & HTTP_STAGE_VERIFY_ENTITY) && !tx->fileInfo.valid && !(rx->flags & HTTP_PUT)) {
        httpError(conn, HTTP_CODE_NOT_FOUND, "Can't open document: %s", tx->filename);
    }
    setVars(conn);
    pairQueues(conn);
    openQueues(conn);
    httpPutForService(conn->writeq, httpCreateHeaderPacket(), 0);
}


void httpCreateRxPipeline(HttpConn *conn, HttpLoc *loc)
{
    Http        *http;
    HttpTx      *tx;
    HttpRx      *rx;
    HttpQueue   *q;
    HttpStage   *stage, *filter;
    int         next;

    mprAssert(conn);
    mprAssert(loc);

    http = conn->http;
    rx = conn->rx;
    tx = conn->tx;

    rx->inputPipeline = mprCreateList(-1, 0);
    mprAddItem(rx->inputPipeline, http->netConnector);
    if (loc) {
        for (next = 0; (filter = mprGetNextItem(loc->inputStages, &next)) != 0; ) {
            if (!matchFilter(conn, filter, HTTP_STAGE_RX)) {
                continue;
            }
            mprAddItem(rx->inputPipeline, filter);
        }
    }
    mprAddItem(rx->inputPipeline, tx->handler);
    if (tx->outputPipeline) {
        /* Set the zero'th entry Incase a filter changed tx->handler */
        mprSetItem(tx->outputPipeline, 0, tx->handler);
    }
    /*  Create the incoming queue heads and open the queues.  */
    q = tx->queue[HTTP_QUEUE_RX];
    for (next = 0; (stage = mprGetNextItem(rx->inputPipeline, &next)) != 0; ) {
        q = httpCreateQueue(conn, stage, HTTP_QUEUE_RX, q);
    }
    conn->readq = tx->queue[HTTP_QUEUE_RX]->prevQ;

    pairQueues(conn);
    openQueues(conn);
}


static void pairQueues(HttpConn *conn)
{
    HttpTx      *tx;
    HttpQueue   *q, *qhead, *rq, *rqhead;

    tx = conn->tx;
    qhead = tx->queue[HTTP_QUEUE_TX];
    rqhead = tx->queue[HTTP_QUEUE_RX];
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        if (q->pair == 0) {
            for (rq = rqhead->nextQ; rq != rqhead; rq = rq->nextQ) {
                if (q->stage == rq->stage) {
                    q->pair = rq;
                    rq->pair = q;
                }
            }
        }
    }
}


static void openQueues(HttpConn *conn)
{
    HttpTx      *tx;
    HttpQueue   *q, *qhead;
    int         i;

    tx = conn->tx;
    for (i = 0; i < HTTP_MAX_QUEUE; i++) {
        qhead = tx->queue[i];
        for (q = qhead->nextQ; q->nextQ != qhead; q = q->nextQ) {
            if (q->open && !(q->flags & (HTTP_QUEUE_OPEN))) {
                if (q->pair == 0 || !(q->pair->flags & HTTP_QUEUE_OPEN)) {
                    q->flags |= HTTP_QUEUE_OPEN;
                    httpOpenQueue(q, conn->tx->chunkSize);
                }
            }
        }
    }
}


void httpSetPipelineHandler(HttpConn *conn, HttpStage *handler)
{
    conn->tx->handler = (handler) ? handler : conn->http->passHandler;
}


void httpSetSendConnector(HttpConn *conn, cchar *path)
{
#if !BLD_FEATURE_ROMFS
    HttpTx      *tx;

    tx = conn->tx;
    tx->flags |= HTTP_TX_SENDFILE;
    tx->filename = sclone(path);
#else
    mprError("Send connector not available if ROMFS enabled");
#endif
}


void httpDestroyPipeline(HttpConn *conn)
{
    HttpTx      *tx;
    HttpQueue   *q, *qhead;
    int         i;

    if (conn->tx) {
        tx = conn->tx;
        for (i = 0; i < HTTP_MAX_QUEUE; i++) {
            qhead = tx->queue[i];
            for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
                if (q->close && q->flags & HTTP_QUEUE_OPEN) {
                    q->flags &= ~HTTP_QUEUE_OPEN;
                    q->stage->close(q);
                }
            }
        }
    }
}


void httpStartPipeline(HttpConn *conn)
{
    HttpQueue   *qhead, *q, *prevQ, *nextQ;
    HttpTx      *tx;
    
    tx = conn->tx;

    if (conn->rx->needInputPipeline) {
        qhead = tx->queue[HTTP_QUEUE_RX];
        for (q = qhead->nextQ; q->nextQ != qhead; q = nextQ) {
            nextQ = q->nextQ;
            if (q->start && !(q->flags & HTTP_QUEUE_STARTED)) {
                if (q->pair == 0 || !(q->pair->flags & HTTP_QUEUE_STARTED)) {
                    q->flags |= HTTP_QUEUE_STARTED;
                    HTTP_TIME(conn, q->stage->name, "start", q->stage->start(q));
                }
            }
        }
    }
    qhead = tx->queue[HTTP_QUEUE_TX];
    for (q = qhead->prevQ; q->prevQ != qhead; q = prevQ) {
        prevQ = q->prevQ;
        if (q->start && !(q->flags & HTTP_QUEUE_STARTED)) {
            q->flags |= HTTP_QUEUE_STARTED;
            HTTP_TIME(conn, q->stage->name, "start", q->stage->start(q));
        }
    }
    /* Start the handler last */
    q = qhead->nextQ;
    if (q->start) {
        mprAssert(!(q->flags & HTTP_QUEUE_STARTED));
        q->flags |= HTTP_QUEUE_STARTED;
        HTTP_TIME(conn, q->stage->name, "start", q->stage->start(q));
    }

    if (!conn->error && !conn->writeComplete && conn->rx->remainingContent > 0) {
        /* If no remaining content, wait till the processing stage to avoid duplicate writable events */
        httpWritable(conn);
    }
}


/*
    Note: this may be called multiple times
 */
void httpProcessPipeline(HttpConn *conn)
{
    HttpQueue   *q;
    
    if (conn->error) {
        httpFinalize(conn);
    }
    q = conn->tx->queue[HTTP_QUEUE_TX]->nextQ;
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
    while (conn->state < HTTP_STATE_COMPLETE && (q = httpGetNextQueueForService(conn->serviceq)) != NULL) {
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
    qhead = tx->queue[HTTP_QUEUE_TX];
    for (q = qhead->nextQ; q != qhead; q = q->nextQ) {
        httpDiscardData(q, 1);
    }
}


static void trimExtraPath(HttpConn *conn)
{
    HttpAlias   *alias;
    HttpRx      *rx;
    HttpTx      *tx;
    char        *cp, *extra, *start;
    ssize       len;

    rx = conn->rx;
    tx = conn->tx;
    alias = rx->alias;

    /*
        Find the script name in the uri. This is assumed to be either:
        - The original uri up to and including first path component containing a ".", or
        - The entire original uri
        Once found, set the scriptName and trim the extraPath from pathInfo
        The filename is used to search for a component with "." because we want to skip the alias prefix.
     */
    start = &tx->filename[strlen(alias->filename)];
    if ((cp = strchr(start, '.')) != 0 && (extra = strchr(cp, '/')) != 0) {
        len = alias->prefixLen + extra - start;
        if (0 < len && len < slen(rx->pathInfo)) {
            *extra = '\0';
            rx->extraPath = sclone(&rx->pathInfo[len]);
            rx->pathInfo[len] = '\0';
            return;
        }
    }
}


/*
    Create the form variables based on the URI query. Also create formVars for CGI style programs (cgi | egi)
 */
static void setVars(HttpConn *conn)
{
    HttpRx      *rx;
    HttpTx      *tx;

    rx = conn->rx;
    tx = conn->tx;

    if (tx->handler->flags & HTTP_STAGE_EXTRA_PATH) {
        trimExtraPath(conn);
    }
    if (tx->handler->flags & HTTP_STAGE_QUERY_VARS && rx->parsedUri->query) {
        rx->formVars = httpAddVars(rx->formVars, rx->parsedUri->query, slen(rx->parsedUri->query));
    }
    if (tx->handler->flags & HTTP_STAGE_CGI_VARS) {
        httpCreateCGIVars(conn);
    }
}


/*
    Match a filter by extension
 */
static bool matchFilter(HttpConn *conn, HttpStage *filter, int dir)
{
    HttpTx      *tx;

    tx = conn->tx;
    if (filter->match) {
        return filter->match(conn, filter, dir);
    }
    if (filter->extensions && tx->extension) {
        return mprLookupKey(filter->extensions, tx->extension) != 0;
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
