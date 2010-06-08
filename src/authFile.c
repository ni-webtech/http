/*
    authFile.c - File based authorization using httpPassword files.

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

#if BLD_FEATURE_AUTH_FILE
/********************************** Forwards **********************************/

static bool isUserValid(HttpAuth *auth, cchar *realm, cchar *user);

/*********************************** Code *************************************/

cchar *httpGetNativePassword(HttpAuth *auth, cchar *realm, cchar *user)
{
    HttpUser      *up;
    char        *key;

    up = 0;
    key = mprStrcat(auth, -1, realm, ":", user, NULL);
    if (auth->users) {
        up = (HttpUser*) mprLookupHash(auth->users, key);
    }
    mprFree(key);
    if (up == 0) {
        return 0;
    }
    return up->password;
}


bool httpValidateNativeCredentials(HttpAuth *auth, cchar *realm, cchar *user, cchar *password, cchar *requiredPassword, 
    char **msg)
{
    char    passbuf[HTTP_MAX_PASS * 2], *hashedPassword;
    int     len;

    hashedPassword = 0;
    
    if (auth->type == HTTP_AUTH_BASIC) {
        mprSprintf(auth, passbuf, sizeof(passbuf), "%s:%s:%s", user, realm, password);
        len = strlen(passbuf);
        hashedPassword = mprGetMD5Hash(auth, passbuf, len, NULL);
        password = hashedPassword;
    }
    if (!isUserValid(auth, realm, user)) {
        *msg = "Access Denied, Unknown User.";
        mprFree(hashedPassword);
        return 0;
    }
    if (strcmp(password, requiredPassword)) {
        *msg = "Access Denied, Wrong Password.";
        mprFree(hashedPassword);
        return 0;
    }
    mprFree(hashedPassword);
    return 1;
}


/*
    Determine if this user is specified as being eligible for this realm. We examine the requiredUsers and requiredGroups.
 */
static bool isUserValid(HttpAuth *auth, cchar *realm, cchar *user)
{
    HttpGroup         *gp;
    HttpUser          *up;
    cchar           *tok, *gtok;
    char            ubuf[80], gbuf[80], *key, *requiredUser, *group, *name;
    int             rc, next;

    if (auth->anyValidUser) {
        key = mprStrcat(auth, -1, realm, ":", user, NULL);
        if (auth->users == 0) {
            return 0;
        }
        rc = mprLookupHash(auth->users, key) != 0;
        mprFree(key);
        return rc;
    }

    if (auth->requiredUsers) {
        tok = NULL;
        requiredUser = mprGetWordTok(ubuf, sizeof(ubuf), auth->requiredUsers, " \t", &tok);
        while (requiredUser) {
            if (strcmp(user, requiredUser) == 0) {
                return 1;
            }
            requiredUser = mprGetWordTok(ubuf, sizeof(ubuf), 0, " \t", &tok);
        }
    }

    if (auth->requiredGroups) {
        gtok = NULL;
        group = mprGetWordTok(gbuf, sizeof(gbuf), auth->requiredGroups, " \t", &gtok);
        /*
            For each group, check all the users in the group.
         */
        while (group) {
            if (auth->groups == 0) {
                gp = 0;
            } else {
                gp = (HttpGroup*) mprLookupHash(auth->groups, group);
            }
            if (gp == 0) {
                mprError(auth, "Can't find group %s", group);
                group = mprGetWordTok(gbuf, sizeof(gbuf), 0, " \t", &gtok);
                continue;
            }
            for (next = 0; (name = mprGetNextItem(gp->users, &next)) != 0; ) {
                if (strcmp(user, name) == 0) {
                    return 1;
                }
            }
            group = mprGetWordTok(gbuf, sizeof(gbuf), 0, " \t", &gtok);
        }
    }
    if (auth->requiredAcl != 0) {
        key = mprStrcat(auth, -1, realm, ":", user, NULL);
        up = (HttpUser*) mprLookupHash(auth->users, key);
        if (up) {
            mprLog(auth, 6, "UserRealm \"%s\" has ACL %lx, Required ACL %lx", key, up->acl, auth->requiredAcl);
            if (up->acl & auth->requiredAcl) {
                mprFree(key);
                return 1;
            }
        }
        mprFree(key);
    }
    return 0;
}


HttpGroup *httpCreateGroup(HttpAuth *auth, cchar *name, HttpAcl acl, bool enabled)
{
    HttpGroup     *gp;

    gp = mprAllocObjZeroed(auth, HttpGroup);
    if (gp == 0) {
        return 0;
    }

    gp->acl = acl;
    gp->name = mprStrdup(gp, name);
    gp->enabled = enabled;
    gp->users = mprCreateList(gp);
    return gp;
}


int httpAddGroup(HttpAuth *auth, cchar *group, HttpAcl acl, bool enabled)
{
    HttpGroup     *gp;

    mprAssert(auth);
    mprAssert(group && *group);

    gp = httpCreateGroup(auth, group, acl, enabled);
    if (gp == 0) {
        return MPR_ERR_NO_MEMORY;
    }
    /*
        Create the index on demand
     */
    if (auth->groups == 0) {
        auth->groups = mprCreateHash(auth, -1);
    }
    if (mprLookupHash(auth->groups, group)) {
        return MPR_ERR_ALREADY_EXISTS;
    }
    if (mprAddHash(auth->groups, group, gp) == 0) {
        return MPR_ERR_NO_MEMORY;
    }
    return 0;
}


HttpUser *httpCreateUser(HttpAuth *auth, cchar *realm, cchar *user, cchar *password, bool enabled)
{
    HttpUser      *up;

    up = mprAllocObjZeroed(auth, HttpUser);
    if (up == 0) {
        return 0;
    }
    up->name = mprStrdup(up, user);
    up->realm = mprStrdup(up, realm);
    up->password = mprStrdup(up, password);
    up->enabled = enabled;
    return up;
}


int httpAddUser(HttpAuth *auth, cchar *realm, cchar *user, cchar *password, bool enabled)
{
    HttpUser  *up;

    char    *key;

    up = httpCreateUser(auth, realm, user, password, enabled);
    if (up == 0) {
        return MPR_ERR_NO_MEMORY;
    }
    if (auth->users == 0) {
        auth->users = mprCreateHash(auth, -1);
    }
    key = mprStrcat(auth, -1, realm, ":", user, NULL);
    if (mprLookupHash(auth->users, key)) {
        mprFree(key);
        return MPR_ERR_ALREADY_EXISTS;
    }

    if (mprAddHash(auth->users, key, up) == 0) {
        mprFree(key);
        return MPR_ERR_NO_MEMORY;
    }
    mprFree(key);
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
    mprAddItem(gp->users, mprStrdup(gp, user));
    return 0;
}


int httpAddUsersToGroup(HttpAuth *auth, cchar *group, cchar *users)
{
    HttpGroup     *gp;
    cchar       *tok;
    char        ubuf[80], *user;

    gp = 0;

    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupHash(auth->groups, group)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    tok = NULL;
    user = mprGetWordTok(ubuf, sizeof(ubuf), users, " \t", &tok);
    while (user) {
        /* Ignore already exists errors */
        httpAddUserToGroup(auth, gp, user);
        user = mprGetWordTok(ubuf, sizeof(ubuf), 0, " \t", &tok);
    }
    return 0;
}


int httpDisableGroup(HttpAuth *auth, cchar *group)
{
    HttpGroup     *gp;

    gp = 0;

    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupHash(auth->groups, group)) == 0) {
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
    key = mprStrcat(auth, -1, realm, ":", user, NULL);
    if (auth->users == 0 || (up = (HttpUser*) mprLookupHash(auth->users, key)) == 0) {
        mprFree(key);
        return MPR_ERR_CANT_ACCESS;
    }
    mprFree(key);
    up->enabled = 0;
    return 0;
}


int httpEnableGroup(HttpAuth *auth, cchar *group)
{
    HttpGroup     *gp;

    gp = 0;
    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupHash(auth->groups, group)) == 0) {
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
    key = mprStrcat(auth, -1, realm, ":", user, NULL);    
    if (auth->users == 0 || (up = (HttpUser*) mprLookupHash(auth->users, key)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    up->enabled = 1;
    return 0;
}


HttpAcl httpGetGroupAcl(HttpAuth *auth, char *group)
{
    HttpGroup     *gp;

    gp = 0;
    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupHash(auth->groups, group)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }
    return gp->acl;
}


bool httpIsGroupEnabled(HttpAuth *auth, cchar *group)
{
    HttpGroup     *gp;

    gp = 0;
    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupHash(auth->groups, group)) == 0) {
        return 0;
    }
    return gp->enabled;
}


bool httpIsUserEnabled(HttpAuth *auth, cchar *realm, cchar *user)
{
    HttpUser  *up;
    char    *key;

    up = 0;
    key = mprStrcat(auth, -1, realm, ":", user, NULL);
    if (auth->users == 0 || (up = (HttpUser*) mprLookupHash(auth->users, key)) == 0) {
        mprFree(key);
        return 0;
    }
    mprFree(key);
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
            c = (int) tolower((int) *aclStr);
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
    MprHash     *groupHash, *userHash;
    HttpUser      *user;
    HttpGroup     *gp;
    
    /*
        Reset the ACL for each user
     */
    if (auth->users != 0) {
        for (userHash = 0; (userHash = mprGetNextHash(auth->users, userHash)) != 0; ) {
            ((HttpUser*) userHash->data)->acl = 0;
        }
    }

    /*
        Get the union of all ACLs defined for a user over all groups that the user is a member of.
     */
    if (auth->groups != 0 && auth->users != 0) {
        for (groupHash = 0; (groupHash = mprGetNextHash(auth->groups, groupHash)) != 0; ) {
            gp = ((HttpGroup*) groupHash->data);
            for (userHash = 0; (userHash = mprGetNextHash(auth->users, userHash)) != 0; ) {
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
    if (auth->groups == 0 || !mprLookupHash(auth->groups, group)) {
        return MPR_ERR_CANT_ACCESS;
    }
    mprRemoveHash(auth->groups, group);
    return 0;
}


int httpRemoveUser(HttpAuth *auth, cchar *realm, cchar *user)
{
    char    *key;

    key = mprStrcat(auth, -1, realm, ":", user, NULL);
    if (auth->users == 0 || !mprLookupHash(auth->users, key)) {
        mprFree(key);
        return MPR_ERR_CANT_ACCESS;
    }
    mprRemoveHash(auth->users, key);
    mprFree(key);
    return 0;
}


int httpRemoveUsersFromGroup(HttpAuth *auth, cchar *group, cchar *users)
{
    HttpGroup     *gp;
    cchar       *tok;
    char        ubuf[80], *user;

    gp = 0;
    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupHash(auth->groups, group)) == 0) {
        return MPR_ERR_CANT_ACCESS;
    }

    tok = NULL;
    user = mprGetWordTok(ubuf, sizeof(ubuf), users, " \t", &tok);
    while (user) {
        httpRemoveUserFromGroup(gp, user);
        user = mprGetWordTok(ubuf, sizeof(ubuf), 0, " \t", &tok);
    }
    return 0;
}


int httpSetGroupAcl(HttpAuth *auth, cchar *group, HttpAcl acl)
{
    HttpGroup     *gp;

    gp = 0;
    if (auth->groups == 0 || (gp = (HttpGroup*) mprLookupHash(auth->groups, group)) == 0) {
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


int httpReadGroupFile(Http *http, HttpAuth *auth, char *path)
{
    MprFile     *file;
    HttpAcl     acl;
    char        buf[MPR_MAX_STRING];
    char        *users, *group, *enabled, *aclSpec, *tok, *cp;

    mprFree(auth->groupFile);
    auth->groupFile = mprStrdup(http, path);

    if ((file = mprOpen(auth, path, O_RDONLY | O_TEXT, 0444)) == 0) {
        return MPR_ERR_CANT_OPEN;
    }

    while (mprGets(file, buf, sizeof(buf))) {
        enabled = mprStrTok(buf, " :\t", &tok);

        for (cp = enabled; isspace((int) *cp); cp++) {
            ;
        }
        if (*cp == '\0' || *cp == '#') {
            continue;
        }

        aclSpec = mprStrTok(0, " :\t", &tok);
        group = mprStrTok(0, " :\t", &tok);
        users = mprStrTok(0, "\r\n", &tok);

        acl = httpParseAcl(auth, aclSpec);
        httpAddGroup(auth, group, acl, (*enabled == '0') ? 0 : 1);
        httpAddUsersToGroup(auth, group, users);
    }
    mprFree(file);
    httpUpdateUserAcls(auth);
    return 0;
}


int httpReadUserFile(Http *http, HttpAuth *auth, char *path)
{
    MprFile     *file;
    char        buf[MPR_MAX_STRING];
    char        *enabled, *user, *password, *realm, *tok, *cp;

    mprFree(auth->userFile);
    auth->userFile = mprStrdup(auth, path);

    if ((file = mprOpen(auth, path, O_RDONLY | O_TEXT, 0444)) == 0) {
        return MPR_ERR_CANT_OPEN;
    }

    while (mprGets(file, buf, sizeof(buf))) {
        enabled = mprStrTok(buf, " :\t", &tok);

        for (cp = enabled; isspace((int) *cp); cp++) {
            ;
        }
        if (*cp == '\0' || *cp == '#') {
            continue;
        }
        user = mprStrTok(0, ":", &tok);
        realm = mprStrTok(0, ":", &tok);
        password = mprStrTok(0, " \t\r\n", &tok);

        user = mprStrTrim(user, " \t");
        realm = mprStrTrim(realm, " \t");
        password = mprStrTrim(password, " \t");

        httpAddUser(auth, realm, user, password, (*enabled == '0' ? 0 : 1));
    }
    mprFree(file);
    httpUpdateUserAcls(auth);
    return 0;
}


int httpWriteUserFile(Http *http, HttpAuth *auth, char *path)
{
    MprFile         *file;
    MprHash         *hp;
    HttpUser        *up;
    char            buf[HTTP_MAX_PASS * 2];
    char            *tempFile;

    tempFile = mprGetTempPath(auth, NULL);
    if ((file = mprOpen(auth, tempFile, O_CREAT | O_TRUNC | O_WRONLY | O_TEXT, 0444)) == 0) {
        mprError(http, "Can't open %s", tempFile);
        mprFree(tempFile);
        return MPR_ERR_CANT_OPEN;
    }
    mprFree(tempFile);

    hp = mprGetNextHash(auth->users, 0);
    while (hp) {
        up = (HttpUser*) hp->data;
        mprSprintf(http, buf, sizeof(buf), "%d: %s: %s: %s\n", up->enabled, up->name, up->realm, up->password);
        mprWrite(file, buf, (int) strlen(buf));
        hp = mprGetNextHash(auth->users, hp);
    }
    mprFree(file);
    unlink(path);
    if (rename(tempFile, path) < 0) {
        mprError(http, "Can't create new %s", path);
        return MPR_ERR_CANT_WRITE;
    }
    return 0;
}


int httpWriteGroupFile(Http *http, HttpAuth *auth, char *path)
{
    MprHash         *hp;
    MprFile         *file;
    HttpGroup       *gp;
    char            buf[MPR_MAX_STRING], *tempFile, *name;
    int             next;

    tempFile = mprGetTempPath(http, NULL);
    if ((file = mprOpen(auth, tempFile, O_CREAT | O_TRUNC | O_WRONLY | O_TEXT, 0444)) == 0) {
        mprError(http, "Can't open %s", tempFile);
        mprFree(tempFile);
        return MPR_ERR_CANT_OPEN;
    }
    mprFree(tempFile);

    hp = mprGetNextHash(auth->groups, 0);
    while (hp) {
        gp = (HttpGroup*) hp->data;
        mprSprintf(http, buf, sizeof(buf), "%d: %x: %s: ", gp->enabled, gp->acl, gp->name);
        mprWrite(file, buf, (int) strlen(buf));
        for (next = 0; (name = mprGetNextItem(gp->users, &next)) != 0; ) {
            mprWrite(file, name, (int) strlen(name));
        }
        mprWrite(file, "\n", 1);
        hp = mprGetNextHash(auth->groups, hp);
    }
    mprFree(file);

    unlink(path);
    if (rename(tempFile, path) < 0) {
        mprError(http, "Can't create new %s", path);
        return MPR_ERR_CANT_WRITE;
    }
    return 0;
}

#else
void __nativeAuthFile() {}
#endif /* BLD_FEATURE_AUTH_FILE */

/*
    @copy   default
    
    Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
    Copyright (c) Michael O'Brien, 1993-2010. All Rights Reserved.
    
    This software is distributed under commercial and open source licenses.
    You httpy use the GPL open source license described below or you may acquire 
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
