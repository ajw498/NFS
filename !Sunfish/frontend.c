/*
	!PHP
	Copyright © Alex Waugh 2000

	$Id: PHPFrontEnd.c,v 1.4 2002/12/13 22:31:10 ajw Exp $


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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/*#include <swis.h>*/
#include <stdarg.h>

#include "Event.h"
#include "oslib/gadget.h"
#include "oslib/radiobutton.h"
#include "oslib/optionbutton.h"
#include "oslib/writablefield.h"
#include "oslib/numberrange.h"
#include "oslib/window.h"


#define event_main_SHOW 0x100
#define event_main_SET  0x101
#define event_main_HIDE 0x102
#define event_QUIT 0x103
#define event_main_PCNFSD 0x104

#define event_ports_SHOW 0x200
#define event_ports_SET  0x201

#define event_filenames_SHOW 0x300
#define event_filenames_SET  0x301

#define event_connection_SHOW 0x400
#define event_connection_SET  0x401

#define gadget_ports_PORTMAPPER  0x2
#define gadget_ports_NFS         0x5
#define gadget_ports_PCNFSD      0x7
#define gadget_ports_MOUNT       0xf
#define gadget_ports_LOCALMIN    0x9
#define gadget_ports_LOCALMAX    0xd
#define gadget_ports_MACHINE     0x0

#define gadget_filenames_SHOWHIDDEN       0x6
#define gadget_filenames_FOLLOWSYMLINKS   0x9
#define gadget_filenames_SYMLINKLEVELS    0x4
#define gadget_filenames_CASESENSITIVE    0x3
#define gadget_filenames_DEFAULTFILETYPE  0x1
#define gadget_filenames_ADDEXTALWAYS     0xb
#define gadget_filenames_ADDEXTNEEDED     0xc
#define gadget_filenames_ADDEXTNEVER      0xd
#define gadget_filenames_UNUMASK          0xe

#define gadget_connection_DATABUFFER      0x8
#define gadget_connection_PIPELINING      0x2
#define gadget_connection_TIMEOUT         0xc
#define gadget_connection_RETRIES         0xd
#define gadget_connection_LOGGING         0x5

#define gadget_main_SERVER                0x0
#define gadget_main_EXPORT                0x1
#define gadget_main_USERNAME              0x2
#define gadget_main_PASSWORD              0x3
#define gadget_main_UID                   0x6
#define gadget_main_GIDS                  0x7
#define gadget_main_UMASK                 0xa
#define gadget_main_USEPCNFSD             0x4

#define UNUSED(x) ((void)x)

#define E(x) { \
	os_error *err=x;\
	if (err!=NULL) {\
		xwimp_report_error(err,0,"Sunfish",NULL);\
		exit(EXIT_FAILURE);\
	}\
}

#define STRMAX 256

struct mount {
	osbool showhidden;
	int followsymlinks;
	osbool casesensitive;
	int defaultfiletype;
	int addext;
	int unumask;
	int portmapperport;
	int nfsport;
	int pcnfsdport;
	int mountport;
	int localportmin;
	int localportmax;
	char machinename[STRMAX];
	int maxdatabuffer;
	osbool pipelining;
	int timeout;
	int retries;
	osbool logging;
	char server[STRMAX];
	char export[STRMAX];
	char username[STRMAX];
	char password[STRMAX];
	int uid;
	char gids[STRMAX];
	int umask;
	osbool usepcnfsd;
};

static struct mount mount = {0,5,1,0xfff,1,0777,111,0,0,0,800,900,"fudge",2048,1,3,2,0,"mint","/home/foo","user","pass",100,"345 3",022,1};
/*
#define Syslog_LogMessage 0x4c880 
void syslogf(char *logname, int level, char *fmt, ...)
{
	static char syslogbuf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(syslogbuf, sizeof(syslogbuf), fmt, ap);
	va_end(ap);

	* Ignore any errors, as there's not much we can do with them *
	_swix(Syslog_LogMessage, _INR(0,2), logname, syslogbuf, level);
}*/

static toolbox_o filenamesid;
static toolbox_o portsid;
static toolbox_o connectionid;

static osbool filenames_open(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);
	char tmp[4];

	E(xoptionbutton_set_state(0, id_block->this_obj, gadget_filenames_SHOWHIDDEN, mount.showhidden));
	sprintf(tmp, /*sizeof(tmp),*/ "%d", mount.followsymlinks);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_filenames_SYMLINKLEVELS, tmp));
	E(xoptionbutton_set_state(0, id_block->this_obj, gadget_filenames_FOLLOWSYMLINKS, mount.followsymlinks != 0));
	E(xoptionbutton_set_state(0, id_block->this_obj, gadget_filenames_CASESENSITIVE, mount.casesensitive));
	sprintf(tmp, /*sizeof(tmp),*/ "%X", mount.defaultfiletype);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_filenames_DEFAULTFILETYPE, tmp));
	switch (mount.addext) {
		case 2:
			E(xradiobutton_set_state(0, id_block->this_obj, gadget_filenames_ADDEXTALWAYS, TRUE));
			break;
		case 1:
			E(xradiobutton_set_state(0, id_block->this_obj, gadget_filenames_ADDEXTNEEDED, TRUE));
			break;
		default:
			E(xradiobutton_set_state(0, id_block->this_obj, gadget_filenames_ADDEXTNEVER, TRUE));
			break;
	}
	sprintf(tmp, /*sizeof(tmp),*/ "%.3o", mount.unumask);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_filenames_UNUMASK, tmp));

	return 1;
}

static osbool filenames_set(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);
	char tmp[4];
	toolbox_c selected;

	E(xoptionbutton_get_state(0, id_block->this_obj, gadget_filenames_SHOWHIDDEN, &mount.showhidden));
	E(xoptionbutton_get_state(0, id_block->this_obj, gadget_filenames_FOLLOWSYMLINKS, &mount.followsymlinks));
	if (mount.followsymlinks) {
		E(xwritablefield_get_value(0, id_block->this_obj, gadget_filenames_SYMLINKLEVELS, tmp, sizeof(tmp), NULL));
		mount.followsymlinks = atoi(tmp);
	}
	E(xoptionbutton_get_state(0, id_block->this_obj, gadget_filenames_CASESENSITIVE, &mount.casesensitive));
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_filenames_DEFAULTFILETYPE, tmp, sizeof(tmp), NULL));
	mount.defaultfiletype = (int)strtol(tmp, NULL, 16);
	E(xradiobutton_get_state(0, id_block->this_obj, gadget_filenames_ADDEXTALWAYS, NULL, &selected));
	switch (selected) {
		case gadget_filenames_ADDEXTALWAYS:
			mount.addext = 2;
			break;
		case gadget_filenames_ADDEXTNEEDED:
			mount.addext = 1;
			break;
		default:
			mount.addext = 0;
			break;
	}
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_filenames_UNUMASK, tmp, sizeof(tmp), NULL));
	mount.unumask = (int)strtol(tmp, NULL, 8);

	return 1;
}

static osbool ports_open(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);
	char tmp[11];

	sprintf(tmp, /*sizeof(tmp),*/ "%d", mount.portmapperport);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_ports_PORTMAPPER, mount.portmapperport ? tmp : ""));
	sprintf(tmp, /*sizeof(tmp),*/ "%d", mount.nfsport);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_ports_NFS, mount.nfsport ? tmp : ""));
	sprintf(tmp, /*sizeof(tmp),*/ "%d", mount.pcnfsdport);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_ports_PCNFSD, mount.pcnfsdport ? tmp : ""));
	sprintf(tmp, /*sizeof(tmp),*/ "%d", mount.mountport);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_ports_MOUNT, mount.mountport ? tmp : ""));
	sprintf(tmp, /*sizeof(tmp),*/ "%d", mount.localportmin);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_ports_LOCALMIN, mount.localportmin ? tmp : ""));
	sprintf(tmp, /*sizeof(tmp),*/ "%d", mount.localportmax);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_ports_LOCALMAX, mount.localportmax ? tmp : ""));
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_ports_MACHINE, mount.machinename));

	return 1;
}

static osbool ports_set(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);
	char tmp[11];

	E(xwritablefield_get_value(0, id_block->this_obj, gadget_ports_PORTMAPPER, tmp, sizeof(tmp), NULL));
	mount.portmapperport = atoi(tmp);
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_ports_NFS, tmp, sizeof(tmp), NULL));
	mount.nfsport = atoi(tmp);
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_ports_PCNFSD, tmp, sizeof(tmp), NULL));
	mount.pcnfsdport = atoi(tmp);
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_ports_MOUNT, tmp, sizeof(tmp), NULL));
	mount.mountport = atoi(tmp);
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_ports_LOCALMIN, tmp, sizeof(tmp), NULL));
	mount.localportmin = atoi(tmp);
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_ports_LOCALMAX, tmp, sizeof(tmp), NULL));
	mount.localportmax = atoi(tmp);
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_ports_MACHINE, mount.machinename, sizeof(mount.machinename), NULL));

	return 1;
}

static osbool connection_open(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);
	char tmp[11];

	E(xnumberrange_set_value(0, id_block->this_obj, gadget_connection_DATABUFFER, ((mount.maxdatabuffer & ~63) - 1 & 0x1FFF) + 1));
	E(xoptionbutton_set_state(0, id_block->this_obj, gadget_connection_PIPELINING, mount.pipelining));
	sprintf(tmp, /*sizeof(tmp),*/ "%d", mount.timeout);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_connection_TIMEOUT, tmp));
	sprintf(tmp, /*sizeof(tmp),*/ "%d", mount.retries);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_connection_RETRIES, tmp));
	E(xoptionbutton_set_state(0, id_block->this_obj, gadget_connection_LOGGING, mount.logging));

	return 1;
}

static osbool connection_set(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);
	char tmp[11];

	E(xnumberrange_get_value(0, id_block->this_obj, gadget_connection_DATABUFFER, &mount.maxdatabuffer));
	E(xoptionbutton_get_state(0, id_block->this_obj, gadget_connection_PIPELINING, &mount.pipelining));
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_connection_TIMEOUT, tmp, sizeof(tmp), NULL));
	mount.timeout = atoi(tmp);
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_connection_RETRIES, tmp, sizeof(tmp), NULL));
	mount.retries = atoi(tmp);
	E(xoptionbutton_get_state(0, id_block->this_obj, gadget_connection_LOGGING, &mount.logging));

	return 1;
}

static osbool pcnfsd_toggle(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);

	osbool usepcnfsd;
	gadget_flags flags;

	E(xradiobutton_get_state(0, id_block->this_obj, gadget_main_USEPCNFSD, &usepcnfsd, NULL));

	E(xgadget_get_flags(0, id_block->this_obj, gadget_main_USERNAME, &flags));
	if (usepcnfsd) flags &= ~gadget_FADED; else flags |= gadget_FADED;
	E(xgadget_set_flags(0, id_block->this_obj, gadget_main_USERNAME, flags));

	E(xgadget_get_flags(0, id_block->this_obj, gadget_main_PASSWORD, &flags));
	if (usepcnfsd) flags &= ~gadget_FADED; else flags |= gadget_FADED;
	E(xgadget_set_flags(0, id_block->this_obj, gadget_main_PASSWORD, flags));

	E(xgadget_get_flags(0, id_block->this_obj, gadget_main_UID, &flags));
	if (usepcnfsd) flags |= gadget_FADED; else flags &= ~gadget_FADED;
	E(xgadget_set_flags(0, id_block->this_obj, gadget_main_UID, flags));

	E(xgadget_get_flags(0, id_block->this_obj, gadget_main_GIDS, &flags));
	if (usepcnfsd) flags |= gadget_FADED; else flags &= ~gadget_FADED;
	E(xgadget_set_flags(0, id_block->this_obj, gadget_main_GIDS, flags));

	E(xgadget_get_flags(0, id_block->this_obj, gadget_main_UMASK, &flags));
	if (usepcnfsd) flags |= gadget_FADED; else flags &= ~gadget_FADED;
	E(xgadget_set_flags(0, id_block->this_obj, gadget_main_UMASK, flags));

	return 1;
}

static osbool mainwin_open(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);
	char tmp[11];

	E(xwritablefield_set_value(0, id_block->this_obj, gadget_main_SERVER, mount.server));
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_main_EXPORT, mount.export));
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_main_USERNAME, mount.username));
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_main_PASSWORD, mount.password));
	sprintf(tmp, /*sizeof(tmp),*/ "%d", mount.uid);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_main_UID, tmp));
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_main_GIDS, mount.gids));
	sprintf(tmp, /*sizeof(tmp),*/ "%.3o", mount.umask);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_main_UMASK, tmp));
	E(xradiobutton_set_state(0, id_block->this_obj, gadget_main_USEPCNFSD, mount.usepcnfsd));

	pcnfsd_toggle(event_code, event, id_block, handle);
	return 1;
}

static osbool mainwin_set(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);
	char tmp[11];

	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_SERVER, mount.server, sizeof(tmp), NULL));
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_EXPORT, mount.export, sizeof(tmp), NULL));
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_USERNAME, mount.username, sizeof(tmp), NULL));
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_PASSWORD, mount.password, sizeof(tmp), NULL));
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_UID, tmp, sizeof(tmp), NULL));
	mount.uid = atoi(tmp);
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_GIDS, mount.gids, sizeof(tmp), NULL));
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_UMASK, tmp, sizeof(tmp), NULL));
	mount.umask = (int)strtol(tmp, NULL, 8);
	E(xradiobutton_get_state(0, id_block->this_obj, gadget_main_USEPCNFSD, &mount.usepcnfsd, NULL));

	return 1;
}

static osbool mainwin_close(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	toolbox_hide_object(0, filenamesid);
	toolbox_hide_object(0, portsid);
	toolbox_hide_object(0, connectionid);
	return 1;
}

static osbool quit(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	exit(EXIT_SUCCESS);
	return 1;
}

static osbool autocreated(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);

	if (strcmp(event->data.created.name,"filenames") == 0) {
		filenamesid = id_block->this_obj;
	}
	if (strcmp(event->data.created.name,"ports") == 0) {
		portsid = id_block->this_obj;
	}
	if (strcmp(event->data.created.name,"connection") == 0) {
		connectionid = id_block->this_obj;
	}

	return 1;
}

static int message_quit(wimp_message *message,void *handle)
{
	UNUSED(message);
	UNUSED(handle);

	exit(EXIT_SUCCESS);
	return 1;
}


int main(void)
{
	int toolbox_events[] = {0};
	int wimp_messages[] = {0};
	wimp_event_no event_code;
	wimp_block poll_block;
	messagetrans_control_block messages;
	toolbox_block id_block;

	if (xtoolbox_initialise(0, 310, (wimp_message_list*)wimp_messages,
							(toolbox_action_list*)toolbox_events,
							"<Sunfish$Dir>", &messages, &id_block, 0, 0, 0)
							!= NULL) exit(EXIT_FAILURE);
	event_initialise(&id_block);
	event_set_mask(1+256);

	event_register_toolbox_handler(event_ANY, event_QUIT, quit, NULL);
	event_register_toolbox_handler(event_ANY, event_main_SHOW, mainwin_open, NULL);
	event_register_toolbox_handler(event_ANY, event_main_HIDE, mainwin_close, NULL);
	event_register_toolbox_handler(event_ANY, event_main_SET,  mainwin_set, NULL);
	event_register_toolbox_handler(event_ANY, event_main_PCNFSD,  pcnfsd_toggle, NULL);

	event_register_toolbox_handler(event_ANY, event_filenames_SHOW, filenames_open, NULL);
	event_register_toolbox_handler(event_ANY, event_filenames_SET,  filenames_set, NULL);

	event_register_toolbox_handler(event_ANY, event_connection_SHOW, connection_open, NULL);
	event_register_toolbox_handler(event_ANY, event_connection_SET,  connection_set, NULL);

	event_register_toolbox_handler(event_ANY, event_ports_SHOW, ports_open, NULL);
	event_register_toolbox_handler(event_ANY, event_ports_SET,  ports_set, NULL);

	event_register_toolbox_handler(event_ANY, action_OBJECT_AUTO_CREATED, autocreated, NULL);
	event_register_message_handler(message_QUIT, message_quit, NULL);

	while (TRUE) event_poll(&event_code, &poll_block, 0);

}
