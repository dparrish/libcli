/*
 * libcli and libev integration files.
 *
 * Copyright (c) 2013 by Versa Networks, Inc.
 * All rights reserved.
 *
 *  evcli.h
 */

#ifndef __EVCLI_H__
#define __EVCLI_H__


int cc_cli_svr_init (void                 *evbase,
                     cc_cli_ctx_alloc_cb   alloc_cb,
                     const char           *ip_str,
                     const unsigned short  port);


#endif  /* __EVCLI_H__ */
