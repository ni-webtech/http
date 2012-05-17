/*
    auth.c - Generic authorization code
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************* Forwards ***********************************/

static void manageAuth(HttpAuth *auth, int flags);

/*********************************** Code *************************************/

HttpAuth *httpCreateAuth()
{
    HttpAuth      *auth;

    if ((auth = mprAllocObj(HttpAuth, manageAuth)) == 0) {
        return 0;
    }
    auth->backend = HTTP_AUTH_FILE;
    return auth;
}


HttpAuth *httpCreateInheritedAuth(HttpAuth *parent)
{
    HttpAuth      *auth;

    if ((auth = mprAllocObj(HttpAuth, manageAuth)) == 0) {
        return 0;
    }
    if (parent) {
        if (parent->allow) {
            auth->allow = mprCloneHash(parent->allow);
        }
        if (parent->deny) {
            auth->deny = mprCloneHash(parent->deny);
        }
        auth->anyValidUser = parent->anyValidUser;
        auth->type = parent->type;
        auth->backend = parent->backend;
        auth->flags = parent->flags;
        auth->order = parent->order;
        auth->qop = parent->qop;
        auth->requiredRealm = parent->requiredRealm;
        auth->requiredUsers = parent->requiredUsers;
        auth->requiredGroups = parent->requiredGroups;

        auth->userFile = parent->userFile;
        auth->groupFile = parent->groupFile;
        auth->users = parent->users;
        auth->groups = parent->groups;

    } else{
        auth->backend = HTTP_AUTH_FILE;
    }
    return auth;
}


static void manageAuth(HttpAuth *auth, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(auth->allow);
        mprMark(auth->deny);
        mprMark(auth->requiredRealm);
        mprMark(auth->requiredGroups);
        mprMark(auth->requiredUsers);
        mprMark(auth->qop);
        mprMark(auth->userFile);
        mprMark(auth->groupFile);
        mprMark(auth->users);
        mprMark(auth->groups);
    }
}


void httpSetAuthAllow(HttpAuth *auth, cchar *allow)
{
    if (auth->allow == 0) {
        auth->allow = mprCreateHash(-1, MPR_HASH_STATIC_VALUES);
    }
    mprAddKey(auth->allow, sclone(allow), "");
}


void httpSetAuthAnyValidUser(HttpAuth *auth)
{
    auth->anyValidUser = 1;
    auth->flags |= HTTP_AUTH_REQUIRED;
}


void httpSetAuthDeny(HttpAuth *auth, cchar *client)
{
    if (auth->deny == 0) {
        auth->deny = mprCreateHash(-1, MPR_HASH_STATIC_VALUES);
    }
    mprAddKey(auth->deny, sclone(client), "");
}


void httpSetAuthOrder(HttpAuth *auth, int order)
{
    auth->order = order;
}


void httpSetAuthQop(HttpAuth *auth, cchar *qop)
{
    if (strcmp(qop, "auth") == 0 || strcmp(qop, "auth-int") == 0) {
        auth->qop = sclone(qop);
    } else {
        auth->qop = mprEmptyString();
    }
}


void httpSetAuthRealm(HttpAuth *auth, cchar *realm)
{
    auth->requiredRealm = sclone(realm);
}


void httpSetAuthRequiredGroups(HttpAuth *auth, cchar *groups)
{
    auth->requiredGroups = sclone(groups);
    auth->flags |= HTTP_AUTH_REQUIRED;
    auth->anyValidUser = 0;
}


void httpSetAuthRequiredUsers(HttpAuth *auth, cchar *users)
{
    auth->requiredUsers = sclone(users);
    auth->flags |= HTTP_AUTH_REQUIRED;
    auth->anyValidUser = 0;
}


void httpSetAuthUser(HttpConn *conn, cchar *user)
{
    conn->authUser = sclone(user);
}


/*  
    Validate the user credentials with the designated authorization backend method.
 */
static bool validateCred(HttpAuth *auth, cchar *realm, char *user, cchar *password, cchar *requiredPass, char **msg)
{
    if (auth->backend == HTTP_AUTH_FILE) {
        return httpValidateFileCredentials(auth, realm, user, password, requiredPass, msg);
    } else if (auth->backend == HTTP_AUTH_PAM) {
        return httpValidatePamCredentials(auth, realm, user, password, NULL, msg);
    } else {
        *msg = "Required authorization backend method is not enabled or configured";
    }
    return 0;
}


/*  
    Get the password (if the designated authorization backend method will give it to us)
 */
static cchar *getPassword(HttpAuth *auth, cchar *realm, cchar *user)
{
    if (auth->backend == HTTP_AUTH_FILE) {
        return httpGetFilePassword(auth, realm, user);
    } else if (auth->backend == HTTP_AUTH_PAM) {
        return httpGetPamPassword(auth, realm, user);
    }
    return 0;
}


void httpInitAuth(Http *http)
{
    http->validateCred = validateCred;
    http->getPassword = getPassword;
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
