/*
 * Copyright (c) 2012 Jiri Svoboda
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup inet
 * @{
 */
/**
 * @file
 * @brief
 */

#ifndef INET_H_
#define INET_H_

#include <adt/list.h>
#include <bool.h>
#include <inet/iplink.h>
#include <ipc/loc.h>
#include <sys/types.h>
#include <async.h>

/** Inet Client */
typedef struct {
	async_sess_t *sess;
	uint8_t protocol;
	link_t client_list;
} inet_client_t;

/** Inetping Client */
typedef struct {
	/** Callback session */
	async_sess_t *sess;
	/** Session identifier */
	uint16_t ident;
	/** Link to client list */
	link_t client_list;
} inetping_client_t;

/** Host address */
typedef struct {
	uint32_t ipv4;
} inet_addr_t;

/** Network address */
typedef struct {
	/** Address */
	uint32_t ipv4;
	/** Number of valid bits in @c ipv4 */
	int bits;
} inet_naddr_t;

/** Address object info */
typedef struct {
	/** Network address */
	inet_naddr_t naddr;
	/** Link service ID */
	sysarg_t ilink;
	/** Address object name */
	char *name;
} inet_addr_info_t;

/** IP link info */
typedef struct {
	/** Link service name */
	char *name;
	/** Default MTU */
	size_t def_mtu;
} inet_link_info_t;

/** Static route info */
typedef struct {
	/** Destination network address */
	inet_naddr_t dest;
	/** Router address */
	inet_addr_t router;
	/** Static route name */
	char *name;
} inet_sroute_info_t;

typedef struct {
	/** Source address */
	inet_addr_t src;
	/** Destination address */
	inet_addr_t dest;
	/** Type of service */
	uint8_t tos;
	/** Protocol */
	uint8_t proto;
	/** Time to live */
	uint8_t ttl;
	/** Identifier */
	uint16_t ident;
	/** Do not fragment */
	bool df;
	/** More fragments */
	bool mf;
	/** Offset of fragment into datagram, in bytes */
	size_t offs;
	/** Packet data */
	void *data;
	/** Packet data size in bytes */
	size_t size;
} inet_packet_t;

typedef struct {
	inet_addr_t src;
	inet_addr_t dest;
	uint8_t tos;
	void *data;
	size_t size;
} inet_dgram_t;

typedef struct {
	link_t link_list;
	service_id_t svc_id;
	char *svc_name;
	async_sess_t *sess;
	iplink_t *iplink;
	size_t def_mtu;
} inet_link_t;

typedef struct {
	link_t addr_list;
	sysarg_t id;
	inet_naddr_t naddr;
	inet_link_t *ilink;
	char *name;
} inet_addrobj_t;

/** Static route configuration */
typedef struct {
	link_t sroute_list;
	sysarg_t id;
	/** Destination network */
	inet_naddr_t dest;
	/** Router via which to route packets */
	inet_addr_t router;
	char *name;
} inet_sroute_t;

typedef enum {
	/** Destination is on this network node */
	dt_local,
	/** Destination is directly reachable */
	dt_direct,
	/** Destination is behind a router */
	dt_router
} inet_dir_type_t;

/** Direction (next hop) to a destination */
typedef struct {
	/** Route type */
	inet_dir_type_t dtype;
	/** Address object (direction) */
	inet_addrobj_t *aobj;
	/** Local destination address */
	inet_addr_t ldest;
} inet_dir_t;

typedef struct {
	inet_addr_t src;
	inet_addr_t dest;
	uint16_t seq_no;
	void *data;
	size_t size;
} inetping_sdu_t;

extern int inet_ev_recv(inet_client_t *, inet_dgram_t *);
extern int inet_recv_packet(inet_packet_t *);
extern int inet_route_packet(inet_dgram_t *, uint8_t, uint8_t, int);
extern int inet_get_srcaddr(inet_addr_t *, uint8_t, inet_addr_t *);
extern int inet_recv_dgram_local(inet_dgram_t *, uint8_t);

#endif

/** @}
 */
