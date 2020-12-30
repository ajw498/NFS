/*
	Bits shared between the module and the frontend

        Copyright (C) 2003 Alex Waugh

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef SUNFISH_H
#define SUNFISH_H

#define SUNFISH_FILETYPE 0x1b6
#define SUNFISH_FSNUMBER 188

/* Types for adding ,xyz extensions */
#ifndef NEVER
#define NEVER  0
#define NEEDED 1
#define ALWAYS 2
#endif

/* Syslog */
#define LOGNAME "Sunfish"
#define LOGENTRY 40
#define LOGEXIT  60
#define LOGERROR 20
#define LOGDATASUMMARY 70
#define LOGDATA  130


/* The default port range to bind to. */
#define LOCALPORTMIN_DEFAULT 800
#define LOCALPORTMAX_DEFAULT 900

/* The default buffer size to use. Ideally, this would be 8K, but the
   Castle 100bT podule card cannot handle such large packets */
#define MAXDATABUFFER_UDP_DEFAULT 4096
#define MAXDATABUFFER_TCP_DEFAULT 8192

#define MAXDATABUFFER_UDP_MAX 8192
#define MAXDATABUFFER_TCP_MAX 32768
#define MAXDATABUFFER_INCR    1024

#endif
