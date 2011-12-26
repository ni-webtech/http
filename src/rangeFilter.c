/*
    rangeFilter.c - Ranged request filter.
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

static bool applyRange(HttpQueue *q, HttpPacket *packet);
static void createRangeBoundary(HttpConn *conn);
static HttpPacket *createRangePacket(HttpConn *conn, HttpRange *range);
static HttpPacket *createFinalRangePacket(HttpConn *conn);
static void outgoingRangeService(HttpQueue *q);
static bool fixRangeLength(HttpConn *conn);
static int matchRange(HttpConn *conn, HttpRoute *route, int dir);
static void startRange(HttpQueue *q);

/*********************************** Code *************************************/

int httpOpenRangeFilter(Http *http)
{
    HttpStage     *filter;

    mprLog(5, "Open range filter");
    if ((filter = httpCreateFilter(http, "rangeFilter", HTTP_STAGE_ALL, NULL)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    http->rangeFilter = filter;
    filter->match = matchRange; 
    filter->start = startRange; 
    filter->outgoingService = outgoingRangeService; 
    return 0;
}


static int matchRange(HttpConn *conn, HttpRoute *route, int dir)
{
    mprAssert(conn->rx);

    if ((dir & HTTP_STAGE_TX) && conn->tx->outputRanges) {
        return HTTP_ROUTE_OK;
    }
    return HTTP_ROUTE_REJECT;
}


static void startRange(HttpQueue *q)
{
    HttpConn    *conn;
    HttpTx      *tx;

    conn = q->conn;
    tx = conn->tx;
    /*
        The httpContentNotModified routine can set outputRanges to zero if returning not-modified.
     */
    if (tx->outputRanges == 0 || tx->status != HTTP_CODE_OK || !fixRangeLength(conn)) {
        httpRemoveQueue(q);
    } else {
        tx->status = HTTP_CODE_PARTIAL;
        if (tx->outputRanges->next) {
            createRangeBoundary(conn);
        }
    }
}


static void outgoingRangeService(HttpQueue *q)
{
    HttpPacket  *packet;
    HttpConn    *conn;
    HttpTx      *tx;

    conn = q->conn;
    tx = conn->tx;

    for (packet = httpGetPacket(q); packet; packet = httpGetPacket(q)) {
        if (packet->flags & HTTP_PACKET_DATA) {
            if (!applyRange(q, packet)) {
                return;
            }
        } else {
            /*
                Send headers and end packet downstream
             */
            if (packet->flags & HTTP_PACKET_END && tx->rangeBoundary) {
                httpPutPacketToNext(q, createFinalRangePacket(conn));
            }
            if (!httpWillNextQueueAcceptPacket(q, packet)) {
                httpPutBackPacket(q, packet);
                return;
            }
            httpPutPacketToNext(q, packet);
        }
    }
}


static bool applyRange(HttpQueue *q, HttpPacket *packet)
{
    HttpRange   *range;
    HttpConn    *conn;
    HttpTx      *tx;
    MprOff      endPacket, length, gap, span;
    ssize       count;

    conn = q->conn;
    tx = conn->tx;
    range = tx->currentRange;

    /*  
        Process the data packet over multiple ranges ranges until all the data is processed or discarded.
        A packet may contain data or it may be empty with an associated entityLength. If empty, range packets
        are filled with entity data as required.
     */
    while (range && packet) {
        length = httpGetPacketEntityLength(packet);
        if (length <= 0) {
            break;
        }
        endPacket = tx->rangePos + length;
        if (endPacket < range->start) {
            /* Packet is before the next range, so discard the entire packet and seek forwards */
            tx->rangePos += length;
            break;

        } else if (tx->rangePos < range->start) {
            /*  Packet starts before range so skip some data, but some packet data is in range */
            gap = (range->start - tx->rangePos);
            tx->rangePos += gap;
            if (gap < length) {
                httpAdjustPacketStart(packet, (ssize) gap);
            }
            /* Keep going and examine next range */

        } else {
            /* In range */
            mprAssert(range->start <= tx->rangePos && tx->rangePos < range->end);
            span = min(length, (range->end - tx->rangePos));
            count = (ssize) min(span, q->nextQ->packetSize);
            mprAssert(count > 0);
            if (!httpWillNextQueueAcceptSize(q, count)) {
                httpPutBackPacket(q, packet);
                return 0;
            }
            if (length > count) {
                /* Split packet if packet extends past range */
                httpPutBackPacket(q, httpSplitPacket(packet, count));
            }
            if (packet->fill && (*packet->fill)(q, packet, tx->rangePos, count) < 0) {
                return 0;
            }
            if (tx->rangeBoundary) {
                httpPutPacketToNext(q, createRangePacket(conn, range));
            }
            httpPutPacketToNext(q, packet);
            packet = 0;
            tx->rangePos += count;
        }
        if (tx->rangePos >= range->end) {
            tx->currentRange = range = range->next;
        }
    }
    return 1;
}


/*  
    Create a range boundary packet
 */
static HttpPacket *createRangePacket(HttpConn *conn, HttpRange *range)
{
    HttpPacket  *packet;
    HttpTx      *tx;
    char        *length;

    tx = conn->tx;

    length = (tx->entityLength >= 0) ? itos(tx->entityLength) : "*";
    packet = httpCreatePacket(HTTP_RANGE_BUFSIZE);
    packet->flags |= HTTP_PACKET_RANGE;
    mprPutFmtToBuf(packet->content, 
        "\r\n--%s\r\n"
        "Content-Range: bytes %Ld-%Ld/%s\r\n\r\n",
        tx->rangeBoundary, range->start, range->end - 1, length);
    return packet;
}


/*  
    Create a final range packet that follows all the data
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


/*  
    Create a range boundary. This is required if more than one range is requested.
 */
static void createRangeBoundary(HttpConn *conn)
{
    HttpTx      *tx;
    int         when;

    tx = conn->tx;
    mprAssert(tx->rangeBoundary == 0);
    when = (int) conn->http->now;
    tx->rangeBoundary = sfmt("%08X%08X", PTOI(tx) + PTOI(conn) * when, when);
}


/*  
    Ensure all the range limits are within the entity size limits. Fixup negative ranges.
 */
static bool fixRangeLength(HttpConn *conn)
{
    HttpTx      *tx;
    HttpRange   *range;
    MprOff      length;

    tx = conn->tx;
    length = tx->entityLength ? tx->entityLength : tx->length;

    for (range = tx->outputRanges; range; range = range->next) {
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
                    Can't compute an offset from the end as we don't know the entity length and it is not always possible
                    or wise to buffer all the output.
                 */
                httpError(conn, HTTP_CODE_RANGE_NOT_SATISFIABLE, "Can't compute end range with unknown content length"); 
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
