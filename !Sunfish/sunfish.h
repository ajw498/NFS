/*
	$Id$
	$URL$

	Bits shared between the module and the frontend
*/

#ifndef SUNFISH_H
#define SUNFISH_H

#define SUNFISH_FILETYPE 0x1b6

/* Types for adding ,xyz extensions */
#define NEVER  0
#define NEEDED 1
#define ALWAYS 2


/* The default port range to bind to. */
#define LOCALPORTMIN_DEFAULT 800
#define LOCALPORTMAX_DEFAULT 900

/* The default buffer size to use. Ideally, this would be 8K, but the
   Castle 100bT podule card cannot handle such large packets */
#define MAXDATABUFFER_UDP_DEFAULT 4096
#define MAXDATABUFFER_TCP_DEFAULT 8192

#endif
