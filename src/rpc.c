/*
	$Id$

	Functions to implement RPC calling. All network access is done in this file.


	Copyright (C) 2003 Alex Waugh
	
	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "sunfish.h"
#include "utils.h"

#include "rpc-calls.h"
#include "portmapper-calls.h"
#ifdef NFS3
#include "mount3-calls.h"
#include "nfs3-calls.h"
#else
#include "mount1-calls.h"
#include "nfs2-calls.h"
#endif
#include "pcnfsd-calls.h"
#include "rpc.h"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <socklib.h>
#include <time.h>
#include <sys/time.h>
#include <swis.h>
#include <sys/errno.h>
#include <unixlib.h>



/* A monotonicly increasing transaction id */
static unsigned int nextxid = 0;

/* A pool of rx buffers. */
static struct buffer_list *buffers = NULL;
static int numbuffers = 0;

enum request_status {
  EMPTY,
  TXWAIT,
  TX,
  RXWAIT,
  RX,
  HELD,
  DONE
};

/* A list for keeping track of requests and their replies. We need to
   remember the entire call buffer in case the call gets lost and we
   need to retransmit */
struct request_entry {
	enum request_status status;
	struct conn_info *conn;
	int reference;
	unsigned int retries;
	int error;
	time_t lastactivity;
	struct buffer_list tx;
	void *extradata;
	int extralen;
	struct buffer_list *rx;
	struct request_entry *next;
	struct request_entry *nextpipelined;
	struct request_entry *prevpipelined;
};

static struct request_entry *request_entries = NULL;
static int numentries = 0;

/* These point to the current buffer for tx or rx, and are used by
   the process_* macros to read/write data from/to */
char *ibuf;
char *ibufend;
char *obuf;
char *obufend;

/* The last txentry created, used to identify which to transmit next */
static struct request_entry *txentry;

/* The last rxentry decoded, used to identify the one to hold */
static struct request_entry *lastrxentry;


static void free_rx_buffer(struct buffer_list *freebuffer)
{
	struct buffer_list *buffer = buffers;
	struct buffer_list **prev = &buffers;

	if (freebuffer == NULL) return;

	while (buffer) {
		if (buffer == freebuffer) {
			buffer->len = -1;
			if (numbuffers > FIFOSIZE) {
				*prev = buffer->next;
				free(buffer);
				numbuffers--;
			}
			return;
		}
		prev = &(buffer->next);
		buffer = buffer->next;
	}
}

void rpc_free_all_buffers(void)
{
	struct request_entry *entry = request_entries;
	struct buffer_list *buffer = buffers;

	while (entry) {
		struct request_entry *next = entry->next;
		free(entry);
		entry = next;
	}
	request_entries = NULL;
	numentries = 0;

	while (buffer) {
		struct buffer_list *next = buffer->next;
		free(buffer);
		buffer = next;
	}
	buffers = NULL;
	numbuffers = 0;
}

static struct request_entry *get_request_entry(struct conn_info *conn)
{
	struct request_entry *entry = request_entries;
	struct request_entry *foundentry = NULL;
	struct request_entry *foundpipelined = NULL;

	/* Search for an existing unused entry */
	while (entry) {
		if (entry->status == EMPTY) {
			foundentry = entry;
		} else if ((entry->status != HELD) &&
		           (entry->conn == conn) &&
		           (entry->reference == conn->reference) &&
		           (entry->nextpipelined == NULL)) {
			foundpipelined = entry;
		}
		entry = entry->next;
	}

	entry = foundentry;

	if (entry == NULL) {
		entry = malloc(sizeof(struct request_entry));
		if (entry == NULL) return NULL;
		entry->next = request_entries;
		request_entries = entry;
		numentries++;
	} else {
		free_rx_buffer(entry->rx);
	}

	entry->nextpipelined = NULL;
	entry->prevpipelined = foundpipelined;
	if (foundpipelined) foundpipelined->nextpipelined = entry;

	entry->status = TXWAIT;
	entry->conn = conn;
	entry->reference = conn->reference;
	entry->retries = 0;
	entry->lastactivity = clock();
	entry->error = 0;
	entry->rx = NULL;
	entry->tx.len = 0;
	entry->extralen = 0;

	return entry;
}

static struct buffer_list *get_rx_buffer(void)
{
	struct buffer_list *buffer = buffers;

	/* Search for an existing unused entry */
	while (buffer) {
		if (buffer->len == -1) break;
		buffer = buffer->next;
	}

	if (buffer == NULL) {
		buffer = malloc(sizeof(struct buffer_list));
		if (buffer == NULL) return NULL;
		buffer->next = buffers;
		buffers = buffer;
		numbuffers++;
	}

	buffer->len = 0;
	buffer->position = 0;
	buffer->readlen = sizeof(int);

	return buffer;
}

void rpc_free_request_entry(struct conn_info *conn)
{
	struct request_entry *entry = request_entries;
	struct request_entry **prev = &request_entries;

	/* Search for matching entries, and remove from list if we have enough already */
	while (entry) {
		struct request_entry *next = entry->next;
		if (entry->conn == conn) {
			entry->status = EMPTY;
			free_rx_buffer(entry->rx);
			entry->rx = NULL;
			if (numentries > FIFOSIZE) {
				*prev = next;
				free(entry);
				numentries--;
			} else {
				prev = &(entry->next);
			}
		} else {
			prev = &(entry->next);
		}
		entry = next;
	}
}

/* Swap the active rx buffer */
void rpc_hold_rxbuffer(void)
{
	lastrxentry->status = HELD;
}

static int rpc_create_socket(struct conn_info *conn)
{
	int on = 1;

	conn->sock = socket(AF_INET, conn->tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
	if (conn->sock < 0) return errno;

	/* Make the socket non-blocking */
	if (ioctl(conn->sock, FIONBIO, &on) < 0) {
		close(conn->sock);
		conn->sock = -1;
		return errno;
	}

	if (conn->localportmax != 0) {
		/* Use a specific local port */
		struct sockaddr_in name;
		int port = conn->localportmin;
		int ret;

		memset(&name, 0, sizeof(name));
		name.sin_family = AF_INET;
		name.sin_addr.s_addr = (int)htonl(INADDR_ANY);

		do {
			name.sin_port = htons(port++);
			ret = bind(conn->sock, (struct sockaddr *)&name, sizeof(name));
		} while (ret != 0 && port <= conn->localportmax);

		if (ret) {
			close(conn->sock);
			conn->sock = -1;
			return errno;
		}
	}

	if (conn->server == NULL) {
		if (setsockopt(conn->sock, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) return errno;
	}

	return 0;
}

/* Initialise for each mount */
os_error *rpc_init_connection(struct conn_info *conn)
{
	struct hostent *hp;
	os_error *err;
	int ret;

	if (nextxid == 0) {
		/* Initialise xid to a random number, to avoid issues with
		   duplicate request caches if the module and frontend use
		   the same xid */
		nextxid = rand();
#ifdef MODULE
		nextxid |= 0x80000000;
#endif
	}

	ret = rpc_create_socket(conn);
	if (ret) return gen_error(RPCERRBASE + 4,"Unable to connect or bind socket (%s)", xstrerror(ret));

	memset(&(conn->sockaddr), 0, sizeof(conn->sockaddr));

	if (conn->server) {
		err = gethostbyname_timeout(conn->server, conn->timeout, &hp);
		if (err) return err;

		memcpy(&(conn->sockaddr.sin_addr), hp->h_addr, hp->h_length);
	}
	conn->sockaddr.sin_family = AF_INET;
	conn->sockaddr.sin_port = 0;

	return NULL;
}

/* Close for each mount */
os_error *rpc_close_connection(struct conn_info *conn)
{
	int ret = 0;

	rpc_free_request_entry(conn);

	if (conn->sock < 0) return NULL;

	ret = close(conn->sock);
	conn->sock = -1;

	if (ret) return gen_error(RPCERRBASE + 3, "Socket close failed (%s)", xstrerror(errno));

	return NULL;
}

/* Log an entire data packet */
static void logdata(int rx, char *buf, int len)
{
	int i;

	syslogf(LOGNAME, LOGDATASUMMARY, "%s data (%d): rm/xid %.2x%.2x%.2x%.2x %.2x%.2x%.2x%.2x", rx ? "rx" : "tx", len, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
	for (i=0; i<(len & ~3); i+=4) syslogf(LOGNAME, LOGDATA, "  %.2x %.2x %.2x %.2x", buf[i], buf[i+1], buf[i+2], buf[i+3]);
	for (i=0; i<(len & 3); i++) syslogf(LOGNAME, LOGDATA, "  %.2x", buf[(len &~3) + i]);
}

/* Setup buffer and write call header to it */
os_error *rpc_prepare_call(unsigned int prog, unsigned int vers, unsigned int proc, struct conn_info *conn)
{
	struct rpc_msg call_header;

	call_header.body.mtype = CALL;
	call_header.body.u.cbody.rpcvers = RPC_VERSION;
	call_header.body.u.cbody.verf.flavor = AUTH_NULL;
	call_header.body.u.cbody.verf.body.size = 0;

	call_header.xid = nextxid++;

	call_header.body.u.cbody.prog = prog;
	call_header.body.u.cbody.vers = vers;
	call_header.body.u.cbody.proc = proc;

	if (conn->auth) {
		call_header.body.u.cbody.cred.flavor = AUTH_UNIX;
		call_header.body.u.cbody.cred.body.size = conn->authsize;
		call_header.body.u.cbody.cred.body.data = conn->auth;
	} else {
		call_header.body.u.cbody.cred.flavor = AUTH_NULL;
		call_header.body.u.cbody.cred.body.size = 0;
	}

	txentry = get_request_entry(conn);
	if (txentry == NULL) return gen_error(NOMEM, NOMEMMESS);
	obuf = txentry->tx.buffer;
	obufend = obuf + SFBUFFERSIZE;

	/* Leave room for the record marker */
	if (conn->tcp) obuf += 4;

	process_rpc_msg(OUTPUT, &call_header, conn->pool);
	return NULL;
}

static int rpc_connect_socket(struct conn_info *conn)
{
	int ret;

	if (conn->tcp) {
		ret = rpc_create_socket(conn);
		if (ret) return ret;
	}

	if (conn->server) {
		ret = connect(conn->sock, (struct sockaddr *)&(conn->sockaddr), sizeof(struct sockaddr_in));
		if (ret == -1 && errno != EINPROGRESS) {
			return errno;
		}
	}
	return 0;
}

/* Send as much data as we can without blocking */
static int poll_tx(struct request_entry *entry)
{
	int ret = -1;
	struct msghdr msg;
	struct iovec iovec[3];
	char zero[] = {0,0,0,0};

	if (entry->conn->sock == -1) {
		ret = rpc_connect_socket(entry->conn);
		if (ret) return ret;
	}

	if (entry->conn->server) {
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags = 0;
		msg.msg_iov = iovec;
		if (entry->tx.position < entry->tx.len) {
			msg.msg_iovlen = entry->extralen ? ((entry->extralen & 3) ? 3 : 2) : 1;
			iovec[0].iov_base = entry->tx.buffer + entry->tx.position;
			iovec[0].iov_len = entry->tx.len - entry->tx.position;
			iovec[1].iov_base = entry->extradata;
			iovec[1].iov_len = entry->extralen;
			iovec[2].iov_base = zero;
			iovec[2].iov_len = 4 - (entry->extralen & 3);
		} else if (entry->tx.position < entry->tx.len + entry->extralen) {
			msg.msg_iovlen = (entry->extralen & 3) ? 2 : 1;
			iovec[0].iov_base = ((char *)(entry->extradata)) + (entry->tx.position - entry->tx.len);
			iovec[0].iov_len = entry->extralen - (entry->tx.position - entry->tx.len);
			iovec[1].iov_base = zero;
			iovec[1].iov_len = 4 - (entry->extralen & 3);
		} else {
			msg.msg_iovlen = 1;
			iovec[0].iov_base = zero;
			iovec[0].iov_len = 4 - (entry->tx.position & 3);
		}
		ret = sendmsg(entry->conn->sock, &msg, 0);
#ifndef MODULE
	} else {
		/* Broadcast */
		struct ifconf ifc;
		char buf[BUFSIZ];
		char *ptr;

		ifc.ifc_len = sizeof(buf);
		ifc.ifc_buf = buf;
		if (socketioctl(entry->conn->sock, SIOCGIFCONF, (char *)&ifc) < 0) return errno;

		ptr = buf;
		while(ptr < buf + ifc.ifc_len) {
			struct sockaddr_in dst;
			int len;
			struct ifreq *ifr = (struct ifreq *)ptr;

			len = ifr->ifr_addr.sa_len;
			ptr += sizeof(ifr->ifr_name) + len;

			if (ifr->ifr_addr.sa_family != AF_INET) continue;

			if (ioctl(entry->conn->sock, SIOCGIFFLAGS, (char *)ifr) < 0) return errno;

			/* Skip uninteresting cases */
			if (((ifr->ifr_flags & IFF_UP) == 0) ||
			    (ifr->ifr_flags & IFF_LOOPBACK) ||
			    ((ifr->ifr_flags & (IFF_BROADCAST | IFF_POINTOPOINT)) == 0)) continue;

			if (ifr->ifr_flags & IFF_BROADCAST) {
				if (socketioctl(entry->conn->sock, SIOCGIFBRDADDR, (char *)ifr) < 0) return errno;
				memcpy(&dst, &(ifr->ifr_broadaddr), sizeof(ifr->ifr_broadaddr));
			} else {
				if (socketioctl(entry->conn->sock, SIOCGIFDSTADDR, (char *)ifr) < 0) return errno;
				memcpy(&dst, &(ifr->ifr_dstaddr), sizeof(ifr->ifr_dstaddr));
			}
			dst.sin_port = entry->conn->sockaddr.sin_port;
			ret = sendto(entry->conn->sock, entry->tx.buffer, entry->tx.len, 0, (struct sockaddr *)&dst, sizeof(dst));
		}
#endif
	}

	if (ret == -1) {
		if ((errno == EWOULDBLOCK) || (errno == ENOTCONN)) return 0;
		return errno;
	}
	if (ret > 0) {
		entry->tx.position += ret;
	}

	if (entry->tx.position >= (entry->tx.len + ((entry->extralen + 3) & ~3))) {
		if (entry->conn->tcp) entry->conn->txmutex = 0;
		entry->status = RXWAIT;
	}
	return 0;
}

/* Search all outstanding tx buffers for a matching xid to the just recieved rx buffer */
static void match_rx_buffer(struct buffer_list *currentrx)
{
	struct request_entry *entry = request_entries;

	while (entry) {
		if (((unsigned *)(entry->tx.buffer))[entry->conn->tcp ? 1 : 0] == ((unsigned *)(currentrx->buffer))[0]) {
			/* xid match */
			if (entry->rx) {
				/* This xid has already recieved a reply, so this must be a duplicate. */
				free_rx_buffer(currentrx);
				return;
			}
			entry->rx = currentrx;
			if ((entry->status == RXWAIT) || (entry->status == RX)) entry->status = DONE;
			return;
		}
		entry = entry->next;
	}
	/* Not found so must be a delayed duplicate, or garbage */
	free_rx_buffer(currentrx);
}

static struct sockaddr_in addr;

char *rpc_get_last_host(void)
{
	static char host[16];

	snprintf(host, sizeof(host), "%d.%d.%d.%d", addr.sin_addr.s_addr & 0xFF,
	                                            addr.sin_addr.s_addr >> 8 & 0xFF,
	                                            addr.sin_addr.s_addr >> 16 & 0xFF,
	                                            addr.sin_addr.s_addr >> 24);
	return host;
}

/* Recieve as much data as we can without blocking */
static int poll_rx(struct conn_info *conn)
{
	int ret;
	int len;
	struct buffer_list *currentrx;
	int addrlen = sizeof(addr);

	if (conn->tcp) {
		currentrx = conn->rxmutex;
		if (currentrx == NULL) return ENOMEM; /* Should be impossible */
		if (currentrx->readlen) {
			len = currentrx->readlen;
		} else {
			len = currentrx->len - currentrx->position;
		}
	} else {
		currentrx = get_rx_buffer();
		if (currentrx == NULL) return ENOMEM;
		len = SFBUFFERSIZE;
	}

	ret = recvfrom(conn->sock, currentrx->buffer + currentrx->position, len, 0, (struct sockaddr *)&addr, &addrlen);
	if (ret == -1) {
		if ((errno == EWOULDBLOCK) || (errno == ENOTCONN)) {
			if (!conn->tcp) free_rx_buffer(currentrx);
			return 0;
		}
		free_rx_buffer(currentrx);
		if (conn->tcp) conn->rxmutex = NULL;
		return errno;
	}
	if (ret > 0) {
		currentrx->position += ret;
		if (conn->tcp) {
			if (currentrx->readlen) {
				currentrx->readlen -= ret;
				if (currentrx->readlen == 0) {
					/* Whole record marker read, so decode */
					len  = (currentrx->buffer[currentrx->position - 1]);
					len |= (currentrx->buffer[currentrx->position - 2]) << 8;
					len |= (currentrx->buffer[currentrx->position - 3]) << 16;
					len |= (currentrx->buffer[currentrx->position - 4] & 0x7F) << 24;
					currentrx->lastsegment = (currentrx->buffer[currentrx->position - 4] & 0x80) >> 7;
					currentrx->position -= sizeof(int);
					currentrx->len += len;
					if (currentrx->len > SFBUFFERSIZE) return EMSGSIZE;
				}
			}
		} else {
			currentrx->len = ret;
		}
		if (currentrx->position >= currentrx->len) {
			if (conn->tcp && !currentrx->lastsegment) {
				/* Another record to read before the request is complete */
				currentrx->readlen = sizeof(int);
			} else {
				if (enablelog) logdata(1, currentrx->buffer, currentrx->len);
				match_rx_buffer(currentrx);
				if (conn->tcp) conn->rxmutex = NULL;
			}
		}
	} else {
		if (!conn->tcp) free_rx_buffer(currentrx);
	}
	return 0;
}

static void handle_error(struct request_entry *entry, int err)
{
	if (entry->conn->tcp && (entry->conn->sock != -1)) {
		close(entry->conn->sock);
		entry->conn->sock = -1;
		if (entry->status == TX) entry->conn->txmutex = 0;
		if (entry->status == RX) {
			free_rx_buffer(entry->conn->rxmutex);
			entry->conn->rxmutex = NULL;
		}
	}
	if (entry->retries <= entry->conn->retries) {
		/* Try again */
		entry->status = TXWAIT;
	} else {
		entry->error = err;
		entry->status = DONE;
	}
}

static void poll_connections(void)
{
	struct request_entry *entry = request_entries;
	time_t t = clock();
	int ret;

#ifdef MODULE
	/* This is the only point at which reentrancy can occur */
	trigger_callback();
#endif

	while (entry) {
		switch (entry->status) {
		case TXWAIT:
			if (entry->conn->txmutex) break;
			if (entry->conn->tcp) entry->conn->txmutex = 1;
			entry->status = TX;
			entry->lastactivity = t;
			entry->retries++;
			entry->tx.position = 0;
			entry->error = 0;
			/* Fallthrough */
		case TX:
			if (t > entry->lastactivity + entry->conn->timeout * CLOCKS_PER_SEC) {
				ret = ETIMEDOUT;
			} else {
				ret = poll_tx(entry);
			}
			if (ret) handle_error(entry, ret);
			if (entry->status != RXWAIT) break;
		case RXWAIT:
			if (entry->rx) entry->status = DONE;
			if (entry->conn->tcp) {
				if (entry->conn->rxmutex) break;
				entry->conn->rxmutex = get_rx_buffer();
				if (entry->conn->rxmutex == NULL) {
					entry->error = ENOMEM;
					entry->status = DONE;
					break;
				}
			}
			entry->status = RX;
			/* Fallthrough */
		case RX:
			if (t > entry->lastactivity + entry->conn->timeout * CLOCKS_PER_SEC) {
				ret = ETIMEDOUT;
			} else {
				ret = poll_rx(entry->conn);
			}
			if (ret) handle_error(entry, ret);
			break;
		case EMPTY:
		case HELD:
		case DONE:
			break;
		}
		entry = entry->next;
	}
}

/* Send the already filled in tx buffer, the read the response and process
   the rpc reply header */
os_error *rpc_do_call(int prog, enum callctl calltype, void *extradata, int extralen, struct conn_info *conn)
{
	struct rpc_msg reply_header;
	struct request_entry *rxentry = request_entries;

	/* Search for the oldest entry we could return */
	while (rxentry) {
		if ((rxentry->status != EMPTY) && (rxentry->status != HELD) &&
		    (rxentry->conn == conn) && (rxentry->reference == conn->reference) &&
		    (rxentry->prevpipelined == NULL)) {
			break;
		}
		rxentry = rxentry->next;
	}
	if (rxentry == NULL) return gen_error(RPCERRBASE + 5, "Cannot find rxentry to use");

	if (calltype == TXBLOCKING || calltype == TXNONBLOCKING) {
		int port;
		unsigned int recordmarker;

		/* Choose the port to use */
		switch (prog) {
		case PMAP_RPC_PROGRAM:
			port = htons(conn->portmapper_port);
			break;
		case MOUNT_RPC_PROGRAM:
			port = htons(conn->mount_port);
			break;
		case PCNFSD_RPC_PROGRAM:
			port = htons(conn->pcnfsd_port);
			break;
		case NFS_RPC_PROGRAM:
		default:
			port = htons(conn->nfs_port);
			break;
		}

		/* Only connect the socket if the port is different from the last
		   time we used the socket, or it is not yet connected at all */
		if (port != conn->sockaddr.sin_port || conn->sock == -1) {
			int ret;

			conn->sockaddr.sin_port = port;

			if (conn->tcp && (conn->sock != -1)) close(conn->sock);

			ret = rpc_connect_socket(conn);
			if (ret) return gen_error(RPCERRBASE + 4,"Unable to connect or bind socket (%s)", xstrerror(ret));
		}

		txentry->tx.len = obuf - txentry->tx.buffer;
		txentry->extradata = extradata;
		txentry->extralen = extralen;

		if (conn->tcp) {
			/* Insert the record marker at the start of the buffer */
			recordmarker = 0x80000000 | (txentry->tx.len + ((extralen + 3) & ~3) - 4);
			obuf = txentry->tx.buffer;
			obufend = obuf + 4;
			if (process_unsigned(OUTPUT, &recordmarker, conn->pool)) goto buffer_overflow;
		}

		if (enablelog) logdata(0, txentry->tx.buffer, txentry->tx.len);

	} else {
		/* RX only, so disable the txentry */
		txentry->status = EMPTY;
		if (txentry->prevpipelined) txentry->prevpipelined->nextpipelined = NULL;
	}

	do {
		poll_connections();
	} while ((calltype == TXBLOCKING || calltype == RXBLOCKING) && rxentry->status != DONE);

	/* Return if the oldest request has not been recieved. This will only
	   ever be the case if we are non-blocking */
	if (rxentry->status != DONE) return ERR_WOULDBLOCK;

	/* Remember this rxentry, so we know which one to use if we are asked to hold it */
	lastrxentry = rxentry;

	/* Mark as empty so it can be reused by the next call, and remove
	   from the pipeline queue. Unless it was a broadcast, in which
	   case retain it for more replies */
	rxentry->status = conn->server ? EMPTY : RXWAIT;
	if (rxentry->nextpipelined) rxentry->nextpipelined->prevpipelined = NULL;

	if (rxentry->error) {
		return gen_error(RPCERRBASE + 8, "Error when sending or recieving data (%s)",xstrerror(rxentry->error));
	}

	/* Setup buffers and parse header */
	ibuf = rxentry->rx->buffer;
	ibufend = ibuf + rxentry->rx->len;

	/* If broadcasting then remove rx buffer so we can recieve another reply */
	if (conn->server == NULL) rxentry->rx = NULL;

	if (process_rpc_msg(INPUT, &reply_header, conn->pool)) goto buffer_overflow;

	/* Check that the RPC completed successfully */
	if (reply_header.body.mtype != REPLY) return gen_error(RPCERRBASE + 10, "Unexpected response (not an RPC reply)");
	if (reply_header.body.u.rbody.stat != MSG_ACCEPTED) {
		if (reply_header.body.u.rbody.u.rreply.stat == AUTH_ERROR) {
			return gen_error(RPCERRBASE + 11, "RPC message rejected (Authentication error)");
		} else {
			return gen_error(RPCERRBASE + 12, "RPC message rejected (%d)", reply_header.body.u.rbody.u.rreply.stat);
		}
	}
	if (reply_header.body.u.rbody.u.areply.reply_data.stat != SUCCESS) {
		return gen_error(RPCERRBASE + 13, "RPC failed (%d)", reply_header.body.u.rbody.u.areply.reply_data.stat);
	}

	return NULL;

buffer_overflow:
	return rpc_buffer_overflow();
}


