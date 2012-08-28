/*
    authPam.c - Authorization using PAM (Pluggable Authorization Module)

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */

/********************************* Includes ***********************************/

#include    "http.h"

#if BIT_HAS_PAM && BIT_PAM
 #include    <security/pam_appl.h>

/********************************* Defines ************************************/

typedef struct {
    char    *name;
    char    *password;
} UserInfo;

/********************************* Forwards ***********************************/

static int pamChat(int msgCount, const struct pam_message **msg, struct pam_response **resp, void *data);

/*********************************** Code *************************************/

bool httpPamVerifyUser(HttpConn *conn)
{
    pam_handle_t        *pamh;
    UserInfo            info;
    struct pam_conv     conv = { pamChat, &info };
    struct group        *gp;
    int                 res;
   
    mprAssert(conn->username);
    mprAssert(conn->password);

    info.name = (char*) conn->username;
    info.password = (char*) conn->password;
    pamh = NULL;
        
    if ((res = pam_start("login", conn->username, &conv, &pamh)) != PAM_SUCCESS) {
        return 0;
    }
    if ((res = pam_authenticate(pamh, 0)) != PAM_SUCCESS) {
        return 0;
    }
    pam_end(pamh, PAM_SUCCESS);

    if (!conn->user) {
        conn->user = mprLookupKey(conn->rx->route->auth->users, conn->username);
    }
    if (!conn->user && (gp = getgrgid(getgid())) != 0) {
        /* Create a temporary user with an ability set to the group */
        conn->user = httpCreateUser(conn->rx->route->auth, conn->username, 0, gp->gr_name);
    }
    return 1;
}


/*  
    Callback invoked by the pam_authenticate function
 */
static int pamChat(int msgCount, const struct pam_message **msg, struct pam_response **resp, void *data) 
{
    UserInfo                *info;
    struct pam_response     *reply;
    int                     i;
    
    i = 0;
    reply = 0;
    info = (UserInfo*) data;

    if (resp == 0 || msg == 0 || info == 0) {
        return PAM_CONV_ERR;
    }
    //  MOB - who frees this?
    if ((reply = malloc(msgCount * sizeof(struct pam_response))) == 0) {
        return PAM_CONV_ERR;
    }
    for (i = 0; i < msgCount; i++) {
        reply[i].resp_retcode = 0;
        reply[i].resp = 0;
        
        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_ON:
            reply[i].resp = strdup(info->name);
            break;

        case PAM_PROMPT_ECHO_OFF:
            //  MOB - what is this doing?
            reply[i].resp = strdup(info->password);
            break;

        default:
            return PAM_CONV_ERR;
        }
    }
    *resp = reply;
    return PAM_SUCCESS;
}
#endif /* BIT_HAS_PAM */

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
