/*
    basic.c - Basic Authorization 

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

/*********************************** Code *************************************/
/*
    Parse the client 'Authorization' header and the server 'Www-Authenticate' header
 */
int httpBasicParse(HttpConn *conn)
{
    HttpRx  *rx;
    char    *decoded, *cp;

    if (conn->endpoint) {
        rx = conn->rx;
        if ((decoded = mprDecode64(rx->authDetails)) == 0) {
            return MPR_ERR_BAD_FORMAT;
        }
        if ((cp = strchr(decoded, ':')) != 0) {
            *cp++ = '\0';
        }
        conn->username = sclone(decoded);
        conn->password = sclone(cp);
        conn->encoded = 0;
#if UNUSED
        conn->password = mprGetMD5(sfmt("%s:%s:%s", conn->username, conn->rx->route->auth->realm, cp));
#endif
    }
    return 0;
}


/*
    Respond to the request by asking for a client login
 */
void httpBasicLogin(HttpConn *conn)
{
    HttpAuth    *auth;

    auth = conn->rx->route->auth;
    httpSetHeader(conn, "WWW-Authenticate", "Basic realm=\"%s\"", auth->realm);
    httpError(conn, HTTP_CODE_UNAUTHORIZED, "Access Denied. Login required");
}


/*
    Add the client 'Authorization' header for authenticated requests
 */
void httpBasicSetHeaders(HttpConn *conn)
{
    httpAddHeader(conn, "Authorization", "basic %s", mprEncode64(sfmt("%s:%s", conn->username, conn->password)));
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
