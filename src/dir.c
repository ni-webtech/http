/*
    dir.c -- Support authorization on a per-directory basis.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************* Forwards ************************************/

static void manageDir(HttpDir *dir, int flags);

/************************************ Code *************************************/

HttpDir *httpCreateDir(cchar *path)
{
    HttpDir   *dir;

    mprAssert(path);

    if ((dir = mprAllocObj(HttpDir, manageDir)) == 0) {
        return 0;
    }
    dir->indexName = sclone("index.html");
    dir->auth = httpCreateAuth(0);
    if (path) {
        dir->path = sclone(path);
    }
    return dir;
}


HttpDir *httpCreateInheritedDir(HttpDir *parent)
{
    HttpDir   *dir;

    mprAssert(parent);

    if ((dir = mprAllocObj(HttpDir, manageDir)) == 0) {
        return 0;
    }
    dir->indexName = sclone(parent->indexName);
    httpSetDirPath(dir, parent->path);
    dir->auth = httpCreateAuth(parent->auth);
    return dir;
}


static void manageDir(HttpDir *dir, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(dir->auth);
        mprMark(dir->indexName);
        mprMark(dir->path);
    }
}


void httpSetDirPath(HttpDir *dir, cchar *path)
{
    mprAssert(dir);
    mprAssert(path);

    dir->path = strim(mprGetAbsPath(path), "/", MPR_TRIM_END);
}


void httpSetDirIndex(HttpDir *dir, cchar *name) 
{ 
    mprAssert(dir);
    mprAssert(name && *name);

    dir->indexName = sclone(name); 
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
