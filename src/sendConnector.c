/*
    sendConnector.c -- Send file connector. 

    The Sendfile connector supports the optimized transmission of whole static files. It uses operating system 
    sendfile APIs to eliminate reading the document into user space and multiple socket writes. The send connector 
    is not a general purpose connector. It cannot handle dynamic data or ranged requests. It does support chunked requests.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/**************************** Forward Declarations ****************************/
#if !BIT_FEATURE_ROMFS

static void addPacketForSend(HttpQueue *q, HttpPacket *packet);
static void adjustSendVec(HttpQueue *q, MprOff written);
static MprOff buildSendVec(HttpQueue *q);
static void adjustPacketData(HttpQueue *q, MprOff written);

/*********************************** Code *************************************/

int httpOpenSendConnector(Http *http)
{
    HttpStage     *stage;

    mprLog(5, "Open send connector");
    if ((stage = httpCreateConnector(http, "sendConnector", HTTP_STAGE_ALL, NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    stage->open = httpSendOpen;
    stage->close = httpSendClose;
    stage->outgoingService = httpSendOutgoingService; 
    http->sendConnector = stage;
    return 0;
}


/*  
    Initialize the send connector for a request
 */
void httpSendOpen(HttpQueue *q)
{
    HttpConn    *conn;
    HttpTx      *tx;

    conn = q->conn;
    tx = conn->tx;

    if (tx->connector != conn->http->sendConnector) {
        httpAssignQueue(q, tx->connector, HTTP_QUEUE_TX);
        tx->connector->open(q);
        return;
    }
    if (!(tx->flags & HTTP_TX_NO_BODY)) {
        mprAssert(tx->fileInfo.valid);
        if (tx->fileInfo.size > conn->limits->transmissionBodySize) {
            httpError(conn, HTTP_ABORT | HTTP_CODE_REQUEST_TOO_LARGE,
                "Http transmission aborted. File size exceeds max body of %,Ld bytes", conn->limits->transmissionBodySize);
            return;
        }
        tx->file = mprOpenFile(tx->filename, O_RDONLY | O_BINARY, 0);
        if (tx->file == 0) {
            httpError(conn, HTTP_CODE_NOT_FOUND, "Can't open document: %s, err %d", tx->filename, mprGetError());
        }
    }
}


void httpSendClose(HttpQueue *q)
{
    HttpTx  *tx;

    tx = q->conn->tx;
    if (tx->file) {
        mprCloseFile(tx->file);
        tx->file = 0;
    }
}


void httpSendOutgoingService(HttpQueue *q)
{
    HttpConn    *conn;
    HttpTx      *tx;
    MprFile     *file;
    MprOff      written;
    int         errCode;

    conn = q->conn;
    tx = conn->tx;
    conn->lastActivity = conn->http->now;
    mprAssert(conn->sock);

    if (!conn->sock || conn->connectorComplete) {
        return;
    }
    if (tx->flags & HTTP_TX_NO_BODY) {
        httpDiscardQueueData(q, 1);
    }
    if ((tx->bytesWritten + q->ioCount) > conn->limits->transmissionBodySize) {
        httpError(conn, HTTP_ABORT | HTTP_CODE_REQUEST_TOO_LARGE | ((tx->bytesWritten) ? HTTP_ABORT : 0),
            "Http transmission aborted. Exceeded max body of %,Ld bytes", conn->limits->transmissionBodySize);
        if (tx->bytesWritten) {
            httpConnectorComplete(conn);
            return;
        }
    }
    /*
        Loop doing non-blocking I/O until blocked or all the packets received are written.
     */
    while (1) {
        /*
            Rebuild the iovector only when the past vector has been completely written. Simplifies the logic quite a bit.
         */
        if (q->ioIndex == 0 && buildSendVec(q) <= 0) {
            break;
        }
        file = q->ioFile ? tx->file : 0;
        written = mprSendFileToSocket(conn->sock, file, q->ioPos, q->ioCount, q->iovec, q->ioIndex, NULL, 0);

        mprLog(8, "Send connector ioCount %d, wrote %Ld, written so far %Ld, sending file %d, q->count %d/%d", 
                q->ioCount, written, tx->bytesWritten, q->ioFile, q->count, q->max);
        if (written < 0) {
            errCode = mprGetError(q);
            if (errCode == EAGAIN || errCode == EWOULDBLOCK) {
                /* Socket is full. Wait for an I/O event */
                httpSocketBlocked(conn);
                break;
            }
            if (errCode != EPIPE && errCode != ECONNRESET) {
                mprLog(7, "SendFileToSocket failed, errCode %d", errCode);
            }
            httpError(conn, HTTP_ABORT | HTTP_CODE_COMMS_ERROR, "SendFileToSocket failed, errCode %d", errCode);
            httpConnectorComplete(conn);
            break;

        } else if (written == 0) {
            /* Socket is full. Wait for an I/O event */
            httpSocketBlocked(conn);
            break;

        } else if (written > 0) {
            tx->bytesWritten += written;
            adjustPacketData(q, written);
            adjustSendVec(q, written);
        }
    }
    if (q->ioCount == 0) {
        if ((q->flags & HTTP_QUEUE_EOF)) {
            httpConnectorComplete(conn);
        } else {
            httpNotifyWritable(conn);
        }
    }
}


/*  
    Build the IO vector. This connector uses the send file API which permits multiple IO blocks to be written with 
    file data. This is used to write transfer the headers and chunk encoding boundaries. Return the count of bytes to 
    be written. Return -1 for EOF.
 */
static MprOff buildSendVec(HttpQueue *q)
{
    HttpConn    *conn;
    HttpPacket  *packet;

    mprAssert(q->ioIndex == 0);

    conn = q->conn;
    q->ioCount = 0;
    q->ioFile = 0;

    /*  
        Examine each packet and accumulate as many packets into the I/O vector as possible. Can only have one data packet at
        a time due to the limitations of the sendfile API (on Linux). And the data packet must be after all the 
        vector entries. Leave the packets on the queue for now, they are removed after the IO is complete for the 
        entire packet.
     */
    for (packet = q->first; packet; packet = packet->next) {
        if (packet->flags & HTTP_PACKET_HEADER) {
            httpWriteHeaders(conn, packet);
            q->count += httpGetPacketLength(packet);
            
        } else if (httpGetPacketLength(packet) == 0 && packet->esize == 0) {
            q->flags |= HTTP_QUEUE_EOF;
            if (packet->prefix == NULL) {
                break;
            }
        }
        if (q->ioFile || q->ioIndex >= (HTTP_MAX_IOVEC - 2)) {
            break;
        }
        addPacketForSend(q, packet);
    }
    return q->ioCount;
}


/*  
    Add one entry to the io vector
 */
static void addToSendVector(HttpQueue *q, char *ptr, ssize bytes)
{
    mprAssert(ptr > 0);
    mprAssert(bytes > 0);

    q->iovec[q->ioIndex].start = ptr;
    q->iovec[q->ioIndex].len = bytes;
    q->ioCount += bytes;
    q->ioIndex++;
}


/*  
    Add a packet to the io vector. Return the number of bytes added to the vector.
 */
static void addPacketForSend(HttpQueue *q, HttpPacket *packet)
{
    HttpTx      *tx;
    HttpConn    *conn;
    int         item;

    conn = q->conn;
    tx = conn->tx;
    
    mprAssert(q->count >= 0);
    mprAssert(q->ioIndex < (HTTP_MAX_IOVEC - 2));

    if (packet->prefix) {
        addToSendVector(q, mprGetBufStart(packet->prefix), mprGetBufLength(packet->prefix));
    }
    if (packet->esize > 0) {
        mprAssert(q->ioFile == 0);
        q->ioFile = 1;
        q->ioCount += packet->esize;

    } else if (httpGetPacketLength(packet) > 0) {
        /*
            Header packets have actual content. File data packets are virtual and only have a count.
         */
        addToSendVector(q, mprGetBufStart(packet->content), httpGetPacketLength(packet));
        item = (packet->flags & HTTP_PACKET_HEADER) ? HTTP_TRACE_HEADER : HTTP_TRACE_BODY;
        if (httpShouldTrace(conn, HTTP_TRACE_TX, item, tx->ext) >= 0) {
            httpTraceContent(conn, HTTP_TRACE_TX, item, packet, 0, tx->bytesWritten);
        }
    }
}


/*  
    Clear entries from the IO vector that have actually been transmitted. This supports partial writes due to the socket
    being full. Don't come here if we've seen all the packets and all the data has been completely written. ie. small files
    don't come here.
 */
static void adjustPacketData(HttpQueue *q, MprOff bytes)
{
    HttpPacket  *packet;
    ssize       len;

    mprAssert(q->first);
    mprAssert(q->count >= 0);
    mprAssert(bytes >= 0);

    while ((packet = q->first) != 0) {
        if (packet->prefix) {
            len = mprGetBufLength(packet->prefix);
            len = (ssize) min(len, bytes);
            mprAdjustBufStart(packet->prefix, len);
            bytes -= len;
            /* Prefixes don't count in the q->count. No need to adjust */
            if (mprGetBufLength(packet->prefix) == 0) {
                packet->prefix = 0;
            }
        }
        if (packet->esize) {
            len = (ssize) min(packet->esize, bytes);
            packet->esize -= len;
            packet->epos += len;
            bytes -= len;
            mprAssert(packet->esize >= 0);
            mprAssert(bytes == 0);
            if (packet->esize > 0) {
                break;
            }
        } else if ((len = httpGetPacketLength(packet)) > 0) {
            len = (ssize) min(len, bytes);
            mprAdjustBufStart(packet->content, len);
            bytes -= len;
            q->count -= len;
            mprAssert(q->count >= 0);
        }
        if (httpGetPacketLength(packet) == 0) {
            httpGetPacket(q);
        }
        mprAssert(bytes >= 0);
        if (bytes == 0 && (q->first == NULL || !(q->first->flags & HTTP_PACKET_END))) {
            break;
        }
    }
}


/*  
    Clear entries from the IO vector that have actually been transmitted. This supports partial writes due to the socket
    being full. Don't come here if we've seen all the packets and all the data has been completely written. ie. small files
    don't come here.
 */
static void adjustSendVec(HttpQueue *q, MprOff written)
{
    MprIOVec    *iovec;
    ssize       len;
    int         i, j;

    iovec = q->iovec;
    for (i = 0; i < q->ioIndex; i++) {
        len = iovec[i].len;
        if (written < len) {
            iovec[i].start += (ssize) written;
            iovec[i].len -= (ssize) written;
            return;
        }
        written -= len;
        q->ioCount -= len;
        for (j = i + 1; i < q->ioIndex; ) {
            iovec[j++] = iovec[i++];
        }
        q->ioIndex--;
        i--;
    }
    if (written > 0 && q->ioFile) {
        /* All remaining data came from the file */
        q->ioPos += written;
    }
    q->ioIndex = 0;
    q->ioCount = 0;
    q->ioFile = 0;
}


#else
int httpOpenSendConnector(Http *http) { return 0; }
void httpSendOpen(HttpQueue *q) {}
void httpSendOutgoingService(HttpQueue *q) {}

#endif /* !BIT_FEATURE_ROMFS */
/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2012. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You may use the GPL open source license described below or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound 
    by the terms of either license. Consult the LICENSE.TXT distributed with 
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
