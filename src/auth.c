/*
    auth.c - Authorization and access management

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/********************************* Forwards ***********************************/

static void computeAbilities(HttpAuth *auth, MprHash *abilities, cchar *ability);
static void manageAuth(HttpAuth *auth, int flags);
static void manageRole(HttpRole *role, int flags);
static void manageUser(HttpUser *user, int flags);
static void postLogin(HttpConn *conn);
static bool verifyUser(HttpConn *conn);

/*********************************** Code *************************************/

void httpInitAuth(Http *http)
{
    httpAddAuthType(http, "basic", httpBasicLogin, httpBasicParse, httpBasicSetHeaders);
    httpAddAuthType(http, "digest", httpDigestLogin, httpDigestParse, httpDigestSetHeaders);
    httpAddAuthType(http, "post", postLogin, NULL, NULL);

#if BIT_HAS_PAM && BIT_PAM
    /*
        Pam must be actively selected duringin configuration
     */
    httpAddAuthStore(http, "pam", httpPamVerifyUser);
#endif
    httpAddAuthStore(http, "internal", verifyUser);
}


int httpCheckAuth(HttpConn *conn)
{
    HttpRx      *rx;
    HttpAuth    *auth;
    HttpRoute   *route;
    HttpSession *session;
    cchar       *version;
    bool        cached;

    rx = conn->rx;
    route = rx->route;
    auth = route->auth;

    mprAssert(auth);

    if ((auth->flags & HTTP_SECURE) && !conn->secure) {
        httpError(conn, HTTP_CODE_BAD_REQUEST, "Access denied. Secure access required.");
        return 0;
    }
    if (!auth->type || (auth->flags & HTTP_AUTO_LOGIN)) {
        /* Authentication not required */
        return 1;
    }
    mprAssert(!conn->user);

    cached = 0;
    if (rx->cookie && (session = httpGetSession(conn, 0)) != 0) {
        /*
            Retrieve authentication state from the session storage. Faster than re-authenticating.
         */
        if ((conn->username = (char*) httpGetSessionVar(conn, HTTP_SESSION_USERNAME, 0)) != 0) {
            version = httpGetSessionVar(conn, HTTP_SESSION_AUTHVER, 0);
            if (stoi(version) == auth->version) {
                mprLog(5, "Using cached authentication data for user %s", conn->username);
                cached = 1;
            }
        }
    }
    if (!cached) {
        if (conn->authType && !smatch(conn->authType, auth->type->name)) {
            httpError(conn, HTTP_CODE_BAD_REQUEST, "Access denied. Wrong authentication protocol type.");
            return 0;
        }
        if (rx->authDetails && (auth->type->parseAuth)(conn) < 0) {
            mprAssert(conn->error);
            return 0;
        }
        if (!conn->username) {
            (auth->type->askLogin)(conn);
            return 0;
        }
        if (!(auth->store->verifyUser)(conn)) {
            (auth->type->askLogin)(conn);
            return 0;
        }
        /*
            Store authentication state and user in session storage
         */
        if ((session = httpCreateSession(conn)) != 0) {
            httpSetSessionVar(conn, HTTP_SESSION_AUTHVER, itos(auth->version));
            httpSetSessionVar(conn, HTTP_SESSION_USERNAME, conn->username);
        }
    }
    if (auth->permittedUsers && !mprLookupKey(auth->permittedUsers, conn->username)) {
        mprLog(2, "User \"%s\" is not specified as a permitted user to access %s", conn->username, conn->rx->pathInfo);
        httpError(conn, HTTP_CODE_FORBIDDEN, "Access denied. User is not authorized for access.");
        return 0;
    }
    conn->authenticated = httpCanUser(conn, auth->requiredAbilities);
    return conn->authenticated;
}


bool httpCanUser(HttpConn *conn, MprHash *requiredAbilities)
{
    HttpAuth    *auth;
    MprKey      *kp;

    auth = conn->rx->route->auth;
    if (!auth->requiredAbilities) {
        /* No abilities are required */
        return 1;
    }
    if (!conn->username) {
        /* User not authenticated */
        return 0;
    }
    if (!conn->user) {
        if (auth->users == 0 || (conn->user = mprLookupKey(auth->users, conn->username)) == 0) {
            mprLog(2, "Can't find user %s", conn->username);
            return 0;
        }
    }
    for (ITERATE_KEYS(requiredAbilities, kp)) {
        if (!mprLookupKey(conn->user->abilities, kp->key)) {
            mprLog(2, "User \"%s\" does not possess the required ability: \"%s\" to access %s", 
                conn->username, kp->key, conn->rx->pathInfo);
            return 0;
        }
    }
    return 1;
}


bool httpLogin(HttpConn *conn, cchar *username, cchar *password)
{
    HttpAuth    *auth;
    HttpSession *session;

    auth = conn->rx->route->auth;
    if (!username || !*username) {
        mprLog(5, "httpLogin missing username");
        return 0;
    }
    conn->username = sclone(username);
#if UNUSED
    conn->password = mprGetMD5(sfmt("%s:%s:%s", conn->username, auth->realm, password));
#else
    conn->password = sclone(password);
    conn->encoded = 0;
#endif
    if (!(auth->store->verifyUser)(conn)) {
        return 0;
    }
    if ((session = httpCreateSession(conn)) != 0) {
        httpSetSessionVar(conn, HTTP_SESSION_AUTHVER, itos(auth->version));
        httpSetSessionVar(conn, HTTP_SESSION_USERNAME, conn->username);
    }
    return 1;
}


bool httpIsAuthenticated(HttpConn *conn)
{
    return conn->authenticated;
}


HttpAuth *httpCreateAuth()
{
    HttpAuth    *auth;
    
    if ((auth = mprAllocObj(HttpAuth, manageAuth)) == 0) {
        return 0;
    }
    httpSetAuthStore(auth, "internal");
    return auth;
}


HttpAuth *httpCreateInheritedAuth(HttpAuth *parent)
{
    HttpAuth      *auth;

    if ((auth = mprAllocObj(HttpAuth, manageAuth)) == 0) {
        return 0;
    }
    if (parent) {
        //  OPT. Structure assignment
        auth->allow = parent->allow;
        auth->deny = parent->deny;
        auth->type = parent->type;
        auth->store = parent->store;
        auth->flags = parent->flags;
        auth->qop = parent->qop;
        auth->realm = parent->realm;
        auth->permittedUsers = parent->permittedUsers;
        auth->requiredAbilities = parent->requiredAbilities;
        auth->users = parent->users;
        auth->roles = parent->roles;
        auth->version = parent->version;
        auth->loggedIn = parent->loggedIn;
        auth->loginPage = parent->loginPage;
        auth->parent = parent;
    }
    return auth;
}


static void manageAuth(HttpAuth *auth, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(auth->allow);
        mprMark(auth->deny);
        mprMark(auth->loggedIn);
        mprMark(auth->loginPage);
        mprMark(auth->permittedUsers);
        mprMark(auth->qop);
        mprMark(auth->realm);
        mprMark(auth->requiredAbilities);
        mprMark(auth->store);
        mprMark(auth->type);
        mprMark(auth->users);
        mprMark(auth->roles);
    }
}


static void manageAuthType(HttpAuthType *type, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(type->name);
    }
}


int httpAddAuthType(Http *http, cchar *name, HttpAskLogin askLogin, HttpParseAuth parseAuth, HttpSetAuth setAuth)
{
    HttpAuthType    *type;

    if ((type = mprAllocObj(HttpAuthType, manageAuthType)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    type->name = sclone(name);
    type->askLogin = askLogin;
    type->parseAuth = parseAuth;
    type->setAuth = setAuth;

    if (mprAddKey(http->authTypes, name, type) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    return 0;
}


static void manageAuthStore(HttpAuthStore *store, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(store->name);
    }
}


/*
    Add a password store backend
 */
int httpAddAuthStore(Http *http, cchar *name, HttpVerifyUser verifyUser)
{
    HttpAuthStore    *store;

    if ((store = mprAllocObj(HttpAuthStore, manageAuthStore)) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    store->name = sclone(name);
    store->verifyUser = verifyUser;
    if (mprAddKey(http->authStores, name, store) == 0) {
        return MPR_ERR_CANT_CREATE;
    }
    return 0;
}


void httpSetAuthAllow(HttpAuth *auth, cchar *allow)
{
    if (auth->allow == 0 || (auth->parent && auth->parent->allow == auth->allow)) {
        auth->allow = mprCreateHash(-1, MPR_HASH_STATIC_VALUES);
    }
    mprAddKey(auth->allow, sclone(allow), auth);
}


void httpSetAuthAnyValidUser(HttpAuth *auth)
{
    auth->permittedUsers = 0;
}


void httpSetAuthAutoLogin(HttpAuth *auth, bool on)
{
    auth->flags &= ~HTTP_AUTO_LOGIN;
    auth->flags |= on ? HTTP_AUTO_LOGIN : 0;
}


void httpSetAuthRequiredAbilities(HttpAuth *auth, cchar *abilities)
{
    char    *ability, *tok;

    auth->requiredAbilities = mprCreateHash(0, 0);
    for (ability = stok(sclone(abilities), " \t,", &tok); abilities; abilities = stok(NULL, " \t,", &tok)) {
        computeAbilities(auth, auth->requiredAbilities, ability);
    }
}


void httpSetAuthDeny(HttpAuth *auth, cchar *client)
{
    if (auth->deny == 0 || (auth->parent && auth->parent->deny == auth->deny)) {
        auth->deny = mprCreateHash(-1, MPR_HASH_STATIC_VALUES);
    }
    mprAddKey(auth->deny, sclone(client), auth);
}


void httpSetAuthOrder(HttpAuth *auth, int order)
{
    auth->flags &= (HTTP_ALLOW_DENY | HTTP_DENY_ALLOW);
    auth->flags |= (order & (HTTP_ALLOW_DENY | HTTP_DENY_ALLOW));
}


/*
    Internal login service routine. Called in response to a post request.
 */
static void loginServiceProc(HttpConn *conn)
{
    HttpAuth    *auth;
    cchar       *username, *password, *referrer, *uri;

    auth = conn->rx->route->auth;
    username = httpGetParam(conn, "username", 0);
    password = httpGetParam(conn, "password", 0);

    if (httpLogin(conn, username, password)) {
        if (sstarts(auth->loggedIn, "referrer")) {
            if ((referrer = httpGetSessionVar(conn, "referrer", 0)) != 0) {
                /*
                    Preserve protocol scheme from existing connection
                 */
                HttpUri *where = httpCreateUri(referrer, 0);
                httpCompleteUri(where, conn->rx->parsedUri);
                referrer = httpUriToString(where, 0);
                httpRedirect(conn, HTTP_CODE_MOVED_TEMPORARILY, referrer);
            } else {
                //  MOB - can we do something simpler?
                if ((uri = schr(referrer, ':')) != 0) {
                    httpRedirect(conn, HTTP_CODE_MOVED_TEMPORARILY, ++uri);
                } else {
                    httpRedirect(conn, HTTP_CODE_MOVED_TEMPORARILY, httpLink(conn, "~", 0));
                }
            }
        } else {
            httpRedirect(conn, HTTP_CODE_MOVED_TEMPORARILY, auth->loggedIn);
        }
    } else {
        httpRedirect(conn, HTTP_CODE_MOVED_TEMPORARILY, auth->loginPage);
    }
}


static void logoutServiceProc(HttpConn *conn)
{
    HttpAuth    *auth;

    auth = conn->rx->route->auth;
    httpRemoveSessionVar(conn, HTTP_SESSION_USERNAME);
    httpRedirect(conn, HTTP_CODE_MOVED_TEMPORARILY, auth->loginPage);
}


void httpSetAuthPost(HttpRoute *parent, cchar *loginPage, cchar *loginService, cchar *logoutService, cchar *loggedIn)
{
    HttpAuth    *auth;
    HttpRoute   *route;

    auth = parent->auth;
    auth->loginPage = sclone(loginPage);
    if (loggedIn) {
        auth->loggedIn = sclone(loggedIn);
    }
    /*
        Create a route without auth for the loginPage
     */
    if ((route = httpCreateInheritedRoute(parent)) != 0) {
        httpSetRoutePattern(route, loginPage, 0);
        route->auth->type = 0;
        httpFinalizeRoute(route);
    }
    if (loginService && *loginService) {
        route = httpBindRoute(parent, loginService, loginServiceProc);
        route->auth->type = 0;
    }
    if (logoutService && *logoutService) {
        route = httpBindRoute(parent, logoutService, logoutServiceProc);
        route->auth->type = 0;
    }
}


void httpSetAuthQop(HttpAuth *auth, cchar *qop)
{
    auth->qop = sclone(qop);
}


void httpSetAuthRealm(HttpAuth *auth, cchar *realm)
{
    auth->realm = sclone(realm);
    auth->version = ((Http*) MPR->httpService)->nextAuth++;
}


void httpSetAuthPermittedUsers(HttpAuth *auth, cchar *users)
{
    char    *user, *tok;

    auth->permittedUsers = mprCreateHash(0, 0);
    for (user = stok(sclone(users), " \t,", &tok); users; users = stok(NULL, " \t,", &tok)) {
        mprAddKey(auth->permittedUsers, user, user);
    }
}


void httpSetAuthSecure(HttpAuth *auth, int enable)
{

    auth->flags &= ~HTTP_SECURE;
    if (enable) {
        auth->flags |= HTTP_SECURE;
    }
}


int httpSetAuthStore(HttpAuth *auth, cchar *store)
{
    Http    *http;

    http = MPR->httpService;
    if ((auth->store = mprLookupKey(http->authStores, store)) == 0) {
        return MPR_ERR_CANT_FIND;
    }
#if BIT_HAS_PAM && BIT_PAM
    if (smatch(store, "pam") && auth->type && smatch(auth->type->name, "digest")) {
        mprError("Can't use PAM password stores with digest authentication");
        return MPR_ERR_BAD_ARGS;
    }
#else
    if (smatch(store, "pam")) {
        mprError("PAM is not supported in the current configuration");
        return MPR_ERR_BAD_ARGS;
    }
#endif
    auth->version = ((Http*) MPR->httpService)->nextAuth++;
    return 0;
}


int httpSetAuthType(HttpAuth *auth, cchar *type, cchar *details)
{
    Http    *http;

    http = MPR->httpService;
    if ((auth->type = mprLookupKey(http->authTypes, type)) == 0) {
        mprError("Can't find auth type %s", type);
        return MPR_ERR_CANT_FIND;
    }
    auth->version = ((Http*) MPR->httpService)->nextAuth++;
    return 0;
}


HttpAuthType *httpLookupAuthType(cchar *type)
{
    Http    *http;

    http = MPR->httpService;
    return mprLookupKey(http->authTypes, type);
}


HttpRole *httpCreateRole(HttpAuth *auth, cchar *name, cchar *abilities)
{
    HttpRole    *role;
    char        *ability, *tok;

    if ((role = mprAllocObj(HttpRole, manageRole)) == 0) {
        return 0;
    }
    role->name = sclone(name);
    role->abilities = mprCreateHash(0, 0);
    for (ability = stok(sclone(abilities), " \t", &tok); ability; ability = stok(NULL, " \t", &tok)) {
        mprAddKey(role->abilities, ability, role);
    }
    return role;
}


static void manageRole(HttpRole *role, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(role->name);
        mprMark(role->abilities);
    }
}


int httpAddRole(HttpAuth *auth, cchar *name, cchar *abilities)
{
    HttpRole    *role;

    if (auth->roles == 0) {
        auth->roles = mprCreateHash(0, 0);
        auth->version = ((Http*) MPR->httpService)->nextAuth++;

    } else if (auth->parent && auth->parent->roles == auth->roles) {
        /* Inherit parent roles */
        auth->roles = mprCloneHash(auth->parent->roles);
        auth->version = ((Http*) MPR->httpService)->nextAuth++;
    }
    if (mprLookupKey(auth->roles, name)) {
        return MPR_ERR_ALREADY_EXISTS;
    }
    if ((role = httpCreateRole(auth, name, abilities)) == 0) {
        return MPR_ERR_MEMORY;
    }
    if (mprAddKey(auth->roles, name, role) == 0) {
        return MPR_ERR_MEMORY;
    }
    mprLog(5, "Role \"%s\" has abilities: %s", role->name, abilities);
    return 0;
}


int httpRemoveRole(HttpAuth *auth, cchar *role)
{
    if (auth->roles == 0 || !mprLookupKey(auth->roles, role)) {
        return MPR_ERR_CANT_ACCESS;
    }
    mprRemoveKey(auth->roles, role);
    return 0;
}


HttpUser *httpCreateUser(HttpAuth *auth, cchar *name, cchar *password, cchar *abilities)
{
    HttpUser    *user;
    char        *ability, *tok;

    if ((user = mprAllocObj(HttpUser, manageUser)) == 0) {
        return 0;
    }
    user->name = sclone(name);
    user->password = sclone(password);
    if (abilities) {
        user->abilities = mprCreateHash(0, 0);
        for (ability = stok(sclone(abilities), " \t,", &tok); ability; ability = stok(NULL, " \t,", &tok)) {
            mprAddKey(user->abilities, ability, user);
        }
        httpComputeUserAbilities(auth, user);
    }
    return user;
}


static void manageUser(HttpUser *user, int flags)
{
    if (flags & MPR_MANAGE_MARK) {
        mprMark(user->password);
        mprMark(user->name);
        mprMark(user->abilities);
    }
}


int httpAddUser(HttpAuth *auth, cchar *name, cchar *password, cchar *abilities)
{
    HttpUser    *user;

    if (auth->users == 0) {
        auth->users = mprCreateHash(-1, 0);
        auth->version = ((Http*) MPR->httpService)->nextAuth++;

    } else if (auth->parent && auth->parent->users == auth->users) {
        auth->users = mprCloneHash(auth->parent->users);
        auth->version = ((Http*) MPR->httpService)->nextAuth++;
    }
    if (mprLookupKey(auth->users, name)) {
        return MPR_ERR_ALREADY_EXISTS;
    }
    if ((user = httpCreateUser(auth, name, password, abilities)) == 0) {
        return MPR_ERR_MEMORY;
    }
    if (mprAddKey(auth->users, name, user) == 0) {
        return MPR_ERR_MEMORY;
    }
    return 0;
}


int httpRemoveUser(HttpAuth *auth, cchar *user)
{
    if (auth->users == 0 || !mprLookupKey(auth->users, user)) {
        return MPR_ERR_CANT_ACCESS;
    }
    mprRemoveKey(auth->users, user);
    return 0;
}


/*
    Compute the set of user abilities. User ability strings can be either roles or abilities. Expand roles into
    the equivalent set of abilities.
 */
static void computeAbilities(HttpAuth *auth, MprHash *abilities, cchar *ability)
{
    MprKey      *ap;
    HttpRole    *role;

    if ((role = mprLookupKey(auth->roles, ability)) != 0) {
        /* Interpret as a role */
        for (ITERATE_KEYS(role->abilities, ap)) {
            if (!mprLookupKey(abilities, ap->key)) {
                mprAddKey(abilities, ap->key, MPR->emptyString);
            }
        }
    } else {
        /* Not found: Interpret as an ability */
        mprAddKey(abilities, ability, MPR->emptyString);
    }
}


/*
    Compute the set of user abilities. User ability strings can be either roles or abilities. Expand roles into
    the equivalent set of abilities.
 */
void httpComputeUserAbilities(HttpAuth *auth, HttpUser *user)
{
    MprHash     *abilities;
    MprKey      *ap;
    MprBuf      *buf;

    abilities = mprCreateHash(0, 0);
    for (ITERATE_KEYS(user->abilities, ap)) {
        computeAbilities(auth, abilities, ap->key);
    }
    user->abilities = abilities;
#if BIT_DEBUG
    buf = mprCreateBuf(0, 0);
    for (ITERATE_KEYS(user->abilities, ap)) {
        mprPutFmtToBuf(buf, "%s ", ap->key);
    }
    mprAddNullToBuf(buf);
    mprLog(5, "User \"%s\" has abilities: %s", user->name, mprGetBufStart(buf));
#endif
}


void httpComputeAllUserAbilities(HttpAuth *auth)
{
    MprKey      *kp;
    HttpUser    *user;

    for (ITERATE_KEY_DATA(auth->users, kp, user)) {
        httpComputeUserAbilities(auth, user);
    }
}


/*
    Verify the user password based on the internal users set. This is used when not using PAM or custom verification.
 */
static bool verifyUser(HttpConn *conn)
{
    HttpRx      *rx;
    HttpAuth    *auth;
    bool        success;

    rx = conn->rx;
    auth = rx->route->auth;
    if (!conn->encoded) {
        conn->password = mprGetMD5(sfmt("%s:%s:%s", conn->username, auth->realm, conn->password));
        conn->encoded = 1;
    }
    if (!conn->user) {
        conn->user = mprLookupKey(auth->users, conn->username);
    }
    if (!conn->user) {
        mprLog(5, "verifyUser: Unknown user \"%s\" for route %s", conn->username, rx->route->name);
        return 0;
    }
    if (rx->passDigest) {
        /* Digest authentication computes a digest using the password as one ingredient */
        return smatch(conn->password, rx->passDigest);
    }
    if ((success = smatch(conn->password, conn->user->password)) != 0) {
        mprLog(5, "verifyUser: User \"%s\" verified for route %s", conn->username, rx->route->name);
    } else {
        mprLog(5, "Password for user \"%s\" failed to verify for route %s", conn->username, rx->route->name);
    }
    return success;
}


/*
    Post authentication callback to ask the user to login via a web form
 */
static void postLogin(HttpConn *conn)
{
    httpRedirect(conn, HTTP_CODE_MOVED_TEMPORARILY, conn->rx->route->auth->loginPage);
}


int httpWriteAuthFile(HttpAuth *auth, char *path)
{
    MprFile         *file;
    MprKey          *kp;
    HttpRole        *role;
    HttpUser        *user;
    char            *tempFile;

    tempFile = mprGetTempPath(NULL);
    if ((file = mprOpenFile(tempFile, O_CREAT | O_TRUNC | O_WRONLY | O_TEXT, 0444)) == 0) {
        mprError("Can't open %s", tempFile);
        return MPR_ERR_CANT_OPEN;
    }
    mprWriteFileFmt(file, "#\n#   %s - Authorization data\n#\n\n", mprGetPathBase(path));

    for (ITERATE_KEY_DATA(auth->roles, kp, role)) {
        mprWriteFileFmt(file, "Role %s %s\n", kp->key, role->abilities);
    }
    mprPutFileChar(file, '\n');
    for (ITERATE_KEY_DATA(auth->users, kp, user)) {
        mprWriteFileFmt(file, "User %s %s\n", user->password, user->abilities);
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

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
