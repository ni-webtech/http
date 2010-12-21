/*
    rangeFilter.c - Ranged request filter.
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

static void createRangeBoundary(HttpConn *conn);
static HttpPacket *createRangePacket(HttpConn *conn, HttpRange *range);
static HttpPacket *createFinalRangePacket(HttpConn *conn);
static void incomingRangeData(HttpQueue *q, HttpPacket *packet);
static void outgoingRangeService(HttpQueue *q);
static bool fixRangeLength(HttpConn *conn);
static bool matchRange(HttpConn *conn, HttpStage *handler);
static void rangeService(HttpQueue *q, HttpRangeProc fill);

/*********************************** Code *************************************/

int httpOpenRangeFilter(Http *http)
{
    HttpStage     *filter;

    filter = httpCreateFilter(http, "rangeFilter", HTTP_STAGE_ALL);
    if (filter == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    http->rangeFilter = filter;
    http->rangeService = rangeService;
    filter->match = matchRange; 
    filter->outgoingService = outgoingRangeService; 
    filter->incomingData = incomingRangeData; 
    return 0;
}


static bool matchRange(HttpConn *conn, HttpStage *handler)
{
    return (conn->rx->ranges) ? 1 : 0;;
}


/*
    The RangeFilter does nothing for incoming data. The rx understands range headers
 */
static void incomingRangeData(HttpQueue *q, HttpPacket *packet)
{
    httpSendPacketToNext(q, packet);
}


/*  
    Apply ranges to outgoing data. 
 */
static void rangeService(HttpQueue *q, HttpRangeProc fill)
{
    HttpPacket  *packet;
    HttpRange   *range;
    HttpConn    *conn;
    HttpRx      *rx;
    HttpTx      *tx;
    ssize       bytes, count, endpos;

    conn = q->conn;
    rx = conn->rx;
    tx = conn->tx;
    range = tx->currentRange;

    if (!(q->flags & HTTP_QUEUE_SERVICED)) {
        if (tx->entityLength < 0 && q->last->flags & HTTP_PACKET_END) {

           /*   Compute an entity length. This allows negative ranges computed from the end of the data.
            */
           tx->entityLength = q->count;
        }
        if (tx->status != HTTP_CODE_OK || !fixRangeLength(conn)) {
            httpSendPackets(q);
            httpRemoveQueue(q);
            return;
        }
        if (rx->ranges->next) {
            createRangeBoundary(conn);
        }
        tx->status = HTTP_CODE_PARTIAL;
    }

    for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
        if (!(packet->flags & HTTP_PACKET_DATA)) {
            if (packet->flags & HTTP_PACKET_END && tx->rangeBoundary) {
                httpSendPacketToNext(q, createFinalRangePacket(conn));
            }
            if (!httpWillNextQueueAcceptPacket(q, packet)) {
                httpPutBackPacket(q, packet);
                return;
            }
            httpSendPacketToNext(q, packet);
            continue;
        }

        /*  Process the current packet over multiple ranges ranges until all the data is processed or discarded.
         */
        bytes = packet->content ? mprGetBufLength(packet->content) : packet->entityLength;
        while (range && bytes > 0) {

            endpos = tx->pos + bytes;
            if (endpos < range->start) {
                /* Packet is before the next range, so discard the entire packet */
                tx->pos += bytes;
                httpFreePacket(q, packet);
                break;

            } else if (tx->pos > range->end) {
                /* Missing some output - should not happen */
                mprAssert(0);

            } else if (tx->pos < range->start) {
                /*  Packets starts before range with some data in range so skip some data */
                count = (range->start - tx->pos);
                bytes -= count;
                tx->pos += count;
                if (packet->content == 0) {
                    packet->entityLength -= count;
                }
                if (packet->content) {
                    mprAdjustBufStart(packet->content, count);
                }
                continue;

            } else {
                /* In range */
                mprAssert(range->start <= tx->pos && tx->pos < range->end);
                count = min(bytes, (int) (range->end - tx->pos));
                count = min(count, q->nextQ->packetSize);
                mprAssert(count > 0);
                if (count < bytes) {
                    //  TODO OPT> Only need to resize if this completes all the range data.
                    httpResizePacket(q, packet, count);
                }
                if (!httpWillNextQueueAcceptPacket(q, packet)) {
                    httpPutBackPacket(q, packet);
                    return;
                }
                if (fill) {
                    if ((*fill)(q, packet) < 0) {
                        return;
                    }
                }
                bytes -= count;
                tx->pos += count;
                if (tx->rangeBoundary) {
                    httpSendPacketToNext(q, createRangePacket(conn, range));
                }
                httpSendPacketToNext(q, packet);
                if (tx->pos >= range->end) {
                    range = range->next;
                }
                break;
            }
        }
    }
    tx->currentRange = range;
}


static void outgoingRangeService(HttpQueue *q)
{
    rangeService(q, NULL);
}


/*  Create a range boundary packet
 */
static HttpPacket *createRangePacket(HttpConn *conn, HttpRange *range)
{
    HttpPacket  *packet;
    HttpTx      *tx;
    char        lenBuf[16];

    tx = conn->tx;

    if (tx->entityLength >= 0) {
        itos(lenBuf, sizeof(lenBuf), tx->entityLength, 10);
    } else {
        lenBuf[0] = '*';
        lenBuf[1] = '\0';
    }
    packet = httpCreatePacket(HTTP_RANGE_BUFSIZE);
    packet->flags |= HTTP_PACKET_RANGE;
    mprPutFmtToBuf(packet->content, 
        "\r\n--%s\r\n"
        "Content-Range: bytes %d-%d/%s\r\n\r\n",
        tx->rangeBoundary, range->start, range->end - 1, lenBuf);
    return packet;
}


/*  Create a final range packet that follows all the data
 */
static HttpPacket *createFinalRangePacket(HttpConn *conn)
{
    HttpPacket  *packet;
    HttpTx      *tx;

    tx = conn->tx;

    packet = httpCreatePacket(HTTP_RANGE_BUFSIZE);
    packet->flags |= HTTP_PACKET_RANGE;
    mprPutFmtToBuf(packet->content, "\r\n--%s--\r\n", tx->rangeBoundary);
    return packet;
}


/*  Create a range boundary. This is required if more than one range is requested.
 */
static void createRangeBoundary(HttpConn *conn)
{
    HttpTx      *tx;

    tx = conn->tx;
    mprAssert(tx->rangeBoundary == 0);
    tx->rangeBoundary = mprAsprintf("%08X%08X", PTOI(tx) + PTOI(conn) * (int) conn->time, (int) conn->time);
}


/*  Ensure all the range limits are within the entity size limits. Fixup negative ranges.
 */
static bool fixRangeLength(HttpConn *conn)
{
    HttpRx      *rx;
    HttpTx      *tx;
    HttpRange   *range;
    ssize       length;

    rx = conn->rx;
    tx = conn->tx;
    length = tx->entityLength;

    for (range = rx->ranges; range; range = range->next) {
        /*
                Range: 0-49             first 50 bytes
                Range: 50-99,200-249    Two 50 byte ranges from 50 and 200
                Range: -50              Last 50 bytes
                Range: 1-               Skip first byte then emit the rest
         */
        if (length) {
            if (range->end > length) {
                range->end = length;
            }
            if (range->start > length) {
                range->start = length;
            }
        }
        if (range->start < 0) {
            if (length <= 0) {
                /*
                    Can't compute an offset from the end as we don't know the entity length
                 */
                return 0;
            }
            /* select last -range-end bytes */
            range->start = length - range->end + 1;
            range->end = length;
        }
        if (range->end < 0) {
            if (length <= 0) {
                return 0;
            }
            range->end = length - range->end - 1;
        }
        range->len = (int) (range->end - range->start);
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
