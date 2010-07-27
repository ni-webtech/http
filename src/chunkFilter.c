/*
    chunkFilter.c - Transfer chunk endociding filter.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

static void incomingChunkData(HttpQueue *q, HttpPacket *packet);
static bool matchChunk(HttpConn *conn, HttpStage *handler);
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

    filter = httpCreateFilter(http, "chunkFilter", HTTP_STAGE_ALL);
    if (filter == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    http->chunkFilter = filter;
    filter->open = openChunk; 
    filter->match = matchChunk; 
    filter->outgoingService = outgoingChunkService; 
    filter->incomingData = incomingChunkData; 
    return 0;
}


static bool matchChunk(HttpConn *conn, HttpStage *handler)
{
    return (conn->transmitter->length <= 0) ? 1 : 0;
}


static void openChunk(HttpQueue *q)
{
    HttpConn        *conn;
    HttpReceiver    *rec;

    conn = q->conn;
    rec = conn->receiver;

    q->packetSize = min(conn->limits->chunkSize, q->max);
    rec->chunkState = HTTP_CHUNK_START;
}


/*  
    Get the next chunk size. Chunked data format is:
        Chunk spec <CRLF>
        Data <CRLF>
        Chunk spec (size == 0) <CRLF>
        <CRLF>
    Chunk spec is: "HEX_COUNT; chunk length DECIMAL_COUNT\r\n". The "; chunk length DECIMAL_COUNT is optional.
    As an optimization, use "\r\nSIZE ...\r\n" as the delimiter so that the CRLF after data does not special consideration.
    Achive this by parseHeaders reversing the input start by 2.
 */
static void incomingChunkData(HttpQueue *q, HttpPacket *packet)
{
    HttpConn        *conn;
    HttpReceiver    *rec;
    MprBuf          *buf;
    char            *start, *cp;
    int             bad;

    conn = q->conn;
    rec = conn->receiver;

    if (!(rec->flags & HTTP_REC_CHUNKED)) {
        httpSendPacketToNext(q, packet);
        return;
    }
    buf = packet->content;

    if (packet->content == 0) {
        if (rec->chunkState == HTTP_CHUNK_DATA) {
            httpProtocolError(conn, HTTP_CODE_BAD_REQUEST, "Bad chunk state");
            return;
        }
        rec->chunkState = HTTP_CHUNK_EOF;
    }
    
    /*  
        NOTE: the request head ensures that packets are correctly sized by packet inspection. The packet will never
        have more data than the chunk state expects.
     */
    switch (rec->chunkState) {
    case HTTP_CHUNK_START:
        /*  
            Validate:  "\r\nSIZE.*\r\n"
         */
        if (mprGetBufLength(buf) < 5) {
            break;
        }
        start = mprGetBufStart(buf);
        bad = (start[0] != '\r' || start[1] != '\n');
        for (cp = &start[2]; cp < buf->end && *cp != '\n'; cp++) {}
        if (*cp != '\n' && (cp - start) < 80) {
            break;
        }
        bad += (cp[-1] != '\r' || cp[0] != '\n');
        if (bad) {
            httpProtocolError(conn, HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
            return;
        }
        rec->chunkSize = (int) mprAtoi(&start[2], 16);
        if (!isxdigit((int) start[2]) || rec->chunkSize < 0) {
            httpProtocolError(conn, HTTP_CODE_BAD_REQUEST, "Bad chunk specification");
            return;
        }
        mprAdjustBufStart(buf, cp - start + 1);
        rec->remainingContent = rec->chunkSize;
        if (rec->chunkSize == 0) {
            rec->chunkState = HTTP_CHUNK_EOF;
            /*
                We are lenient if the request does not have a trailing "\r\n" after the last chunk
             */
            cp = mprGetBufStart(buf);
            if (mprGetBufLength(buf) == 2 && *cp == '\r' && cp[1] == '\n') {
                mprAdjustBufStart(buf, 2);
            }
        } else {
            rec->chunkState = HTTP_CHUNK_DATA;
        }
        mprAssert(mprGetBufLength(buf) == 0);
        httpFreePacket(q, packet);
        mprLog(q, 5, "chunkFilter: start incoming chunk of %d bytes", rec->chunkSize);
        break;

    case HTTP_CHUNK_DATA:
        mprAssert(httpGetPacketLength(packet) <= rec->chunkSize);
        mprLog(q, 5, "chunkFilter: data %d bytes, rec->remainingContent %d", httpGetPacketLength(packet), 
            rec->remainingContent);
        httpSendPacketToNext(q, packet);
        if (rec->remainingContent == 0) {
            rec->chunkState = HTTP_CHUNK_START;
            rec->remainingContent = HTTP_BUFSIZE;
        }
        break;

    case HTTP_CHUNK_EOF:
        mprAssert(httpGetPacketLength(packet) == 0);
        httpSendPacketToNext(q, packet);
        mprLog(q, 5, "chunkFilter: last chunk");
        break;    

    default:
        mprAssert(0);
    }
}


/*  
    Apply chunks to dynamic outgoing data. 
 */
static void outgoingChunkService(HttpQueue *q)
{
    HttpConn        *conn;
    HttpPacket      *packet;
    HttpTransmitter *trans;

    conn = q->conn;
    trans = conn->transmitter;

    if (!(q->flags & HTTP_QUEUE_SERVICED)) {
        /*  
            If the last packet is the end packet, we have all the data. Thus we know the actual content length 
            and can bypass the chunk handler.
         */
        if (q->last->flags & HTTP_PACKET_END) {
            //  MOB -- but what if a content-length header has been defined but not set trans->length
            if (trans->chunkSize < 0 && trans->length <= 0) {
                /*  
                    Set the response content length and thus disable chunking -- not needed as we know the entity length.
                 */
                trans->length = q->count;
            }
        } else {
            if (trans->chunkSize < 0) {
                trans->chunkSize = min(conn->limits->chunkSize, q->max);
            }
        }
    }
    if (trans->chunkSize <= 0 || trans->altBody) {
        httpDefaultOutgoingServiceStage(q);
    } else {
        for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
            if (!(packet->flags & HTTP_PACKET_HEADER)) {
                httpJoinPackets(q, trans->chunkSize);
                if (httpGetPacketLength(packet) > trans->chunkSize) {
                    httpResizePacket(q, packet, trans->chunkSize);
                }
            }
            if (!httpWillNextQueueAcceptPacket(q, packet)) {
                httpPutBackPacket(q, packet);
                return;
            }
            if (!(packet->flags & HTTP_PACKET_HEADER)) {
                setChunkPrefix(q, packet);
            }
            httpSendPacketToNext(q, packet);
        }
    }
}


static void setChunkPrefix(HttpQueue *q, HttpPacket *packet)
{
    HttpConn      *conn;

    conn = q->conn;

    if (packet->prefix) {
        return;
    }
    packet->prefix = mprCreateBuf(packet, 32, 32);
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
