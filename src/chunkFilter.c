/*
    chunkFilter.c - Transfer chunk endociding filter.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

static int matchChunk(HttpConn *conn, HttpRoute *route, int dir);
static void openChunk(HttpQueue *q);
static void outgoingChunkService(HttpQueue *q);
static void setChunkPrefix(HttpQueue *q, HttpPacket *packet);

/*********************************** Code *************************************/
/* 
   Loadable module initialization
 */
int httpOpenChunkFilter(Http *http)
{
    HttpStage     *filter;

    mprLog(5, "Open chunk filter");
    if ((filter = httpCreateFilter(http, "chunkFilter", HTTP_STAGE_ALL, NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    http->chunkFilter = filter;
    filter->match = matchChunk; 
    filter->open = openChunk; 
    filter->outgoingService = outgoingChunkService; 
    return 0;
}


static int matchChunk(HttpConn *conn, HttpRoute *route, int dir)
{
    HttpTx  *tx;

    if (dir & HTTP_STAGE_TX) {
        /*
            Don't match if chunking is explicitly turned off vi a the X_APPWEB_CHUNK_SIZE header which sets the chunk 
            size to zero. Also remove if the response length is already known.
         */
        tx = conn->tx;
        if (tx->length < 0 && tx->chunkSize != 0) {
            return HTTP_ROUTE_OK;
        }
        return HTTP_ROUTE_REJECT;
    }
    /* 
        Must always be ready to handle chunked response data. Clients create their incoming pipeline before it is
        know what the response data looks like (chunked or not).
     */
    return HTTP_ROUTE_OK;
}


static void openChunk(HttpQueue *q)
{
    HttpConn    *conn;

    conn = q->conn;
    q->packetSize = min(conn->limits->chunkSize, q->max);
}


/*  
    Filter chunk headers and leave behind pure data. This is called for chunked and unchunked data.
    Chunked data format is:
        Chunk spec <CRLF>
        Data <CRLF>
        Chunk spec (size == 0) <CRLF>
        <CRLF>
    Chunk spec is: "HEX_COUNT; chunk length DECIMAL_COUNT\r\n". The "; chunk length DECIMAL_COUNT is optional.
    As an optimization, use "\r\nSIZE ...\r\n" as the delimiter so that the CRLF after data does not special consideration.
    Achive this by parseHeaders reversing the input start by 2.
 */
ssize httpFilterChunkData(HttpQueue *q, HttpPacket *packet)
{
    HttpConn    *conn;
    HttpRx      *rx;
    MprBuf      *buf;
    ssize       chunkSize;
    char        *start, *cp;
    int         bad;

    conn = q->conn;
    rx = conn->rx;
    mprAssert(packet);
    buf = packet->content;
    mprAssert(buf);

    switch (rx->chunkState) {
    case HTTP_CHUNK_UNCHUNKED:
        return (ssize) min(rx->remainingContent, mprGetBufLength(buf));

    case HTTP_CHUNK_DATA:
        mprLog(7, "chunkFilter: data %d bytes, rx->remainingContent %d", httpGetPacketLength(packet), rx->remainingContent);
        if (rx->remainingContent > 0) {
            return (ssize) min(rx->remainingContent, mprGetBufLength(buf));
        }
        /* End of chunk - prep for the next chunk */
        rx->remainingContent = HTTP_BUFSIZE;
        rx->chunkState = HTTP_CHUNK_START;
        /* Fall through */

    case HTTP_CHUNK_START:
        /*  
            Validate:  "\r\nSIZE.*\r\n"
         */
        if (mprGetBufLength(buf) < 5) {
            return MPR_ERR_NOT_READY;
        }
        start = mprGetBufStart(buf);
        bad = (start[0] != '\r' || start[1] != '\n');
        for (cp = &start[2]; cp < buf->end && *cp != '\n'; cp++) {}
        if (*cp != '\n' && (cp - start) < 80) {
            return MPR_ERR_NOT_READY;
        }
        bad += (cp[-1] != '\r' || cp[0] != '\n');
        if (bad) {
            httpError(conn, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
            return 0;
        }
        chunkSize = (int) stoiradix(&start[2], 16, NULL);
        if (!isxdigit((int) start[2]) || chunkSize < 0) {
            httpError(conn, HTTP_ABORT | HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
            return 0;
        }
        mprAdjustBufStart(buf, (cp - start + 1));
        /* Remaining content is set to the next chunk size */
        rx->remainingContent = chunkSize;
        if (chunkSize == 0) {
            /*
                We are lenient if the request does not have a trailing "\r\n" after the last chunk
             */
            cp = mprGetBufStart(buf);
            if (mprGetBufLength(buf) == 2 && *cp == '\r' && cp[1] == '\n') {
                mprAdjustBufStart(buf, 2);
            }
            rx->chunkState = HTTP_CHUNK_EOF;
            rx->eof = 1;
        } else {
            rx->chunkState = HTTP_CHUNK_DATA;
        }
        mprLog(7, "chunkFilter: start incoming chunk of %d bytes", chunkSize);
        return min(chunkSize, mprGetBufLength(buf));

    default:
        mprError("chunkFilter: bad state %d", rx->chunkState);
    }
    return 0;
}


static void outgoingChunkService(HttpQueue *q)
{
    HttpConn    *conn;
    HttpPacket  *packet;
    HttpTx      *tx;
    cchar       *value;

    conn = q->conn;
    tx = conn->tx;

    if (!(q->flags & HTTP_QUEUE_SERVICED)) {
        /*  
            If we don't know the content length (tx->length < 0) and if the last packet is the end packet. Then
            we have all the data. Thus we can determine the actual content length and can bypass the chunk handler.
         */
        if (tx->length < 0 && (value = mprLookupKey(tx->headers, "Content-Length")) != 0) {
            tx->length = stoi(value);
        }
        if (tx->length < 0) {
            if (q->last->flags & HTTP_PACKET_END) {
                if (tx->chunkSize < 0 && tx->length <= 0) {
                    /*  
                        Set the response content length and thus disable chunking -- not needed as we know the entity length.
                     */
                    tx->length = (int) q->count;
                }
            } else {
                if (tx->chunkSize < 0) {
                    tx->chunkSize = min(conn->limits->chunkSize, q->max);
                }
            }
        }
    }
    if (tx->chunkSize <= 0) {
        httpDefaultOutgoingServiceStage(q);
    } else {
        for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
            if (!(packet->flags & HTTP_PACKET_HEADER)) {
                httpPutBackPacket(q, packet);
                httpJoinPackets(q, tx->chunkSize);
                packet = httpGetPacket(q);
                if (httpGetPacketLength(packet) > tx->chunkSize) {
                    httpResizePacket(q, packet, tx->chunkSize);
                }
            }
            if (!httpWillNextQueueAcceptPacket(q, packet)) {
                httpPutBackPacket(q, packet);
                return;
            }
            if (!(packet->flags & HTTP_PACKET_HEADER)) {
                setChunkPrefix(q, packet);
            }
            httpPutPacketToNext(q, packet);
        }
    }
}


static void setChunkPrefix(HttpQueue *q, HttpPacket *packet)
{
    if (packet->prefix) {
        return;
    }
    packet->prefix = mprCreateBuf(32, 32);
    /*  
        NOTE: prefixes don't count in the queue length. No need to adjust q->count
     */
    if (httpGetPacketLength(packet)) {
        mprPutFmtToBuf(packet->prefix, "\r\n%x\r\n", httpGetPacketLength(packet));
    } else {
        mprPutStringToBuf(packet->prefix, "\r\n0\r\n\r\n");
    }
}


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
