/*
	$Id: $

	RPC calling functions
*/

#ifndef RPC_H
#define RPC_H

#include "kernel.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define FHSIZE 32
/**/

typedef _kernel_oserror os_error;  /* Neatify */

struct conn_info {
	char *server;
	unsigned int portmapper_port;
	unsigned int mount_port;
	unsigned int nfs_port;
	char *export;
	char rootfh[FHSIZE];
	char *config;
	struct sockaddr_in sockaddr;
	int sock;
	long timeout;
	int retries;
	int hidden;
	int umask;
	char *gids;
	int uid;
	int gid;
	char *username;
	char *password;
	int logging;
	char *auth;
	int authsize;
	char *machinename;
};

void rpc_init_header(void);

void rpc_prepare_call(unsigned int prog, unsigned int vers, unsigned int proc, struct conn_info *conn);

os_error *rpc_do_call(struct conn_info *conn);

#define rpc_buffer_overflow() gen_error(1,"rpc Buffer overflow")

os_error *gen_error(int num, char *msg, ...); /* This shouldn't be in this file */

extern int enablelog;

void syslogf(char *logname, int level, char *fmt, ...);

void log_error(os_error *err);

#define ERRBASE 1
#define RPCERRBASE 1

void *llmalloc(size_t size);

void swap_rxbuffers(void);

os_error *rpc_init_connection(struct conn_info *conn);
os_error *rpc_close_connection(struct conn_info *conn);

#endif
