/*
	$Id$
	$URL$

	Frontend for creating mount files


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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <stdarg.h>
#include <stddef.h>
#include <ctype.h>
#include <unixlib.h>

#include "Event.h"

#include "oslib/gadget.h"
#include "oslib/menu.h"
#include "oslib/radiobutton.h"
#include "oslib/optionbutton.h"
#include "oslib/writablefield.h"
#include "oslib/numberrange.h"
#include "oslib/window.h"
#include "oslib/iconbar.h"
#include "oslib/proginfo.h"

#include "oslib/osfile.h"
#include "oslib/osfscontrol.h"
#include "oslib/osgbpb.h"
#include "oslib/wimp.h"
#include "oslib/filer.h"

#include "moduledefs.h"
#include "sunfish.h"

#define event_main_SHOW 0x100
#define event_main_SET  0x101
#define event_main_HIDE 0x102
#define event_QUIT 0x103
#define event_main_PCNFSD 0x104
#define event_MOUNT 0x105
#define event_SHOWMOUNTS 0x106
#define event_REMOVEICON 0x107
#define event_HELP 0x108
#define event_EDITMENU 0x109
#define event_NEWMOUNT 0x10a

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

#define gadget_filenames_SHOWHIDDENALWAYS 0x10
#define gadget_filenames_SHOWHIDDENROOT   0x11
#define gadget_filenames_SHOWHIDDENNEVER  0x12
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
#define gadget_connection_TCP             0x12

#define gadget_main_SERVER                0x0
#define gadget_main_EXPORT                0x1
#define gadget_main_USERNAME              0x2
#define gadget_main_PASSWORD              0x3
#define gadget_main_UID                   0x6
#define gadget_main_GIDS                  0x7
#define gadget_main_UMASK                 0xa
#define gadget_main_USEPCNFSD             0x4
#define gadget_main_DONTUSEPCNFSD         0x5
#define gadget_main_LEAFNAME              0x15

#define gadget_menu_REMOVE                0x3

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
	osbool tcp;
	char leafname[STRMAX];
};

static struct mount mount;

static toolbox_o filenamesid;
static toolbox_o portsid;
static toolbox_o connectionid;
static toolbox_o mainwinid;
static toolbox_o mainmenuid;
static toolbox_o editmenuid;


static toolbox_o unmountedicon;

struct mounticon {
	toolbox_o icon;
	wimp_i iconhandle;
	char filename[1024];
	struct mounticon *next;
};

static struct mounticon *iconhead = NULL;

static void mount_save(char *filename)
{
	FILE *file;
	file = fopen(filename, "w");
	if (file == NULL) {
		xwimp_report_error((os_error*)_kernel_last_oserror(),0,"Sunfish",NULL);
		return;
	}

	fprintf(file,"Protocol: NFS2\n");
	fprintf(file,"Server: %s\n",mount.server);
	fprintf(file,"Export: %s\n",mount.export);
	if (mount.usepcnfsd) {
		fprintf(file,"Password: %s\n",mount.password);
		fprintf(file,"Username: %s\n",mount.username);
	} else {
		int gid;
		char *gids;
		fprintf(file,"uid: %d\n",mount.uid);
		gid = (int)strtol(mount.gids, &gids, 10);
		fprintf(file,"gid: %d\n",gid);
		fprintf(file,"gids: %s\n",gids);
		fprintf(file,"umask: %.3o\n",mount.umask);
	}
	fprintf(file,"Transport: %s\n",mount.tcp ? "TCP" : "UDP");
	fprintf(file,"ShowHidden: %d\n",mount.showhidden);
	fprintf(file,"FollowSymlinks: %d\n",mount.followsymlinks);
	fprintf(file,"CaseSensitive: %d\n",mount.casesensitive);
	fprintf(file,"DefaultFiletype: %.3X\n",mount.defaultfiletype);
	fprintf(file,"AddExt: %d\n",mount.addext);
	fprintf(file,"unumask: %.3o\n",mount.unumask);
	if (mount.portmapperport) fprintf(file,"PortmapperPort: %d\n",mount.portmapperport);
	if (mount.nfsport) fprintf(file,"NFSPort: %d\n",mount.nfsport);
	if (mount.pcnfsdport) fprintf(file,"PCNFSDPort: %d\n",mount.pcnfsdport);
	if (mount.mountport) fprintf(file,"MountPort: %d\n",mount.mountport);
	if (mount.localportmin && mount.localportmin) fprintf(file,"LocalPort: %d %d\n",mount.localportmin,mount.localportmax);
	if (mount.machinename[0]) fprintf(file,"MachineName: %s\n",mount.machinename);
	fprintf(file,"MaxDataBuffer: %d\n",mount.maxdatabuffer);
	fprintf(file,"Pipelining: %d\n",mount.pipelining);
	fprintf(file,"Timeout: %d\n",mount.timeout);
	fprintf(file,"Retries: %d\n",mount.retries);
	fprintf(file,"Logging: %d\n",mount.logging);
	fclose(file);

	E(xosfile_set_type(filename, SUNFISH_FILETYPE));
}

#define CHECK(str) (strncasecmp(line,str,sizeof(str))==0)

static void mount_load(char *filename)
{
	FILE *file;
	char buffer[STRMAX];
	char *leafname;

	file = fopen(filename, "r");
	if (file == NULL) {
		xwimp_report_error((os_error*)_kernel_last_oserror(),0,"Sunfish",NULL);
		return;
	}

	leafname = strrchr(filename, '.');
	if (leafname == NULL) {
		leafname = filename;
	} else {
		leafname++;
	}

	snprintf(mount.leafname, STRMAX, "%s", leafname);

	while (fgets(buffer, STRMAX, file) != NULL) {
		char *val;
		char *line;

		/* Strip trailing whitespace */
		val = buffer;
		while (*val) val++;
		while (val > buffer && isspace(val[-1])) val--;
		*val = '\0';

		/* Strip leading whitespace */
		line = buffer;
		while (isspace(*line)) line++;

		/* Find the end of the field */
		val = line;
		while (*val && *val != ':') val++;
		if (*val) *val++ = '\0';
		/* Find the start of the value */
		while (isspace(*val)) val++;

		if (CHECK("#")) {
			/* A comment */
		} else if (CHECK("Protocol")) {
		} else if (CHECK("Server")) {
			strcpy(mount.server, val);
		} else if (CHECK("MachineName")) {
			strcpy(mount.machinename, val);
		} else if (CHECK("PortMapperPort")) {
			mount.portmapperport = (int)strtol(val, NULL, 10);
		} else if (CHECK("MountPort")) {
			mount.mountport = (int)strtol(val, NULL, 10);
		} else if (CHECK("NFSPort")) {
			mount.nfsport = (int)strtol(val, NULL, 10);
		} else if (CHECK("PCNFSDPort")) {
			mount.pcnfsdport = (int)strtol(val, NULL, 10);
		} else if (CHECK("Transport")) {
			mount.tcp = strcasecmp(val, "tcp") == 0;
		} else if (CHECK("Export")) {
			strcpy(mount.export, val);
		} else if (CHECK("UID")) {
			mount.usepcnfsd = 0;
			mount.uid = (int)strtol(val, NULL, 10);
		} else if (CHECK("GID") || CHECK("GIDs")) {
			strncat(mount.gids, val, STRMAX - strlen(mount.gids));
			mount.gids[STRMAX - 1] = '\0';
		} else if (CHECK("Username")) {
			mount.usepcnfsd = 1;
			strcpy(mount.username, val);
		} else if (CHECK("Password")) {
			strcpy(mount.password, val);
		} else if (CHECK("Logging")) {
			mount.logging = (int)strtol(val, NULL, 10);
		} else if (CHECK("umask")) {
			mount.umask = 07777 & (int)strtol(val, NULL, 8); /* umask is specified in octal */
		} else if (CHECK("unumask")) {
			mount.unumask = 07777 & (int)strtol(val, NULL, 8); /* unumask is specified in octal */
		} else if (CHECK("ShowHidden")) {
			mount.showhidden = (int)strtol(val, NULL, 10);
		} else if (CHECK("Timeout")) {
			mount.timeout = (int)strtol(val, NULL, 10);
		} else if (CHECK("Retries")) {
			mount.retries = (int)strtol(val, NULL, 10);
		} else if (CHECK("DefaultFiletype")) {
			mount.defaultfiletype = 0xFFF & (int)strtol(val, NULL, 16);
		} else if (CHECK("AddExt")) {
			mount.addext = (int)strtol(val, NULL, 10);
		} else if (CHECK("LocalPort")) {
			char *end;

			mount.localportmin = (int)strtol(val, &end, 10);
			mount.localportmax = (int)strtol(end, NULL, 10);
			if (mount.localportmax == 0) mount.localportmax = mount.localportmin;
		} else if (CHECK("Pipelining")) {
			mount.pipelining = (int)strtol(val, NULL, 10);
		} else if (CHECK("MaxDataBuffer")) {
			mount.maxdatabuffer = (int)strtol(val, NULL, 10);
		} else if (CHECK("FollowSymlinks")) {
			mount.followsymlinks = (int)strtol(val, NULL, 10);
		} else if (CHECK("CaseSensitive")) {
			mount.casesensitive = (int)strtol(val, NULL, 10);
		}
	}
	fclose(file);
}

static osbool mount_remove(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);

	struct mounticon *remove;

	if (iconhead->icon == id_block->parent_obj) {
		remove = iconhead;
		if (iconhead->next == NULL) {
			E(xtoolbox_show_object(0, unmountedicon, toolbox_POSITION_DEFAULT, NULL, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT));
			E(xmenu_set_fade(0, mainmenuid, gadget_menu_REMOVE, TRUE));
		}
		iconhead = iconhead->next;
	} else {
		struct mounticon *current = iconhead;

		while (current->next && current->next->icon != id_block->parent_obj) current = current->next;
		remove = current->next;
		if (remove == NULL) return 1;
		current->next = remove->next;
	}
	E(xtoolbox_delete_object(0, remove->icon));
	free(remove);

	return 1;
}

static osbool mount_open(bits event_code, toolbox_action *event, toolbox_block *id_block, void *mountdetails)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);

	char buf[1024];

	snprintf(buf, sizeof(buf), "Filer_OpenDir %s",((struct mounticon *)mountdetails)->filename);
	E(xwimp_start_task(buf, NULL));

	return 1;
}

static osbool help(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	E(xwimp_start_task("Filer_Run <Sunfish$Dir>.!Help", NULL));

	return 1;
}

static osbool mount_showall(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	E(xwimp_start_task("Filer_OpenDir Sunfish:mounts", NULL));

	return 1;
}

static void add_mount(char *filename)
{
	toolbox_o icon;
	char *leaf;
	wimp_i oldicon;
	struct mounticon *mountdetails;
	struct mounticon *details;

	for (details = iconhead; details != NULL; details = details->next) {
		/* Don't add an icon if we already have one for this mount */
		if (strcmp(details->filename, filename) == 0) return;
	}
	
	mountdetails = malloc(sizeof(struct mounticon));
	if (mountdetails == NULL) return;

	strcpy(mountdetails->filename, filename);

	leaf = strrchr(mountdetails->filename,'.');
	if (leaf == NULL) leaf = mountdetails->filename; else leaf++;

	E(xtoolbox_create_object(0, (toolbox_id)"mounted", &icon));
	E(xiconbar_set_text(0, icon, leaf));
 	event_register_toolbox_handler(icon, event_MOUNT,  mount_open, mountdetails);

	E(xiconbar_get_icon_handle(0, iconhead == NULL ? unmountedicon : iconhead->icon, &oldicon));
	E(xtoolbox_show_object(0, icon, toolbox_POSITION_FULL, (toolbox_position*)&oldicon, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT));

	if (iconhead == NULL) {
		E(xtoolbox_hide_object(0, unmountedicon));
		E(xmenu_set_fade(0, mainmenuid, gadget_menu_REMOVE, FALSE));
	}
	E(xiconbar_get_icon_handle(0, icon, &(mountdetails->iconhandle)));
	mountdetails->icon = icon;
	mountdetails->next = iconhead;
	iconhead = mountdetails;
}

static osbool filenames_open(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);
	char tmp[4];

	switch (mount.showhidden) {
		case 1:
			E(xradiobutton_set_state(0, id_block->this_obj, gadget_filenames_SHOWHIDDENALWAYS, TRUE));
			break;
		case 2:
			E(xradiobutton_set_state(0, id_block->this_obj, gadget_filenames_SHOWHIDDENROOT, TRUE));
			break;
		default:
			E(xradiobutton_set_state(0, id_block->this_obj, gadget_filenames_SHOWHIDDENNEVER, TRUE));
			break;
	}
	snprintf(tmp, sizeof(tmp), "%d", mount.followsymlinks);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_filenames_SYMLINKLEVELS, tmp));
	E(xoptionbutton_set_state(0, id_block->this_obj, gadget_filenames_FOLLOWSYMLINKS, mount.followsymlinks != 0));
	E(xoptionbutton_set_state(0, id_block->this_obj, gadget_filenames_CASESENSITIVE, mount.casesensitive));
	snprintf(tmp, sizeof(tmp), "%X", mount.defaultfiletype);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_filenames_DEFAULTFILETYPE, tmp));
	switch (mount.addext) {
		case ALWAYS:
			E(xradiobutton_set_state(0, id_block->this_obj, gadget_filenames_ADDEXTALWAYS, TRUE));
			break;
		case NEEDED:
			E(xradiobutton_set_state(0, id_block->this_obj, gadget_filenames_ADDEXTNEEDED, TRUE));
			break;
		default:
			E(xradiobutton_set_state(0, id_block->this_obj, gadget_filenames_ADDEXTNEVER, TRUE));
			break;
	}
	snprintf(tmp, sizeof(tmp), "%.3o", mount.unumask);
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

	E(xradiobutton_get_state(0, id_block->this_obj, gadget_filenames_SHOWHIDDENALWAYS, NULL, &selected));
	switch (selected) {
		case gadget_filenames_SHOWHIDDENALWAYS:
			mount.showhidden = 1;
			break;
		case gadget_filenames_SHOWHIDDENROOT:
			mount.showhidden = 2;
			break;
		default:
			mount.showhidden = 0;
			break;
	}
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
			mount.addext = ALWAYS;
			break;
		case gadget_filenames_ADDEXTNEEDED:
			mount.addext = NEEDED;
			break;
		default:
			mount.addext = NEVER;
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

	snprintf(tmp, sizeof(tmp), "%d", mount.portmapperport);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_ports_PORTMAPPER, mount.portmapperport ? tmp : ""));
	snprintf(tmp, sizeof(tmp), "%d", mount.nfsport);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_ports_NFS, mount.nfsport ? tmp : ""));
	snprintf(tmp, sizeof(tmp), "%d", mount.pcnfsdport);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_ports_PCNFSD, mount.pcnfsdport ? tmp : ""));
	snprintf(tmp, sizeof(tmp), "%d", mount.mountport);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_ports_MOUNT, mount.mountport ? tmp : ""));
	snprintf(tmp, sizeof(tmp), "%d", mount.localportmin);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_ports_LOCALMIN, mount.localportmin ? tmp : ""));
	snprintf(tmp, sizeof(tmp), "%d", mount.localportmax);
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
	snprintf(tmp, sizeof(tmp), "%d", mount.timeout);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_connection_TIMEOUT, tmp));
	snprintf(tmp, sizeof(tmp), "%d", mount.retries);
	E(xwritablefield_set_value(0, id_block->this_obj, gadget_connection_RETRIES, tmp));
	E(xoptionbutton_set_state(0, id_block->this_obj, gadget_connection_LOGGING, mount.logging));
	E(xradiobutton_set_state(0, id_block->this_obj, gadget_connection_TCP, mount.tcp));

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
	E(xradiobutton_get_state(0, id_block->this_obj, gadget_connection_TCP, &mount.tcp, NULL));

	return 1;
}

static osbool pcnfsd_toggle(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	osbool usepcnfsd;
	gadget_flags flags;

	E(xradiobutton_get_state(0, mainwinid, gadget_main_USEPCNFSD, &usepcnfsd, NULL));

	E(xgadget_get_flags(0, mainwinid, gadget_main_USERNAME, &flags));
	if (usepcnfsd) flags &= ~gadget_FADED; else flags |= gadget_FADED;
	E(xgadget_set_flags(0, mainwinid, gadget_main_USERNAME, flags));

	E(xgadget_get_flags(0, mainwinid, gadget_main_PASSWORD, &flags));
	if (usepcnfsd) flags &= ~gadget_FADED; else flags |= gadget_FADED;
	E(xgadget_set_flags(0, mainwinid, gadget_main_PASSWORD, flags));

	E(xgadget_get_flags(0, mainwinid, gadget_main_UID, &flags));
	if (usepcnfsd) flags |= gadget_FADED; else flags &= ~gadget_FADED;
	E(xgadget_set_flags(0, mainwinid, gadget_main_UID, flags));

	E(xgadget_get_flags(0, mainwinid, gadget_main_GIDS, &flags));
	if (usepcnfsd) flags |= gadget_FADED; else flags &= ~gadget_FADED;
	E(xgadget_set_flags(0, mainwinid, gadget_main_GIDS, flags));

	E(xgadget_get_flags(0, mainwinid, gadget_main_UMASK, &flags));
	if (usepcnfsd) flags |= gadget_FADED; else flags &= ~gadget_FADED;
	E(xgadget_set_flags(0, mainwinid, gadget_main_UMASK, flags));

	return 1;
}

static void mainwin_setup(char *mountname)
{
	mount.showhidden = 1;
	mount.followsymlinks = 5;
	mount.casesensitive = 0;
	mount.defaultfiletype = 0xFFF;
	mount.addext = 1;
	mount.unumask = 0;
	mount.portmapperport = 111;
	mount.nfsport = 0;
	mount.pcnfsdport = 0;
	mount.mountport = 0;
	mount.localportmin = LOCALPORTMIN_DEFAULT;
	mount.localportmax = LOCALPORTMAX_DEFAULT;
	mount.machinename[0] = '\0';
	mount.maxdatabuffer = MAXDATABUFFER_UDP_DEFAULT;
	mount.pipelining = 0;
	mount.timeout = 3;
	mount.retries = 2;
	mount.logging = 0;
	mount.server[0] = '\0';
	mount.export[0] = '\0';
	mount.username[0] = '\0';
	mount.password[0] = '\0';
	mount.uid = 0;
	mount.gids[0] = '\0';
	mount.umask = 022;
	mount.usepcnfsd = 0;
	mount.tcp = 0;
	mount.leafname[0] = '\0';

	if (mountname) {
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "Sunfish:mounts.%s", mountname);
		mount_load(buffer);
	}

}

#define MAX_MOUNTS 100

static int numentries = -1;
static struct menu_entry_object menuentry[MAX_MOUNTS];

static osbool editmenu_selection(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);

	if (id_block->this_obj != editmenuid) return 0;
	if (id_block->this_cmp > numentries) return 0;

	mainwin_setup(menuentry[id_block->this_cmp].text);

	E(xtoolbox_show_object(0, mainwinid, toolbox_POSITION_CENTRED, NULL, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT));

	return 1;
}

/* Create a menu of all mount files */
static osbool editmenu_create(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	int start = 0;
	int read;
	char buffer[256];

	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	/* Remove all entries from the menu */
	if (numentries == -1) {
		E(xmenu_remove_entry(0, editmenuid, 0));
	} else {
		for (int i = 0; i < numentries; i++) {
			E(xmenu_remove_entry(0, editmenuid, i));
			free(menuentry[i].text);
		}
	}

	numentries = 0;

	while (start != -1 && numentries < MAX_MOUNTS) {
		E(xosgbpb_dir_entries("Sunfish:mounts", (osgbpb_string_list *)buffer, 1, start, sizeof(buffer), NULL, &read, &start));
		if (read != 0) {
			menuentry[numentries].flags = 0;
			menuentry[numentries].cmp = numentries;
			menuentry[numentries].text = strdup(buffer);
			if (menuentry[numentries].text == NULL) return 1;
			menuentry[numentries].text_limit = strlen(menuentry[numentries].text) + 1;
			menuentry[numentries].click_object_name = NULL;
			menuentry[numentries].sub_menu_object_name = NULL;
			menuentry[numentries].sub_menu_action = 0;
			menuentry[numentries].click_action =  0;
			menuentry[numentries].help = NULL;
			menuentry[numentries].help_limit = 0;

			E(xmenu_add_entry(0, editmenuid, menu_ADD_AT_END, &(menuentry[numentries]), NULL));
			numentries++;
		}
	}

	if (numentries == 0) {
		/* Give the menu one dummy entry */
		menuentry[numentries].flags = menu_ENTRY_FADED;
		menuentry[numentries].cmp = numentries;
		menuentry[numentries].text = "No mounts";
		menuentry[numentries].text_limit = strlen(menuentry[numentries].text) + 1;
		menuentry[numentries].click_object_name = NULL;
		menuentry[numentries].sub_menu_object_name = NULL;
		menuentry[numentries].sub_menu_action = 0;
		menuentry[numentries].click_action =  0;
		menuentry[numentries].help = NULL;
		menuentry[numentries].help_limit = 0;

		E(xmenu_add_entry(0, editmenuid, menu_ADD_AT_END, &(menuentry[numentries]), NULL));

		numentries = -1;
	}

	return 1;
}

static osbool mainwin_newmount(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);
	UNUSED(id_block);

	mainwin_setup(NULL);

	return 1;
}

static osbool mainwin_open(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	char tmp[11];

	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);
	UNUSED(id_block);

	E(xwritablefield_set_value(0, mainwinid, gadget_main_SERVER, mount.server));
	E(xwritablefield_set_value(0, mainwinid, gadget_main_EXPORT, mount.export));
	E(xwritablefield_set_value(0, mainwinid, gadget_main_USERNAME, mount.username));
	E(xwritablefield_set_value(0, mainwinid, gadget_main_PASSWORD, mount.password));
	snprintf(tmp, sizeof(tmp), "%d", mount.uid);
	E(xwritablefield_set_value(0, mainwinid, gadget_main_UID, mount.uid ? tmp : ""));
	E(xwritablefield_set_value(0, mainwinid, gadget_main_GIDS, mount.gids));
	snprintf(tmp, sizeof(tmp), "%.3o", mount.umask);
	E(xwritablefield_set_value(0, mainwinid, gadget_main_UMASK, tmp));
	if (mount.usepcnfsd) {
		E(xradiobutton_set_state(0, mainwinid, gadget_main_USEPCNFSD, TRUE));
	} else {
		E(xradiobutton_set_state(0, mainwinid, gadget_main_DONTUSEPCNFSD, TRUE));
	}

	E(xwritablefield_set_value(0, mainwinid, gadget_main_LEAFNAME, mount.leafname));

	pcnfsd_toggle(0, 0, 0, 0);

	return 1;
}

static osbool mainwin_set(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);
	char tmp[32];
	char filename[1024];

	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_SERVER, mount.server, STRMAX, NULL));
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_EXPORT, mount.export, STRMAX, NULL));
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_USERNAME, mount.username, STRMAX, NULL));
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_PASSWORD, mount.password, STRMAX, NULL));
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_UID, tmp, sizeof(tmp), NULL));
	mount.uid = atoi(tmp);
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_GIDS, mount.gids, STRMAX, NULL));
	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_UMASK, tmp, sizeof(tmp), NULL));
	mount.umask = (int)strtol(tmp, NULL, 8);
	E(xradiobutton_get_state(0, id_block->this_obj, gadget_main_USEPCNFSD, &mount.usepcnfsd, NULL));

	E(xwritablefield_get_value(0, id_block->this_obj, gadget_main_LEAFNAME, mount.leafname, STRMAX, NULL));
	if (mount.leafname[0]) {
		snprintf(filename, sizeof(filename), "<Sunfish$Write>.mounts.%s", mount.leafname);
		mount_save(filename);

		snprintf(filename, sizeof(filename), "Filer_OpenDir Sunfish:mounts.%s",mount.leafname);
		E(xwimp_start_task(filename, NULL));

		snprintf(filename, sizeof(filename), "Sunfish:mounts.%s",mount.leafname);
		add_mount(filename);
	} else {
		os_error err = {1,"Mount leafname must not be blank"};
		xwimp_report_error(&err, 0, "Sunfish", NULL);
		return 0;
	}

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
	if (strcmp(event->data.created.name,"mainwin") == 0) {
		mainwinid = id_block->this_obj;
	}
	if (strcmp(event->data.created.name,"editmounts") == 0) {
		editmenuid = id_block->this_obj;
	}
	if (strcmp(event->data.created.name,"mainmenu") == 0) {
		mainmenuid = id_block->this_obj;
	}
	if (strcmp(event->data.created.name,"ProgInfo") == 0) {
		E(xproginfo_set_version(0, id_block->this_obj, Module_VersionString " (" Module_Date ")"));
	}

	return 1;
}

static osbool message_quit(wimp_message *message,void *handle)
{
	UNUSED(message);
	UNUSED(handle);

	exit(EXIT_SUCCESS);
	return 1;
}

static osbool message_data_open(wimp_message *message, void *handle)
{
	UNUSED(handle);
	bits filetype = message->data.data_xfer.file_type;
	bits load;

	if (filetype == 0x1000) {
		if (xosfile_read_no_path(message->data.data_xfer.file_name, NULL, &load, NULL, NULL, NULL) == NULL) {
			if ((load & 0xFFF00000) == 0xFFF00000) filetype = (load & 0x000FFF00) >> 8;
		}
	}
	if (filetype == SUNFISH_FILETYPE) {
		add_mount(message->data.data_xfer.file_name);
	}
	return 1;
}

static char *mountpathfromicon(wimp_i icon)
{
	struct mounticon *current = iconhead;

	while (current) {
		if (current->iconhandle == icon) return current->filename;
		current = current->next;
	}
	return NULL;
}

static osbool message_data_save(wimp_message *message, void *handle)
{
	UNUSED(handle);
	char leafname[STRMAX];
	char *defaultpath = getenv("Sunfish$DefaultPath");
	char *mountpath = mountpathfromicon(message->data.data_xfer.i);

	if (mountpath == NULL) return 0;

	snprintf(leafname, sizeof(leafname), "%s", message->data.data_xfer.file_name);

	snprintf(message->data.data_xfer.file_name, sizeof(message->data.data_xfer.file_name),
	         "%s%s%s.%s", mountpath, defaultpath ? "." : "", defaultpath ? defaultpath : "", leafname);
	message->size = ((message->data.data_xfer.file_name - (char *)message) + strlen(message->data.data_xfer.file_name) + 1 + 3) & ~3;
	message->your_ref = message->my_ref;
	message->action = message_DATA_SAVE_ACK;

	E(xwimp_send_message(wimp_USER_MESSAGE, message, message->sender));

	return 1;
}

static osbool message_data_load(wimp_message *message, void *handle)
{
	UNUSED(handle);
	char cmd[STRMAX];

	if (message->your_ref == 0) {
		char *defaultpath = getenv("Sunfish$DefaultPath");
		char *mountpath = mountpathfromicon(message->data.data_xfer.i);

		if (mountpath == NULL) return 0;

		snprintf((char *)message->data.reserved, sizeof(message->data.reserved),
		         "%s%s%s", mountpath, defaultpath ? "." : "", defaultpath ? defaultpath : "");
		message->size = (((char *)(message->data.reserved) - (char *)message) + strlen((char *)(message->data.reserved)) + 1 + 3) & ~3;
		message->your_ref = message->my_ref;
		message->action = message_FILER_DEVICE_DIR;
	
		E(xwimp_send_message(wimp_USER_MESSAGE, message, message->sender));

		snprintf(cmd, sizeof(cmd), "Filer_OpenDir %s", message->data.reserved);
		E(xwimp_start_task(cmd, NULL));
	} else {
		char *leaf;

		snprintf(cmd, sizeof(cmd), "Filer_OpenDir %s", message->data.data_xfer.file_name);

		leaf = strrchr(cmd, '.');
		if (leaf) *leaf = '\0';

		E(xwimp_start_task(cmd, NULL));

		message->your_ref = message->my_ref;
		message->action = message_DATA_LOAD_ACK;

		E(xwimp_send_message(wimp_USER_MESSAGE, message, message->sender));
	}

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
	event_register_toolbox_handler(event_ANY, event_HELP,  help, NULL);
	event_register_toolbox_handler(event_ANY, event_EDITMENU,  editmenu_create, NULL);
	event_register_toolbox_handler(event_ANY, action_MENU_SELECTION, editmenu_selection, NULL);
	event_register_toolbox_handler(event_ANY, event_main_SHOW, mainwin_open, NULL);
	event_register_toolbox_handler(event_ANY, event_NEWMOUNT, mainwin_newmount, NULL);
	event_register_toolbox_handler(event_ANY, event_main_HIDE, mainwin_close, NULL);
	event_register_toolbox_handler(event_ANY, event_main_SET,  mainwin_set, NULL);
	event_register_toolbox_handler(event_ANY, event_main_PCNFSD,  pcnfsd_toggle, NULL);

	event_register_toolbox_handler(event_ANY, event_SHOWMOUNTS,  mount_showall, NULL);
	event_register_toolbox_handler(event_ANY, event_REMOVEICON,  mount_remove, NULL);

	event_register_toolbox_handler(event_ANY, event_filenames_SHOW, filenames_open, NULL);
	event_register_toolbox_handler(event_ANY, event_filenames_SET,  filenames_set, NULL);

	event_register_toolbox_handler(event_ANY, event_connection_SHOW, connection_open, NULL);
	event_register_toolbox_handler(event_ANY, event_connection_SET,  connection_set, NULL);

	event_register_toolbox_handler(event_ANY, event_ports_SHOW, ports_open, NULL);
	event_register_toolbox_handler(event_ANY, event_ports_SET,  ports_set, NULL);

	event_register_toolbox_handler(event_ANY, action_OBJECT_AUTO_CREATED, autocreated, NULL);
	event_register_message_handler(message_QUIT, message_quit, NULL);

	event_register_message_handler(message_DATA_OPEN, message_data_open, NULL);
	event_register_message_handler(message_DATA_SAVE, message_data_save, NULL);
	event_register_message_handler(message_DATA_LOAD, message_data_load, NULL);

	E(xtoolbox_create_object(0, (toolbox_id)"unmounted", &unmountedicon));
	E(xtoolbox_show_object(0, unmountedicon, toolbox_POSITION_DEFAULT, NULL, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT));

	xosfile_create_dir("<Sunfish$Write>", 0);
	xosfile_create_dir("<Sunfish$Write>.mounts", 0);

	while (TRUE) event_poll(&event_code, &poll_block, 0);

}
