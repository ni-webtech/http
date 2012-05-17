/*
    authFile.c - File based authorization using password files.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************** Forwards **********************************/

static bool isUserValid(HttpAuth *auth, cchar *realm, cchar *user);
static void manageGroup(HttpGroup *group, int flags);
static void manageUser(HttpUser *user, int flags);

/*********************************** Code *************************************/

cchar *httpGetFilePassword(HttpAuth *auth, cchar *realm, cchar *user)
{
    HttpUser    *up;
    char        *key;

    up = 0;
    key = sjoin(realm, ":", user, NULL);
    if (auth->users) {
        up = (HttpUser*) mprLookupKey(auth->users, key);
    }
    if (up == 0) {
        return 0;
    }
    return up->password;
}


bool httpValidateFileCredentials(HttpAuth *auth, cchar *realm, cchar *user, cchar *password, cchar *requiredPassword, 
    char **msg)
{
    char    passbuf[HTTP_MAX_PASS * 2], *hashedPassword;

    hashedPassword = 0;
    
    if (auth->type == HTTP_AUTH_BASIC) {
        mprSprintf(passbuf, sizeof(passbuf), "%s:%s:%s", user, realm, password);
        hashedPassword = mprGetMD5(passbuf);
        password = hashedPassword;
    }
    if (!isUserValid(auth, realm, user)) {
        *msg = "Access Denied, Unknown User.";
        return 0;
    }
    if (strcmp(password, requiredPassword)) {
        *msg = "Access Denied, Wrong Password.";
        return 0;
    }
    return 1;
}


/*
    Determine if this user is specified as being eligible for this realm. We examine the requiredUsers and requiredGroups.
 */
static bool isUserValid(HttpAuth *auth, cchar *realm, cchar *user)
{
    HttpGroup       *gp;
    HttpUser        *up;
    char            *key, *requiredUser, *requiredUsers, *requiredGroups, *group, *name, *tok, *gtok;
    int             next;

    if (auth->anyValidUser) {
        key = sjoin(realm, ":", user, NULL);
        if (auth->users == 0) {
            return 0;
        }
        return mprLookupKey(auth->users, key) != 0;
    }

    if (auth->requiredUsers) {
        requiredUsers = sclone(auth->requiredUsers);
        tok = NULL;
        requiredUser = stok(requiredUsers, " \t", &tok);
        while (requiredUser) {
            if (strcmp(user, requiredUser) == 0) {
                return 1;
            }
            requiredUser = stok(NULL, " \t", &tok);
        }
    }

    if (auth->requiredGroups) {
        gtok = NULL;
        requiredGroups = sclone(auth->requiredGroups);
        /*
            For each group, check all the users in the group.
         */
        group = stok(requiredGroups, " \t", &gtok);
        while (group) {
            if (auth->groups == 0) {
                gp = 0;
            } else {
                gp = (HttpGroup*) mprLookupKey(auth->groups, group);
            }
            if (gp == 0) {
                mprError("Can't find group %s", group);
                group = stok(NULL, " \t", &gtok);
                continue;
            }
            for (next = 0; (name = mprGetNextItem(gp->users, &next)) != 0; ) {
                if (strcmp(user, name) == 0) {
                    return 1;
                }
            }
            group = stok(NULL, " \t", &gtok);
        }
    }
    if (auth->requiredAcl != 0) {
        key = sjoin(realm, ":", user, NULL);
        up = (HttpUser*) mprLookupKey(auth->users, key);
        if (up) {
            mprLog(6, "UserRealm \"%s\" has ACL %lx, Required ACL %lx", key, up->acl, auth->requiredAcl);
            if (up->acl & auth->requiredAcl) {
                return 1;
            }
        }
    }
    return 0;
}


HttpGroup *httpCreateGroup(HttpAuth *auth, cchar *name, HttpAcl acl, bool enabled)
{
    HttpGroup     *gp;

    if ((gp = mprAllocObj(HttpGroup, manageGroup)) == 0) {
        return 0;
    }
    gp->acl = acl;
    gp->name = sclone(name);
    gp->enabled = enabled;
    gp->users = mprCreateList(0, 0);
    return gp;
}


static void manageGroup(HttpGroup *group, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(group->name);
        mprMark(group->users);
    }
}


int httpAddGroup(HttpAuth *auth, cchar *group, HttpAcl acl, bool enabled)
{
    HttpGroup     *gp;

    mprAssert(auth);
    mprAssert(group && *group);

    gp = httpCreateGroup(auth, group, acl, enabled);
    if (gp == 0) {
        return MPR_ERR_MEMORY;
    }
    /*
        Create the index on demand
     */
    if (auth->groups == 0) {
        auth->groups = mprCreateHash(-1, 0);
    }
    if (mprLookupKey(auth->groups, group)) {
        return MPR_ERR_ALREADY_EXISTS;
    }
    if (mprAddKey(auth->groups, group, gp) == 0) {
        return MPR_ERR_MEMORY;
    }
    return 0;
}


HttpUser *httpCreateUser(HttpAuth *auth, cchar *realm, cchar *user, cchar *password, bool enabled)
{
    HttpUser      *up;

    if ((up = mprAllocObj(HttpUser, manageUser)) == 0) {
        return 0;
    }
    up->name = sclone(user);
    up->realm = sclone(realm);
    up->password = sclone(password);
    up->enabled = enabled;
    return up;
}


static void manageUser(HttpUser *user, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(user->password);
        mprMark(user->realm);
        mprMark(user->name);
    }
}


int httpAddUser(HttpAuth *auth, cchar *realm, cchar *user, cchar *password, bool enabled)
{
    HttpUser  *up;

    char    *key;

    up = httpCreateUser(auth, realm, user, password, enabled);
    if (up == 0) {
        return MPR_ERR_MEMORY;
    }
    if (auth->users == 0) {
        auth->users = mprCreateHash(-1, 0);
    }
    key = sjoin(realm, ":", user, NULL);
    if (mprLookupKey(auth->users, key)) {
        return MPR_ERR_ALREADY_EXISTS;
    }
    if (mprAddKey(auth->users, key, up) == 0) {
        return MPR_ERR_MEMORY;
    }
    return 0;
}


int httpAddUserToGroup(HttpAuth *auth, HttpGroup *gp, cchar *user)
{
    char        *name;
    int         next;

    for (next = 0; (name = mprGetNextItem(gp->users, &next)) != 0; ) {
        if (strcmp(name, user) == 0) {
            return MPR_ERR_ALREADY_EXISTS;
        }
    }
    mprAddItem(gp->users, sclone(user));
    return 0;
}


int httpAddUsersToGroup(HttpAuth *auth, cchar *group, cchar *userList)
{
    HttpGroup   *gp;
    char        *user, *users, *tok;

    gp = 0;

    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupKey(auth->groups, group)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    tok = NULL;
    users = sclone(userList);
    user = stok(users, " ,\t", &tok);
    while (user) {
        /* Ignore already exists errors */
        httpAddUserToGroup(auth, gp, user);
        user = stok(NULL, " \t", &tok);
    }
    return 0;
}


int httpDisableGroup(HttpAuth *auth, cchar *group)
{
    HttpGroup     *gp;

    gp = 0;

    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupKey(auth->groups, group)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    gp->enabled = 0;
    return 0;
}


int httpDisableUser(HttpAuth *auth, cchar *realm, cchar *user)
{
    HttpUser      *up;
    char        *key;

    up = 0;
    key = sjoin(realm, ":", user, NULL);
    if (auth->users == 0 || (up = (HttpUser*) mprLookupKey(auth->users, key)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    up->enabled = 0;
    return 0;
}


int httpEnableGroup(HttpAuth *auth, cchar *group)
{
    HttpGroup     *gp;

    gp = 0;
    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupKey(auth->groups, group)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    gp->enabled = 1;
    return 0;
}


int httpEnableUser(HttpAuth *auth, cchar *realm, cchar *user)
{
    HttpUser      *up;
    char        *key;

    up = 0;
    key = sjoin(realm, ":", user, NULL);    
    if (auth->users == 0 || (up = (HttpUser*) mprLookupKey(auth->users, key)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    up->enabled = 1;
    return 0;
}


HttpAcl httpGetGroupAcl(HttpAuth *auth, char *group)
{
    HttpGroup     *gp;

    gp = 0;
    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupKey(auth->groups, group)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    return gp->acl;
}


bool httpIsGroupEnabled(HttpAuth *auth, cchar *group)
{
    HttpGroup     *gp;

    gp = 0;
    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupKey(auth->groups, group)) == 0) {
        return 0;
    }
    return gp->enabled;
}


bool httpIsUserEnabled(HttpAuth *auth, cchar *realm, cchar *user)
{
    HttpUser  *up;
    char    *key;

    up = 0;
    key = sjoin(realm, ":", user, NULL);
    if (auth->users == 0 || (up = (HttpUser*) mprLookupKey(auth->users, key)) == 0) {
        return 0;
    }
    return up->enabled;
}


/*
    ACLs are simple hex numbers
 */
//  TODO - better to convert to user, role, capability
HttpAcl httpParseAcl(HttpAuth *auth, cchar *aclStr)
{
    HttpAcl acl = 0;
    int     c;

    if (aclStr) {
        if (aclStr[0] == '0' && aclStr[1] == 'x') {
            aclStr += 2;
        }
        for (; isxdigit((int) *aclStr); aclStr++) {
            c = tolower((uchar) *aclStr);
            if ('0' <= c && c <= '9') {
                acl = (acl * 16) + c - '0';
            } else {
                acl = (acl * 16) + c - 'a' + 10;
            }
        }
    }
    return acl;
}


/*
    Update the total ACL for each user. We do this by oring all the ACLs for each group the user is a member of. 
    For fast access, this union ACL is stored in the HttpUser object
 */
void httpUpdateUserAcls(HttpAuth *auth)
{
    MprKey      *groupHash, *userHash;
    HttpUser    *user;
    HttpGroup   *gp;
    
    /*
        Reset the ACL for each user
     */
    if (auth->users != 0) {
        for (userHash = 0; (userHash = mprGetNextKey(auth->users, userHash)) != 0; ) {
            ((HttpUser*) userHash->data)->acl = 0;
        }
    }

    /*
        Get the union of all ACLs defined for a user over all groups that the user is a member of.
     */
    if (auth->groups != 0 && auth->users != 0) {
        for (groupHash = 0; (groupHash = mprGetNextKey(auth->groups, groupHash)) != 0; ) {
            gp = ((HttpGroup*) groupHash->data);
            for (userHash = 0; (userHash = mprGetNextKey(auth->users, userHash)) != 0; ) {
                user = ((HttpUser*) userHash->data);
                if (strcmp(user->name, user->name) == 0) {
                    user->acl = user->acl | gp->acl;
                    break;
                }
            }
        }
    }
}


int httpRemoveGroup(HttpAuth *auth, cchar *group)
{
    if (auth->groups == 0 || !mprLookupKey(auth->groups, group)) {
        return MPR_ERR_CANT_ACCESS;
    }
    mprRemoveKey(auth->groups, group);
    return 0;
}


int httpRemoveUser(HttpAuth *auth, cchar *realm, cchar *user)
{
    char    *key;

    key = sjoin(realm, ":", user, NULL);
    if (auth->users == 0 || !mprLookupKey(auth->users, key)) {
        return MPR_ERR_CANT_ACCESS;
    }
    mprRemoveKey(auth->users, key);
    return 0;
}


//  MOB - inconsistent. This takes a "group" string. httpRemoveUserFromGroup takes a "gp"
int httpRemoveUsersFromGroup(HttpAuth *auth, cchar *group, cchar *userList)
{
    HttpGroup   *gp;
    char        *user, *users, *tok;

    gp = 0;
    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupKey(auth->groups, group)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    tok = NULL;
    users = sclone(userList);
    user = stok(users, " \t", &tok);
    while (user) {
        httpRemoveUserFromGroup(gp, user);
        user = stok(NULL, " \t", &tok);
    }
    return 0;
}


int httpSetGroupAcl(HttpAuth *auth, cchar *group, HttpAcl acl)
{
    HttpGroup     *gp;

    gp = 0;
    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupKey(auth->groups, group)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    gp->acl = acl;
    return 0;
}


void httpSetRequiredAcl(HttpAuth *auth, HttpAcl acl)
{
    auth->requiredAcl = acl;
}


int httpRemoveUserFromGroup(HttpGroup *gp, cchar *user)
{
    char    *name;
    int     next;

    for (next = 0; (name = mprGetNextItem(gp->users, &next)) != 0; ) {
        if (strcmp(name, user) == 0) {
            mprRemoveItem(gp->users, name);
            return 0;
        }
    }
    return MPR_ERR_CANT_ACCESS;
}


int httpReadGroupFile(HttpAuth *auth, char *path)
{
    MprFile     *file;
    HttpAcl     acl;
    char        *buf;
    char        *users, *group, *enabled, *aclSpec, *tok, *cp;

    auth->groupFile = sclone(path);

    if ((file = mprOpenFile(path, O_RDONLY | O_TEXT, 0444)) == 0) {
        return MPR_ERR_CANT_OPEN;
    }
    while ((buf = mprReadLine(file, MPR_BUFSIZE, NULL)) != NULL) {
        enabled = stok(buf, " :\t", &tok);
        for (cp = enabled; cp && isspace((uchar) *cp); cp++) { }
        if (cp == 0 || *cp == '\0' || *cp == '#') {
            continue;
        }
        aclSpec = stok(NULL, " :\t", &tok);
        group = stok(NULL, " :\t", &tok);
        users = stok(NULL, "\r\n", &tok);

        acl = httpParseAcl(auth, aclSpec);
        httpAddGroup(auth, group, acl, (*enabled == '0') ? 0 : 1);
        httpAddUsersToGroup(auth, group, users);
    }
    httpUpdateUserAcls(auth);
    mprCloseFile(file);
    return 0;
}


int httpReadUserFile(HttpAuth *auth, char *path)
{
    MprFile     *file;
    char        *buf;
    char        *enabled, *user, *password, *realm, *tok, *cp;

    auth->userFile = sclone(path);

    if ((file = mprOpenFile(path, O_RDONLY | O_TEXT, 0444)) == 0) {
        return MPR_ERR_CANT_OPEN;
    }
    while ((buf = mprReadLine(file, MPR_BUFSIZE, NULL)) != NULL) {
        enabled = stok(buf, " :\t", &tok);
        for (cp = enabled; cp && isspace((uchar) *cp); cp++) { }
        if (cp == 0 || *cp == '\0' || *cp == '#') {
            continue;
        }
        user = stok(NULL, ":", &tok);
        realm = stok(NULL, ":", &tok);
        password = stok(NULL, " \t\r\n", &tok);

        user = strim(user, " \t", MPR_TRIM_BOTH);
        realm = strim(realm, " \t", MPR_TRIM_BOTH);
        password = strim(password, " \t", MPR_TRIM_BOTH);

        httpAddUser(auth, realm, user, password, (*enabled == '0' ? 0 : 1));
    }
    httpUpdateUserAcls(auth);
    mprCloseFile(file);
    return 0;
}


int httpWriteUserFile(HttpAuth *auth, char *path)
{
    MprFile         *file;
    MprKey          *kp;
    HttpUser        *up;
    char            buf[HTTP_MAX_PASS * 2];
    char            *tempFile;

    tempFile = mprGetTempPath(NULL);
    if ((file = mprOpenFile(tempFile, O_CREAT | O_TRUNC | O_WRONLY | O_TEXT, 0444)) == 0) {
        mprError("Can't open %s", tempFile);
        return MPR_ERR_CANT_OPEN;
    }
    kp = mprGetNextKey(auth->users, 0);
    while (kp) {
        up = (HttpUser*) kp->data;
        mprSprintf(buf, sizeof(buf), "%d: %s: %s: %s\n", up->enabled, up->name, up->realm, up->password);
        mprWriteFile(file, buf, (int) strlen(buf));
        kp = mprGetNextKey(auth->users, kp);
    }
    mprCloseFile(file);
    unlink(path);
    if (rename(tempFile, path) < 0) {
        mprError("Can't create new %s", path);
        return MPR_ERR_CANT_WRITE;
    }
    return 0;
}


int httpWriteGroupFile(HttpAuth *auth, char *path)
{
    MprKey          *kp;
    MprFile         *file;
    HttpGroup       *gp;
    char            buf[MPR_MAX_STRING], *tempFile, *name;
    int             next;

    tempFile = mprGetTempPath(NULL);
    if ((file = mprOpenFile(tempFile, O_CREAT | O_TRUNC | O_WRONLY | O_TEXT, 0444)) == 0) {
        mprError("Can't open %s", tempFile);
        return MPR_ERR_CANT_OPEN;
    }
    kp = mprGetNextKey(auth->groups, 0);
    while (kp) {
        gp = (HttpGroup*) kp->data;
        mprSprintf(buf, sizeof(buf), "%d: %x: %s: ", gp->enabled, gp->acl, gp->name);
        mprWriteFile(file, buf, strlen(buf));
        for (next = 0; (name = mprGetNextItem(gp->users, &next)) != 0; ) {
            mprWriteFile(file, name, strlen(name));
        }
        mprWriteFile(file, "\n", 1);
        kp = mprGetNextKey(auth->groups, kp);
    }
    mprCloseFile(file);
    unlink(path);
    if (rename(tempFile, path) < 0) {
        mprError("Can't create new %s", path);
        return MPR_ERR_CANT_WRITE;
    }
    return 0;
}

/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2012. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You httpy use the GPL open source license described below or you may acquire 
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
