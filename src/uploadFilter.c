/*
    uploadFilter.c - Upload file filter.
    The upload filter processes post data according to RFC-1867 ("multipart/form-data" post data). 
    It saves the uploaded files in a configured upload directory.
    Copyright (c) All Rights Reserved. See copyright notice at the bottom of the file.
 */

/********************************** Includes **********************************/

#include    "http.h"

/*********************************** Locals ***********************************/
/*
    Upload state machine states
 */
#define HTTP_UPLOAD_REQUEST_HEADER        1   /* Request header */
#define HTTP_UPLOAD_BOUNDARY              2   /* Boundary divider */
#define HTTP_UPLOAD_CONTENT_HEADER        3   /* Content part header */
#define HTTP_UPLOAD_CONTENT_DATA          4   /* Content encoded data */
#define HTTP_UPLOAD_CONTENT_END           5   /* End of multipart message */

/*
    Per upload context
 */
typedef struct Upload {
    HttpUploadFile  *currentFile;       /* Current file context */
    MprFile         *file;              /* Current file I/O object */
    char            *boundary;          /* Boundary signature */
    int             boundaryLen;        /* Length of boundary */
    int             contentState;       /* Input states */
    char            *clientFilename;    /* Current file filename */
    char            *tmpPath;           /* Current temp filename for upload data */
    char            *id;                /* Current name keyword value */
} Upload;

/********************************** Forwards **********************************/

static void closeUpload(HttpQueue *q);
static char *getBoundary(void *buf, int bufLen, void *boundary, int boundaryLen);
static void incomingUploadData(HttpQueue *q, HttpPacket *packet);
static bool matchUpload(HttpConn *conn, HttpStage *filter);
static void openUpload(HttpQueue *q);
static int  processContentBoundary(HttpQueue *q, char *line);
static int  processContentHeader(HttpQueue *q, char *line);
static int  processContentData(HttpQueue *q);

/************************************* Code ***********************************/

int httpOpenUploadFilter(Http *http)
{
    HttpStage     *filter;

    filter = httpCreateFilter(http, "uploadFilter", HTTP_STAGE_ALL);
    if (filter == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    http->uploadFilter = filter;
    filter->match = matchUpload; 
    filter->open = openUpload; 
    filter->close = closeUpload; 
    filter->incomingData = incomingUploadData; 
    return 0;
}


/*  
    Match if this request needs the upload filter. Return true if needed.
 */
static bool matchUpload(HttpConn *conn, HttpStage *filter)
{
    HttpReceiver    *rec;
    char            *pat;
    int             len;
    
    rec = conn->receiver;
    if (!(rec->flags & HTTP_POST) || rec->remainingContent <= 0) {
        return 0;
    }
    if (rec->location && rec->location->uploadDir == NULL) {
        return 0;
    }
    pat = "multipart/form-data";
    len = (int) strlen(pat);
    if (mprStrcmpAnyCaseCount(rec->mimeType, pat, len) == 0) {
        rec->upload = 1;
        return 1;
    } else {
        return 0;
    }   
}


/*  
    Initialize the upload filter for a new request
 */
static void openUpload(HttpQueue *q)
{
    HttpConn        *conn;
    HttpTransmitter *trans;
    HttpReceiver    *rec;
    Upload          *up;
    char            *boundary;

    conn = q->conn;
    trans = conn->transmitter;
    rec = conn->receiver;

    up = mprAllocObjZeroed(trans, Upload);
    if (up == 0) {
        return;
    }
    q->queueData = up;
    up->contentState = HTTP_UPLOAD_BOUNDARY;

    if (rec->uploadDir == 0) {
#if BLD_WIN_LIKE
        rec->uploadDir = mprGetNormalizedPath(rec, getenv("TEMP"));
#else
        rec->uploadDir = mprStrdup(rec, "/tmp");
#endif
    }
    if ((boundary = strstr(rec->mimeType, "boundary=")) != 0) {
        boundary += 9;
        up->boundary = mprStrcat(up, -1, "--", boundary, NULL);
        up->boundaryLen = strlen(up->boundary);
    }
    if (up->boundaryLen == 0 || *up->boundary == '\0') {
        httpError(conn, HTTP_CODE_BAD_REQUEST, "Bad boundary");
        return;
    }
    httpSetFormVar(conn, "UPLOAD_DIR", rec->uploadDir);
}


/*  
    Cleanup when the entire request has complete
 */
static void closeUpload(HttpQueue *q)
{
    HttpUploadFile  *file;
    HttpReceiver    *rec;
    Upload          *up;

    rec = q->conn->receiver;
    up = q->queueData;
    
    if (up->currentFile) {
        file = up->currentFile;
        mprDeletePath(q->conn, file->filename);
        file->filename = 0;
        mprFree(up->file);
    }
    if (rec->autoDelete) {
        httpRemoveAllUploadedFiles(q->conn);
    }
}


/*  
    Incoming data acceptance routine. The service queue is used, but not a service routine as the data is processed
    immediately. Partial data is buffered on the service queue until a correct mime boundary is seen.
 */
static void incomingUploadData(HttpQueue *q, HttpPacket *packet)
{
    HttpConn      *conn;
    HttpReceiver   *rec;
    MprBuf      *content;
    Upload      *up;
    char        *line, *nextTok;
    int         count, done, rc;
    
    mprAssert(packet);
    
    conn = q->conn;
    rec = conn->receiver;
    up = q->queueData;
    
    if (httpGetPacketLength(packet) == 0) {
        if (up->contentState != HTTP_UPLOAD_CONTENT_END) {
            httpError(conn, HTTP_CODE_BAD_REQUEST, "Client supplied insufficient upload data");
            return;
        }
        httpSendPacketToNext(q, packet);
        return;
    }
    mprLog(conn, 5, "uploadIncomingData: %d bytes", httpGetPacketLength(packet));
    
    /*  
        Put the packet data onto the service queue for buffering. This aggregates input data incase we don't have
        a complete mime record yet.
     */
    httpJoinPacketForService(q, packet, 0);
    
    packet = q->first;
    content = packet->content;
    mprAddNullToBuf(content);
    count = mprGetBufLength(content);

    for (done = 0, line = 0; !done; ) {
        if  (up->contentState == HTTP_UPLOAD_BOUNDARY || up->contentState == HTTP_UPLOAD_CONTENT_HEADER) {
            /*
                Parse the next input line
             */
            line = mprGetBufStart(content);
            mprStrTok(line, "\n", &nextTok);
            if (nextTok == 0) {
                /* Incomplete line */
                done++;
                break; 
            }
            mprAdjustBufStart(content, (int) (nextTok - line));
            line = mprStrTrim(line, "\r");
        }

        switch (up->contentState) {
        case HTTP_UPLOAD_BOUNDARY:
            if (processContentBoundary(q, line) < 0) {
                done++;
            }
            break;

        case HTTP_UPLOAD_CONTENT_HEADER:
            if (processContentHeader(q, line) < 0) {
                done++;
            }
            break;

        case HTTP_UPLOAD_CONTENT_DATA:
            rc = processContentData(q);
            if (rc < 0) {
                done++;
            }
            if (mprGetBufLength(content) < up->boundaryLen) {
                /*  Incomplete boundary - return to get more data */
                done++;
            }
            break;

        case HTTP_UPLOAD_CONTENT_END:
            done++;
            break;
        }
    }
    /*  
        Compact the buffer to prevent memory growth. There is often residual data after the boundary for the next block.
     */
    if (packet != rec->headerPacket) {
        mprCompactBuf(content);
    }
    q->count -= (count - mprGetBufLength(content));
    mprAssert(q->count >= 0);

    if (mprGetBufLength(content) == 0) {
        /* 
           Quicker to free the buffer so the packets don't have to be joined the next time 
         */
        packet = httpGetPacket(q);
        httpFreePacket(q, packet);
        mprAssert(q->count >= 0);
    }
}


/*  
    Process the mime boundary division
    Returns  < 0 on a request or state error
            == 0 if successful
 */
static int processContentBoundary(HttpQueue *q, char *line)
{
    HttpConn      *conn;
    Upload      *up;

    conn = q->conn;
    up = q->queueData;

    /*
        Expecting a multipart boundary string
     */
    if (strncmp(up->boundary, line, up->boundaryLen) != 0) {
        httpError(conn, HTTP_CODE_BAD_REQUEST, "Bad upload state. Incomplete boundary");
        return MPR_ERR_BAD_STATE;
    }
    if (line[up->boundaryLen] && strcmp(&line[up->boundaryLen], "--") == 0) {
        up->contentState = HTTP_UPLOAD_CONTENT_END;
    } else {
        up->contentState = HTTP_UPLOAD_CONTENT_HEADER;
    }
    return 0;
}


/*  
    Expecting content headers. A blank line indicates the start of the data.
    Returns  < 0  Request or state error
    Returns == 0  Successfully parsed the input line.
 */
static int processContentHeader(HttpQueue *q, char *line)
{
    HttpConn          *conn;
    HttpReceiver       *rec;
    HttpUploadFile    *file;
    Upload          *up;
    char            *key, *headerTok, *rest, *nextPair, *value;

    conn = q->conn;
    rec = conn->receiver;
    up = q->queueData;
    
    if (line[0] == '\0') {
        up->contentState = HTTP_UPLOAD_CONTENT_DATA;
        return 0;
    }
    mprLog(conn, 5, "Header line: %s", line);

    headerTok = line;
    mprStrTok(line, ": ", &rest);

    if (mprStrcmpAnyCase(headerTok, "Content-Disposition") == 0) {

        /*  
            The content disposition header describes either a form
            variable or an uploaded file.
        
            Content-Disposition: form-data; name="field1"
            >>blank line
            Field Data
            ---boundary
     
            Content-Disposition: form-data; name="field1" ->
                filename="user.file"
            >>blank line
            File data
            ---boundary
         */
        key = rest;
        up->id = up->clientFilename = 0;
        while (key && mprStrTok(key, ";\r\n", &nextPair)) {

            key = mprStrTrim(key, " ");
            mprStrTok(key, "= ", &value);
            value = mprStrTrim(value, "\"");

            if (mprStrcmpAnyCase(key, "form-data") == 0) {
                /* Nothing to do */

            } else if (mprStrcmpAnyCase(key, "name") == 0) {
                mprFree(up->id);
                up->id = mprStrdup(up, value);

            } else if (mprStrcmpAnyCase(key, "filename") == 0) {
                if (up->id == 0) {
                    httpError(conn, HTTP_CODE_BAD_REQUEST, "Bad upload state. Missing name field");
                    return MPR_ERR_BAD_STATE;
                }
                mprFree(up->clientFilename);
                up->clientFilename = mprStrdup(up, value);
                /*  
                    Create the file to hold the uploaded data
                 */
                up->tmpPath = mprGetTempPath(up, rec->uploadDir);
                if (up->tmpPath == 0) {
                    httpError(conn, HTTP_CODE_INTERNAL_SERVER_ERROR, 
                        "Can't create upload temp file %s. Check upload temp dir %s", up->tmpPath, rec->uploadDir);
                    return MPR_ERR_CANT_OPEN;
                }
                mprLog(conn, 5, "File upload of: %s stored as %s", up->clientFilename, up->tmpPath);

                up->file = mprOpen(up, up->tmpPath, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0600);
                if (up->file == 0) {
                    httpError(conn, HTTP_CODE_INTERNAL_SERVER_ERROR, "Can't open upload temp file %s", up->tmpPath);
                    return MPR_ERR_BAD_STATE;
                }
                /*  
                    Create the files[id]
                 */
                file = up->currentFile = mprAllocObjZeroed(up, HttpUploadFile);
                file->clientFilename = mprStrdup(file, up->clientFilename);
                file->filename = mprStrdup(file, up->tmpPath);
            }
            key = nextPair;
        }

    } else if (mprStrcmpAnyCase(headerTok, "Content-Type") == 0) {
        if (up->clientFilename) {
            mprLog(conn, 5, "Set files[%s][CONTENT_TYPE] = %s", up->id, rest);
            up->currentFile->contentType = mprStrdup(up->currentFile, rest);
        }
    }
    return 1;
}


static void defineFileFields(HttpQueue *q, Upload *up)
{
    HttpConn          *conn;
    HttpUploadFile    *file;
    char            *key;

    conn = q->conn;
    if (conn->transmitter->handler == conn->http->ejsHandler) {
        /*  
            Ejscript manages this for itself
         */
        return;
    }
    up = q->queueData;
    file = up->currentFile;
    key = mprStrcat(q, -1, "FILE_CLIENT_FILENAME_", up->id, NULL);
    httpSetFormVar(conn, key, file->clientFilename);
    mprFree(key);

    key = mprStrcat(q, -1, "FILE_CONTENT_TYPE_", up->id, NULL);
    httpSetFormVar(conn, key, file->contentType);
    mprFree(key);

    key = mprStrcat(q, -1, "FILE_FILENAME_", up->id, NULL);
    httpSetFormVar(conn, key, file->filename);
    mprFree(key);

    key = mprStrcat(q, -1, "FILE_SIZE_", up->id, NULL);
    httpSetIntFormVar(conn, key, file->size);
    mprFree(key);
}


static int writeToFile(HttpQueue *q, char *data, int len)
{
    HttpConn        *conn;
    HttpUploadFile  *file;
    HttpLimits      *limits;
    Upload          *up;
    int             rc;

    conn = q->conn;
    limits = conn->limits;
    up = q->queueData;
    file = up->currentFile;

    if ((file->size + len) > limits->maxUploadSize) {
        httpError(conn, HTTP_CODE_REQUEST_TOO_LARGE, 
            "Uploaded file %s exceeds maximum %d", up->tmpPath, limits->maxUploadSize);
        return MPR_ERR_CANT_WRITE;
    }
    if (len > 0) {
        /*  
            File upload. Write the file data.
         */
        rc = mprWrite(up->file, data, len);
        if (rc != len) {
            httpError(conn, HTTP_CODE_INTERNAL_SERVER_ERROR, 
                "Can't write to upload temp file %s, rc %d, errno %d", up->tmpPath, rc, mprGetOsError(up));
            return MPR_ERR_CANT_WRITE;
        }
        file->size += len;
        mprLog(q, 6, "uploadFilter: Wrote %d bytes to %s", len, up->tmpPath);
    }
    return 0;
}


/*  
    Process the content data.
    Returns < 0 on error
            == 0 when more data is needed
            == 1 when data successfully written
 */
static int processContentData(HttpQueue *q)
{
    HttpConn        *conn;
    HttpUploadFile  *file;
    HttpLimits      *limits;
    HttpPacket      *packet;
    MprBuf          *content;
    Upload          *up;
    char            *data, *bp, *key;
    int             size, dataLen;

    conn = q->conn;
    up = q->queueData;
    content = q->first->content;
    limits = conn->limits;
    file = up->currentFile;
    packet = 0;

    size = mprGetBufLength(content);
    if (size < up->boundaryLen) {
        /*  Incomplete boundary. Return and get more data */
        return 0;
    }
    bp = getBoundary(mprGetBufStart(content), size, up->boundary, up->boundaryLen);
    if (bp == 0) {
        mprLog(q, 6, "uploadFilter: Got boundary filename %x", up->clientFilename);
        if (up->clientFilename) {
            /*  
                No signature found yet. probably more data to come. Must handle split boundaries.
             */
            data = mprGetBufStart(content);
            dataLen = ((int) (mprGetBufEnd(content) - data)) - (up->boundaryLen - 1);
            if (dataLen > 0 && writeToFile(q, mprGetBufStart(content), dataLen) < 0) {
                return MPR_ERR_CANT_WRITE;
            }
            mprAdjustBufStart(content, dataLen);
            return 0;       /* Get more data */
        }
    }
    data = mprGetBufStart(content);
    if (bp) {
        dataLen = (int) (bp - data);
    } else {
        dataLen = mprGetBufLength(content);
    }
    if (dataLen > 0) {
        mprAdjustBufStart(content, dataLen);
        /*  
            This is the CRLF before the boundary
         */
        if (dataLen >= 2 && data[dataLen - 2] == '\r' && data[dataLen - 1] == '\n') {
            dataLen -= 2;
        }
        if (up->clientFilename) {
            /*  
                Write the last bit of file data and add to the list of files and define environment variables
             */
            if (writeToFile(q, data, dataLen) < 0) {
                return MPR_ERR_CANT_WRITE;
            }
            httpAddUploadFile(conn, up->id, file);
            defineFileFields(q, up);

        } else {
            /*  
                Normal string form data variables
             */
            data[dataLen] = '\0'; 
            mprLog(conn, 5, "uploadFilter: form[%s] = %s", up->id, data);
            key = mprUriDecode(q, up->id);
            data = mprUriDecode(q, data);
            httpSetFormVar(conn, key, data);
            if (packet == 0) {
                packet = httpCreatePacket(q, HTTP_BUFSIZE);
            }
            if (mprGetBufLength(packet->content) > 0) {
                /*
                    Need to add www-form-urlencoding separators
                 */
                mprPutCharToBuf(packet->content, '&');
            }
            mprPutFmtToBuf(packet->content, "%s=%s", up->id, data);
        }
    }
    if (up->clientFilename) {
        /*  
            Now have all the data (we've seen the boundary)
         */
        mprFree(up->file);
        up->file = 0;
        mprFree(up->clientFilename);
        up->clientFilename = 0;
    }
    if (packet) {
        httpSendPacketToNext(q, packet);
    }
    up->contentState = HTTP_UPLOAD_BOUNDARY;
    return 1;
}


/*  
    Find the boundary signature in memory. Returns pointer to the first match.
 */ 
static char *getBoundary(void *buf, int bufLen, void *boundary, int boundaryLen)
{
    char    *cp, *endp;
    char    first;

    mprAssert(buf);
    mprAssert(boundary);
    mprAssert(boundaryLen > 0);

    first = *((char*) boundary);
    cp = (char*) buf;

    if (bufLen < boundaryLen) {
        return 0;
    }
    endp = cp + (bufLen - boundaryLen) + 1;
    while (cp < endp) {
        cp = (char *) memchr(cp, first, endp - cp);
        if (!cp) {
            return 0;
        }
        if (memcmp(cp, boundary, boundaryLen) == 0) {
            return cp;
        }
        cp++;
    }
    return 0;
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
