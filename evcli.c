/*
 * libcli and libev integration files.
 *
 * Copyright (c) 2013 by Versa Networks, Inc.
 * All rights reserved.
 *
 *  evcli.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "libcli.h"
#include "evcli.h"

cc_server_ctx_t svr_ctx;


void
cc_conn_cli_read_cb (int fd, short what, void *arg)
{
    int              ret = 0;
    cc_client_t     *client_ctx = (cc_client_t *) arg;
    struct event    *ev1 = client_ctx->cc_ev_clnt.cc_ev;

    ret = cc_conn_input_read(client_ctx, fd);
    if (ret == CLI_EV_WAIT) {
        return;
    } else if (ret == CLI_EV_QUIT) {
        event_del(ev1);
        cc_client_remove(&client_ctx);
    } else if (ret == CLI_EV_TRY_ACTION) {
        ret = cc_conn_cli_action(client_ctx, fd);
        if (ret == CLI_EV_QUIT) {
            event_del(ev1);
            cc_client_remove(&client_ctx);
            return;
        }
    }

    return;
}

static void
cc_conn_listener_cb (struct evconnlistener *listener,
                     evutil_socket_t        fd,
                     struct sockaddr       *sa,
                     int                    socklen,
                     void                  *user_data)
{
    cc_server_ctx_t     *svr_ctx1 = (cc_server_ctx_t *) user_data;
    struct event_base   *base = svr_ctx1->sc_ev_srvr.cc_ev_base;
    struct event        *ev;
    cc_client_t         *client;
    int                  ret = 0;

    ret = cc_client_add(svr_ctx1, &client);
    if (ret == -1) {
        int ret, n;
        const char *string = "Please close existing connection(s)!\n\n";
        n = strlen(string);
        ret = write(fd, string, n);
        if (ret < 0 || ret != n) {
            /* XXX Should handle errors and short writes here. */
            assert(ret == n);           /* Fails. */
        }
        close(fd);
        return;
    }

    ev = event_new(base,
                   fd,
                   EV_TIMEOUT|EV_READ|EV_PERSIST,
                   cc_conn_cli_read_cb,
                   client);
    if (!ev) {
        event_base_loopbreak(base);
        return;
    }

    client->cc_cli_ctx = svr_ctx1->sc_cli_ctx_alloc_cb();
    assert(client->cc_cli_ctx != NULL);

    client->cc_svr_ctx = svr_ctx1;
    client->cc_ev_clnt.cc_ev = ev;
    client->cc_ri = cc_rbuf_default;

    assert(event_add(ev, NULL) == 0);

    cc_conn_cli_setup(client, fd);
}

int
cc_cli_svr_init (void                 *evbase,
                 cc_cli_ctx_alloc_cb   alloc_cb,
                 const char           *ip_str,
                 const unsigned short  port)
{
    struct sockaddr_in     sin;
    struct evconnlistener *listener;

    memset(&sin, 0, sizeof(sin));

    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_family      = AF_INET;
    sin.sin_port        = htons(port);

    listener = evconnlistener_new_bind(evbase,
                                       cc_conn_listener_cb,
                                       (void *) &svr_ctx,
                                       LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE,
                                       -1,
                                       (struct sockaddr*)&sin,
                                       sizeof(sin));
    if (!listener) {
        return -1;
    }

    svr_ctx.sc_ev_srvr.cc_ev_base = evbase;
    svr_ctx.sc_ev_srvr.cc_ev_listener = listener;
    svr_ctx.sc_cli_ctx_alloc_cb = alloc_cb;

    return 0;
}

