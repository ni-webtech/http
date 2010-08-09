/*
    queue.c -- Queue support routines. Queues are the bi-directional data flow channels for the pipeline.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/************************************ Code ************************************/
/*  
    Createa a new queue for the given stage. If prev is given, then link the new queue after the previous queue.
 */
HttpQueue *httpCreateQueue(HttpConn *conn, HttpStage *stage, int direction, HttpQueue *prev)
{
    HttpQueue   *q;

    q = mprAllocObjZeroed(conn->tx, HttpQueue);
    if (q == 0) {
        return 0;
    }
    httpInitQueue(conn, q, stage->name);
    httpInitSchedulerQueue(q);

    q->conn = conn;
    q->stage = stage;
    q->close = stage->close;
    q->open = stage->open;
    q->start = stage->start;
    q->direction = direction;

    if (direction == HTTP_QUEUE_TRANS) {
        q->put = stage->outgoingData;
        q->service = stage->outgoingService;
        
    } else {
        q->put = stage->incomingData;
        q->service = stage->incomingService;
    }
    if (prev) {
        httpInsertQueue(prev, q);
    }
    return q;
}


/*  
    Initialize a bare queue. Used for dummy heads.
 */
void httpInitQueue(HttpConn *conn, HttpQueue *q, cchar *name)
{
    q->conn = conn;
    q->nextQ = q;
    q->prevQ = q;
    q->owner = name;
    q->packetSize = conn->limits->stageBufferSize;
    q->max = conn->limits->stageBufferSize;
    q->low = q->max / 100 *  5;    
}


/*  
    Insert a queue after the previous element
 */
void httpAppendQueue(HttpQueue *head, HttpQueue *q)
{
    q->nextQ = head;
    q->prevQ = head->prevQ;
    head->prevQ->nextQ = q;
    head->prevQ = q;
}


void httpDisableQueue(HttpQueue *q)
{
    mprLog(q, 7, "Disable q %s", q->owner);
    q->flags |= HTTP_QUEUE_DISABLED;
}


/*  
    Remove all data from non-header, non-eof packets in the queue. If removePackets is true, actually remove the packet too.
 */
void httpDiscardData(HttpQueue *q, bool removePackets)
{
    HttpPacket  *packet, *prev, *next;
    int         len;

    for (prev = 0, packet = q->first; packet; packet = next) {
        next = packet->next;
        if (packet->flags & (HTTP_PACKET_RANGE | HTTP_PACKET_DATA)) {
            if (removePackets) {
                if (prev) {
                    prev->next = next;
                } else {
                    q->first = next;
                }
                if (packet == q->last) {
                    q->last = prev;
                }
                q->count -= httpGetPacketLength(packet);
                mprAssert(q->count >= 0);
                httpFreePacket(q, packet);
                continue;
            } else {
                len = httpGetPacketLength(packet);
                q->conn->tx->length -= len;
                q->count -= len;
                mprAssert(q->count >= 0);
                if (packet->content) {
                    mprFlushBuf(packet->content);
                }
            }
        }
        prev = packet;
    }
}


/*  
    Flush queue data by scheduling the queue and servicing all scheduled queues. Return true if there is room for more data.
    If blocking is requested, the call will block until the queue count falls below the queue max.
    WARNING: Be very careful when using blocking == true. Should only be used by end applications and not by middleware.
 */
bool httpFlushQueue(HttpQueue *q, bool blocking)
{
    HttpConn    *conn;
    HttpQueue   *next;
    int         oldMode;

    LOG(q, 6, "httpFlushQueue blocking %d", blocking);

    if (q->flags & HTTP_QUEUE_DISABLED) {
        return 0;
    }
    conn = q->conn;
    do {
        oldMode = mprSetSocketBlockingMode(conn->sock, blocking);
        httpScheduleQueue(q);
        next = q->nextQ;
        if (next->count >= next->max) {
            httpScheduleQueue(next);
        }
        httpServiceQueues(conn);
        mprSetSocketBlockingMode(conn->sock, oldMode);
    } while (blocking && q->count >= q->max);
    return (q->count < q->max) ? 1 : 0;
}


void httpEnableQueue(HttpQueue *q)
{
    mprLog(q, 7, "Enable q %s", q->owner);
    q->flags &= ~HTTP_QUEUE_DISABLED;
    httpScheduleQueue(q);
}


HttpQueue *httpFindPreviousQueue(HttpQueue *q)
{
    while (q->prevQ) {
        q = q->prevQ;
        if (q->service) {
            return q;
        }
    }
    return 0;
}


HttpQueue *httpGetNextQueueForService(HttpQueue *q)
{
    HttpQueue     *next;
    
    if (q->scheduleNext != q) {
        next = q->scheduleNext;
        next->schedulePrev->scheduleNext = next->scheduleNext;
        next->scheduleNext->schedulePrev = next->schedulePrev;
        next->schedulePrev = next->scheduleNext = next;
        return next;
    }
    return 0;
}


/*  
    Return the number of bytes the queue will accept. Always positive.
 */
int httpGetQueueRoom(HttpQueue *q)
{
    mprAssert(q->max > 0);
    mprAssert(q->count >= 0);
    
    if (q->count >= q->max) {
        return 0;
    }
    return q->max - q->count;
}


void httpInitSchedulerQueue(HttpQueue *q)
{
    q->scheduleNext = q;
    q->schedulePrev = q;
}


/*  
    Insert a queue after the previous element
 */
void httpInsertQueue(HttpQueue *prev, HttpQueue *q)
{
    q->nextQ = prev->nextQ;
    q->prevQ = prev;
    prev->nextQ->prevQ = q;
    prev->nextQ = q;
}


bool httpIsQueueEmpty(HttpQueue *q)
{
    return q->first == 0;
}


void httpOpenQueue(HttpQueue *q, int chunkSize)
{
    if (chunkSize > 0) {
        q->packetSize = min(q->packetSize, chunkSize);
    }
    q->flags |= HTTP_QUEUE_OPEN;
    if (q->open) {
        q->open(q);
    }
}


/*  
    Read data. If sync mode, this will block. If async, will never block.
    Will return what data is available up to the requested size. Returns a byte count.
 */
int httpRead(HttpConn *conn, char *buf, int size)
{
    HttpPacket  *packet;
    HttpQueue   *q;
    HttpRx      *rec;
    MprBuf      *content;
    int         nbytes, len, events, inactivityTimeout;

    q = conn->readq;
    rec = conn->rx;
    
    while (q->count == 0 && !conn->async && conn->sock && (conn->state <= HTTP_STATE_CONTENT)) {
        httpServiceQueues(conn);
        events = MPR_READABLE;
        if (conn->sock && !mprSocketHasPendingData(conn->sock)) {
            if (mprIsSocketEof(conn->sock)) {
                break;
            }
            inactivityTimeout = conn->limits->inactivityTimeout ? conn->limits->inactivityTimeout : INT_MAX;
            events = mprWaitForSingleIO(conn, conn->sock->fd, MPR_READABLE, inactivityTimeout);
        }
        if (events) {
            httpCallEvent(conn, MPR_READABLE);
        }
    }
    for (nbytes = 0; size > 0 && q->count > 0; ) {
        if ((packet = q->first) == 0) {
            break;
        }
        content = packet->content;
        len = mprGetBufLength(content);
        len = min(len, size);
        if (len > 0) {
            len = mprGetBlockFromBuf(content, buf, len);
        }
        rec->readContent += len;
        buf += len;
        size -= len;
        q->count -= len;
        nbytes += len;
        if (mprGetBufLength(content) == 0) {
            httpGetPacket(q);
        }
    }
    return nbytes;
}


int httpIsEof(HttpConn *conn) 
{
    return conn->rx == 0 || conn->rx->readComplete;
}


char *httpReadString(HttpConn *conn)
{
    HttpRx    *rec;
    char            *content;
    int             remaining, sofar, nbytes;

    rec = conn->rx;

    if (rec->length > 0) {
        content = mprAlloc(rec, rec->length + 1);
        remaining = rec->length;
        sofar = 0;
        while (remaining > 0) {
            nbytes = httpRead(conn, &content[sofar], remaining);
            if (nbytes < 0) {
                return 0;
            }
            sofar += nbytes;
            remaining -= nbytes;
        }
    } else {
        content = mprAlloc(rec, HTTP_BUFSIZE);
        sofar = 0;
        while (1) {
            nbytes = httpRead(conn, &content[sofar], HTTP_BUFSIZE);
            if (nbytes < 0) {
                return 0;
            } else if (nbytes == 0) {
                break;
            }
            sofar += nbytes;
            content = mprRealloc(conn, content, sofar + HTTP_BUFSIZE);
        }
    }
    content[sofar] = '\0';
    return content;
}


void httpRemoveQueue(HttpQueue *q)
{
    q->prevQ->nextQ = q->nextQ;
    q->nextQ->prevQ = q->prevQ;
    q->prevQ = q->nextQ = q;
}


void httpScheduleQueue(HttpQueue *q)
{
    HttpQueue     *head;
    
    mprAssert(q->conn);
    head = &q->conn->serviceq;
    
    if (q->scheduleNext == q && !(q->flags & HTTP_QUEUE_DISABLED)) {
        q->scheduleNext = head;
        q->schedulePrev = head->schedulePrev;
        head->schedulePrev->scheduleNext = q;
        head->schedulePrev = q;
    }
}


void httpServiceQueue(HttpQueue *q)
{
    if (!q->servicing) {
        q->servicing = 1;
        /*  
            Since we are servicing this "q" now, we can remove from the schedule queue if it is already queued.
         */
        if (q->conn->serviceq.scheduleNext == q) {
            httpGetNextQueueForService(&q->conn->serviceq);
        }
        q->service(q);
        q->flags |= HTTP_QUEUE_SERVICED;
        q->servicing = 0;
    }
}


/*  
    Return true if the next queue will accept this packet. If not, then disable the queue's service procedure.
    This may split the packet if it exceeds the downstreams maximum packet size.
 */
bool httpWillNextQueueAcceptPacket(HttpQueue *q, HttpPacket *packet)
{
    HttpConn    *conn;
    HttpQueue   *next;
    int         size;

    conn = q->conn;
    next = q->nextQ;

    size = packet->content ? mprGetBufLength(packet->content) : 0;
    if (size == 0 || (size <= next->packetSize && (size + next->count) <= next->max)) {
        return 1;
    }
    if (httpResizePacket(q, packet, 0) < 0) {
        return 0;
    }
    size = httpGetPacketLength(packet);
    if (size <= next->packetSize && (size + next->count) <= next->max) {
        return 1;
    }

    /*  
        The downstream queue is full, so disable the queue and mark the downstream queue as full and service 
        if immediately if not disabled.  
     */
    httpDisableQueue(q);
    next->flags |= HTTP_QUEUE_FULL;
    if (!(next->flags & HTTP_QUEUE_DISABLED)) {
        httpScheduleQueue(next);
    }
    return 0;
}


/*  
    Write a block of data. This is the lowest level write routine for data. This will buffer the data and 
 */
int httpWriteBlock(HttpQueue *q, cchar *buf, int size)
{
    HttpPacket  *packet;
    HttpConn    *conn;
    HttpTx      *trans;
    int         bytes, written, packetSize;

    mprAssert(q == q->conn->writeq);
               
    conn = q->conn;
    trans = conn->tx;
    if (trans->finalized) {
        return MPR_ERR_CANT_WRITE;
    }
    for (written = 0; size > 0; ) {
        LOG(q, 6, "httpWriteBlock q_count %d, q_max %d", q->count, q->max);
        if (conn->state >= HTTP_STATE_COMPLETE) {
            return MPR_ERR_CANT_WRITE;
        }
        if (q->last != q->first && q->last->flags & HTTP_PACKET_DATA) {
            packet = q->last;
            mprAssert(packet->content);
        } else {
            packet = 0;
        }
        if (packet == 0 || mprGetBufSpace(packet->content) == 0) {
            packetSize = (trans->chunkSize > 0) ? trans->chunkSize : q->packetSize;
            if ((packet = httpCreateDataPacket(q, packetSize)) != 0) {
                httpPutForService(q, packet, 0);
            }
        }
        bytes = mprPutBlockToBuf(packet->content, buf, size);
        buf += bytes;
        size -= bytes;
        q->count += bytes;
        written += bytes;
    }
    if (q->count >= q->max) {
        httpFlushQueue(q, 0);
    }
    if (conn->error) {
        return MPR_ERR_CANT_WRITE;
    }
    return written;
}


int httpWriteString(HttpQueue *q, cchar *s)
{
    return httpWriteBlock(q, s, (int) strlen(s));
}


int httpWrite(HttpQueue *q, cchar *fmt, ...)
{
    va_list     vargs;
    char        *buf;
    int         rc;
    
    va_start(vargs, fmt);
    buf = mprVasprintf(q, -1, fmt, vargs);
    va_end(vargs);

    rc = httpWriteString(q, buf);
    mprFree(buf);
    return rc;
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
