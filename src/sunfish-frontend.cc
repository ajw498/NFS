/*
	$Id$

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
#include "oslib/stringset.h"
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

#include "sunfishdefs.h"
#include "sunfish.h"

#define event_QUIT 0x103
#define event_MOUNT 0x105
#define event_SHOWMOUNTS 0x106
#define event_REMOVEICON 0x107
#define event_HELP 0x108
#define event_EDITMENU 0x109
#define event_NEWMOUNT 0x10a




#define gadget_menu_REMOVE                0x3

#include <string>
using namespace std;

#include <swis.h>

#define SYSLOGF_BUFSIZE 1024
#define Syslog_LogMessage 0x4C880

void syslogf(char *fmt)
{
	_swix(Syslog_LogMessage, _INR(0,2), "Wibble", fmt, 1);

}

#include <map>

class window
{
public:
	window(const char *name);
//	virtual ~window() {}//why?
	toolbox_o objectid;
	void show(void);
	void hide(void);
protected:
	virtual void abouttobeshown(void) { }
	virtual void abouttobehidden(void) { }
	virtual osbool customevent(bits event_code) { return FALSE; }
private:
	static osbool wautocreated(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle);
	static osbool wshow(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle);
	static osbool whide(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle);
	static osbool wevent(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle);
//	osbool event(bits event_code);
	const char *objectname;
};

/*void window::register_event(int event, eventfn fn)
{
	events[event] = fn;
} */

class gadget
{
public:
	gadget(window *parentwin, int id) { gadgetid = id; parent = parentwin; }
	void faded(void);
	void unfaded(void);
	void faded(osbool set);

private:
protected:
	int gadgetid;
	window *parent;
};

void gadget::faded(void)
{
	gadget_flags flags;
	xgadget_get_flags(0, parent->objectid, gadgetid, &flags);
	flags |= gadget_FADED;
	xgadget_set_flags(0, parent->objectid, gadgetid, flags);
}

void gadget::unfaded(void)
{
	gadget_flags flags;
	xgadget_get_flags(0, parent->objectid, gadgetid, &flags);
	flags &= ~gadget_FADED;
	xgadget_set_flags(0, parent->objectid, gadgetid, flags);
}

void gadget::faded(osbool set)
{
	gadget_flags flags;
	xgadget_get_flags(0, parent->objectid, gadgetid, &flags);
	if (set) flags |= gadget_FADED; else flags &= ~gadget_FADED;
	xgadget_set_flags(0, parent->objectid, gadgetid, flags);
}

class writablefield:
	public gadget
{
public:
	writablefield(window *parentwin, int id) : gadget(parentwin, id) { }
	void operator=(const char *value);
	void operator=(const int value);
	operator char*();
	operator int();
};

void writablefield::operator=(const char *value)
{
	/*Error if parentid not yet set */
	xwritablefield_set_value(0, parent->objectid, gadgetid, value);
}

void writablefield::operator=(const int value)
{
	char tmp[15];
	/*Error if parentid not yet set */
	snprintf(tmp, sizeof(tmp), "%d", value);
	xwritablefield_set_value(0, parent->objectid, gadgetid, tmp);
}

writablefield::operator char*()
{
	string str;
	static char buf[1024];
	xwritablefield_get_value(0, parent->objectid, gadgetid, buf, sizeof(buf), NULL);
	str = "Dummy";
	return buf;
}

writablefield::operator int()
{
	char buf[1024];
	xwritablefield_get_value(0, parent->objectid, gadgetid, buf, sizeof(buf), NULL);
	return atoi(buf);
}

class stringset:
	public gadget
{
public:
	stringset(window *parentwin, int id) : gadget(parentwin, id) { }
	void operator=(const char *value);
	void operator=(const int value);
	operator char*();
	operator int();
};

void stringset::operator=(const char *value)
{
	/*Error if parentid not yet set */
	xstringset_set_selected_string(0, parent->objectid, gadgetid, value);
}

void stringset::operator=(const int value)
{
	char tmp[15];
	/*Error if parentid not yet set */
	snprintf(tmp, sizeof(tmp), "%d", value);
	xstringset_set_selected_string(0, parent->objectid, gadgetid, tmp);
}

stringset::operator char*()
{
	static char buf[1024];
	xstringset_get_selected_string(0, parent->objectid, gadgetid, buf, sizeof(buf), NULL);
	return buf;
}

stringset::operator int()
{
	char buf[1024];
	xstringset_get_selected_string(0, parent->objectid, gadgetid, buf, sizeof(buf), NULL);
	return atoi(buf);
}


class numberrange:
	public gadget
{
public:
	numberrange(window *parentwin, int id) : gadget(parentwin, id) { }
	void operator=(const int value);
	operator int();
	void setupperbound(const int value);
};

void numberrange::operator=(const int value)
{
	xnumberrange_set_value(0, parent->objectid, gadgetid, value);
}

void numberrange::setupperbound(const int value)
{
	xnumberrange_set_bounds(numberrange_BOUND_UPPER, parent->objectid, gadgetid, 0, value, 0, 0);
}

numberrange::operator int()
{
	int value;
	xnumberrange_get_value(0, parent->objectid, gadgetid, &value);
	return value;
}

class radiobutton:
	public gadget
{
public:
	radiobutton(window *parentwin, int id) : gadget(parentwin, id) { }
	void operator=(const osbool value);
	operator osbool();
};

void radiobutton::operator=(const osbool value)
{
	/*Error if parentid not yet set */
//	char tmp[1024];
//	sprintf(tmp,"parent %x gadget %x str %s",parent->objectid, gadgetid, value);
//	syslogf("Wibble",1,tmp);
	xradiobutton_set_state(0, parent->objectid, gadgetid, value);
}

radiobutton::operator osbool()
{
	osbool value;
	xradiobutton_get_state(0, parent->objectid, gadgetid, &value, NULL);
	return value;
}

class optionbutton:
	public gadget
{
public:
	optionbutton(window *parentwin, int id) : gadget(parentwin, id) { }
	void operator=(const osbool value);
	operator osbool();
};

void optionbutton::operator=(const osbool value)
{
	/*Error if parentid not yet set */
	xoptionbutton_set_state(0, parent->objectid, gadgetid, value);
}

optionbutton::operator osbool()
{
	osbool value;
	xoptionbutton_get_state(0, parent->objectid, gadgetid, &value);
	return value;
}

window::window(const char *name)
{
	objectname = name;
	objectid = 0;

	/*ERROR*/event_register_toolbox_handler(event_ANY, action_OBJECT_AUTO_CREATED, wautocreated, this);
}

osbool window::wautocreated(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle)
{
	window *thiswin = static_cast<window*>(handle);

	if (strcmp(event->data.created.name,thiswin->objectname) == 0) {
		thiswin->objectid = id_block->this_obj;
		/*ERROR*/event_deregister_toolbox_handler(event_ANY, action_OBJECT_AUTO_CREATED, wautocreated, handle);

		/*ERROR*/event_register_toolbox_handler(id_block->this_obj, action_ANY, wevent, handle);

		/*ERROR*/event_register_toolbox_handler(id_block->this_obj, action_WINDOW_ABOUT_TO_BE_SHOWN, wshow, handle);
		/*ERROR*/event_register_toolbox_handler(id_block->this_obj, action_WINDOW_DIALOGUE_COMPLETED, whide, handle);
		return 0;//1; FIXME
	}

	return 0;
}

/*osbool window::event(bits event_code)
{
	if (events.find(event_code) != events.end()) {
		events[event_code]();
		return TRUE;
	}
	return FALSE;
} */

osbool window::wevent(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle)
{
	window *thiswin = static_cast<window*>(handle);

/*	if (thiswin->events.find(event_code) != thiswin->events.end()) {
		eventfn fn = thiswin->events[event_code];
		thiswin->*fn();
		return TRUE;
	}
	return FALSE;*/
	return thiswin->customevent(event_code);
}

osbool window::whide(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle)
{
	window *thiswin = static_cast<window*>(handle);

	thiswin->abouttobehidden();
	return TRUE;
}

void window::show(void)
{
	/**/xtoolbox_show_object(0, objectid, toolbox_POSITION_DEFAULT, NULL, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT);
}

void window::hide(void)
{
	/**/xtoolbox_hide_object(0, objectid);
}


///
///
///


#define STRMAX 256

struct mount {
	osbool showhidden;
	int followsymlinks;
	osbool casesensitive;
	osbool unixex;
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
	char exportname[STRMAX];
	char username[STRMAX];
	char password[STRMAX];
	int uid;
	char gids[STRMAX];
	int umask;
	osbool usepcnfsd;
	osbool tcp;
	osbool nfs3;
	char leafname[STRMAX];
	char encoding[STRMAX];
};

static struct mount mount;


#define UNUSED(x) ((void)x)

#define E(x) { \
	os_error *err=x;\
	if (err!=NULL) {\
		xwimp_report_error(err,0,"Sunfish",NULL);\
		exit(EXIT_FAILURE);\
	}\
}


//#include <typeinfo>

osbool window::wshow(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle)
{
//	struct bodge *thisbodge = static_cast<struct bodge*>(handle);
//	window *thiswin = thisbodge->winptr;
	window *thiswin = static_cast<window*>(handle);

//	if (typeid(*thiswin) *thiswin2 = dynamic_cast<typeid(*thiswin)>(thiswin)) {
		thiswin->abouttobeshown();
//	}
	return TRUE;
}






//static toolbox_o filenamesid;
//static toolbox_o portsid;
///static toolbox_o connectionid;
//static toolbox_o mainwinid;
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

	fprintf(file,"Protocol: NFS%s\n", mount.nfs3 ? "3" : "2");
	fprintf(file,"Server: %s\n",mount.server);
	fprintf(file,"Export: %s\n",mount.exportname);
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
	fprintf(file,"UnixEx: %d\n",mount.unixex);
	fprintf(file,"DefaultFiletype: %.3X\n",mount.defaultfiletype);
	fprintf(file,"AddExt: %d\n",mount.addext);
	fprintf(file,"unumask: %.3o\n",mount.unumask);
	if (mount.portmapperport) fprintf(file,"PortmapperPort: %d\n",mount.portmapperport);
	if (mount.nfsport) fprintf(file,"NFSPort: %d\n",mount.nfsport);
	if (mount.pcnfsdport) fprintf(file,"PCNFSDPort: %d\n",mount.pcnfsdport);
	if (mount.mountport) fprintf(file,"MountPort: %d\n",mount.mountport);
	if (mount.localportmin && mount.localportmin) fprintf(file,"LocalPort: %d %d\n",mount.localportmin,mount.localportmax);
	if (mount.machinename[0]) fprintf(file,"MachineName: %s\n",mount.machinename);
	if ((/*case*/strcmp(mount.encoding, "No conversion") != 0) && mount.encoding[0]) fprintf(file,"Encoding: %s\n", mount.encoding);

	fprintf(file,"MaxDataBuffer: %d\n",mount.maxdatabuffer);
	fprintf(file,"Pipelining: %d\n",mount.pipelining);
	fprintf(file,"Timeout: %d\n",mount.timeout);
	fprintf(file,"Retries: %d\n",mount.retries);
	fprintf(file,"Logging: %d\n",mount.logging);
	fclose(file);

	E(xosfile_set_type(filename, SUNFISH_FILETYPE));
}

#define CHECK(str) (strncmp(line,str,sizeof(str))==0)
/*case*/

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
			mount.nfs3 = /*case*/strcmp(val, "NFS3") == 0;
		} else if (CHECK("Server")) {
			strcpy(mount.server, val);
		} else if (CHECK("MachineName")) {
			strcpy(mount.machinename, val);
		} else if (CHECK("Encoding")) {
			strcpy(mount.encoding, val);
		} else if (CHECK("PortMapperPort")) {
			mount.portmapperport = (int)strtol(val, NULL, 10);
		} else if (CHECK("MountPort")) {
			mount.mountport = (int)strtol(val, NULL, 10);
		} else if (CHECK("NFSPort")) {
			mount.nfsport = (int)strtol(val, NULL, 10);
		} else if (CHECK("PCNFSDPort")) {
			mount.pcnfsdport = (int)strtol(val, NULL, 10);
		} else if (CHECK("Transport")) {
			mount.tcp = /*case*/strcmp(val, "tcp") == 0;
		} else if (CHECK("Export")) {
			strcpy(mount.exportname, val);
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
			if (mount.maxdatabuffer > MAXDATABUFFER_TCP_MAX) {
				mount.maxdatabuffer = MAXDATABUFFER_TCP_MAX;
			}
			mount.maxdatabuffer = (mount.maxdatabuffer + (MAXDATABUFFER_INCR - 1)) & ~(MAXDATABUFFER_INCR - 1);
		} else if (CHECK("FollowSymlinks")) {
			mount.followsymlinks = (int)strtol(val, NULL, 10);
		} else if (CHECK("CaseSensitive")) {
			mount.casesensitive = (int)strtol(val, NULL, 10);
		} else if (CHECK("UnixEx")) {
			mount.unixex = (int)strtol(val, NULL, 10);
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
	
	mountdetails = (mounticon *)malloc(sizeof(struct mounticon));
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

class filenameswin:
	public window
{
public:
	radiobutton showhiddenalways;
	radiobutton showhiddenroot;
	radiobutton showhiddennever;
	optionbutton casesensitive;
	stringset encoding;
	writablefield defaultfiletype;
	radiobutton addextalways;
	radiobutton addextneeded;
	radiobutton addextnever;
	optionbutton unixex;
	optionbutton followsymlinks;
	writablefield symlinklevels;
	writablefield unumask;

	filenameswin(const char *name) :
		window(name),

		showhiddenalways (this, 0x10),
		showhiddenroot   (this, 0x11),
		showhiddennever  (this, 0x12),
		casesensitive    (this, 0x3),
		encoding         (this, 0x24),
		defaultfiletype  (this, 0x1),
		addextalways     (this, 0xb),
		addextneeded     (this, 0xc),
		addextnever      (this, 0xd),
		unixex           (this, 0x14),
		followsymlinks   (this, 0x9),
		symlinklevels    (this, 0x4),
		unumask          (this, 0xe)
		{ }

	osbool customevent(bits event_code);

protected:
	void abouttobeshown(void);
};

void filenameswin::abouttobeshown()
{
	char tmp[4];

	showhiddenalways = mount.showhidden == 1;
	showhiddenroot = mount.showhidden == 2;
	showhiddennever = mount.showhidden == 3;
	followsymlinks = mount.followsymlinks > 0;
	symlinklevels = mount.followsymlinks;
	casesensitive = mount.casesensitive;
	unixex = mount.unixex;
	snprintf(tmp, sizeof(tmp), "%X", mount.defaultfiletype);
	defaultfiletype = tmp;
	addextalways = mount.addext == ALWAYS;
	addextneeded = mount.addext == NEEDED;
	addextnever  = mount.addext == NEVER;
	snprintf(tmp, sizeof(tmp), "%.3o", mount.unumask);
	unumask = tmp;
	encoding = mount.encoding;
}

osbool filenameswin::customevent(bits event_code)
{
	if (event_code == 0x301) {
		mount.showhidden = showhiddenalways ? 1 : (showhiddenroot ? 2 : 0);
		mount.followsymlinks = followsymlinks ? symlinklevels : 0;
		mount.casesensitive = casesensitive;
		mount.unixex = unixex;
		mount.defaultfiletype = (int)strtol(defaultfiletype, NULL, 16);
		mount.addext = addextalways ? ALWAYS : (addextneeded ? NEEDED : NEVER);
		mount.unumask = (int)strtol(unumask, NULL, 8);
		strcpy(mount.encoding, encoding);
		return TRUE;
	}

	return FALSE;
}

class portswin:
	public window
{
public:
	writablefield portmapper;
	writablefield nfsd;
	writablefield pcnfsd;
	writablefield mountd;
	writablefield localmin;
	writablefield localmax;
	writablefield machinename;

	portswin(const char *name) :
		window(name),

		portmapper (this, 0x2),
		nfsd       (this, 0x5),
		pcnfsd     (this, 0x7),
		mountd     (this, 0xf),
		localmin   (this, 0x9),
		localmax   (this, 0xd),
		machinename(this, 0x0)
		{ }

	osbool customevent(bits event_code);

protected:
	void abouttobeshown(void);
};

void portswin::abouttobeshown(void)
{
	if (mount.portmapperport) portmapper = mount.portmapperport; else portmapper = "";
	if (mount.nfsport) nfsd = mount.nfsport; else nfsd = "";
	if (mount.pcnfsdport) pcnfsd = mount.pcnfsdport; else pcnfsd = "";
	if (mount.mountport) mountd = mount.mountport; else mountd = "";
	if (mount.localportmin) localmin = mount.localportmin; else localmin = "";
	if (mount.localportmax) localmax = mount.localportmax; else localmax = "";
	machinename = mount.machinename;
}

osbool portswin::customevent(bits event_code)
{
	if (event_code == 0x201) {
		mount.portmapperport = portmapper;
		mount.nfsport = nfsd;
		mount.pcnfsdport = pcnfsd;
		mount.mountport = mountd;
		mount.localportmin = localmin;
		mount.localportmax = localmax;
		strcpy(mount.machinename, machinename);
		return TRUE;
	}

	return FALSE;
}

class connectionwin:
	public window
{
public:
	numberrange databuffer;
	optionbutton pipelining;
	writablefield timeout;
	writablefield retries;
	optionbutton logging;
	radiobutton tcp;
	radiobutton udp;
	radiobutton nfs3;
	radiobutton nfs2;

	connectionwin(const char *name) :
		window(name),

		databuffer (this, 0x8),
		pipelining (this, 0x2),
		timeout    (this, 0xc),
		retries    (this, 0xd),
		logging    (this, 0x5),
		tcp        (this, 0x12),
		udp        (this, 0x11),
		nfs3       (this, 0x13),
		nfs2       (this, 0x14)
		{ }

	osbool customevent(bits event_code);

protected:
	void abouttobeshown(void);
};

osbool connectionwin::customevent(bits event_code)
{
	switch (event_code) {
	case 0x401:
		/* Set */
		mount.maxdatabuffer = databuffer;
		mount.pipelining = pipelining;
		mount.timeout = timeout;
		mount.retries = retries;
		mount.logging = logging;
		mount.tcp = tcp;
		mount.nfs3 = nfs3;
		return TRUE;
	case 0x402:
		/* radio button clicked */
		if (udp || nfs2) {
			if (databuffer > MAXDATABUFFER_UDP_MAX) databuffer = MAXDATABUFFER_UDP_MAX;
			databuffer.setupperbound(MAXDATABUFFER_UDP_MAX); 
		} else {
			databuffer.setupperbound(MAXDATABUFFER_TCP_MAX);
		}
		retries.faded(tcp); //FIXME Remove?
		if (tcp) retries = "1";
		return TRUE;
	}

	return FALSE;
}

void connectionwin::abouttobeshown(void)
{
	databuffer = mount.maxdatabuffer;
	pipelining = mount.pipelining;
	timeout = mount.timeout;
	retries = mount.retries;
	logging = mount.logging;
	tcp = mount.tcp;
	udp = !mount.tcp;
	nfs2 = !mount.nfs3;
	nfs3 = mount.nfs3;

	customevent(0x402);
}

static void mainwin_setup(char *mountname)
{
	mount.showhidden = 1;
	mount.followsymlinks = 5;
	mount.casesensitive = 0;
	mount.unixex = 0;
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
	mount.exportname[0] = '\0';
	mount.username[0] = '\0';
	mount.password[0] = '\0';
	mount.uid = 0;
	mount.gids[0] = '\0';
	mount.umask = 022;
	mount.usepcnfsd = 0;
	mount.tcp = 0;
	mount.nfs3 = 0;
	mount.leafname[0] = '\0';
	strcpy(mount.encoding, "No conversion");

	if (mountname) {
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "Sunfish:mounts.%s", mountname);
		mount_load(buffer);
	}

}

#define MAX_MOUNTS 100

static int numentries = -1;
static struct menu_entry_object menuentry[MAX_MOUNTS];

/* Create a menu of all mount files */
static osbool editmenu_create(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	int start = 0;
	int read;
	char buffer[256];
	int i;

	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	/* Remove all entries from the menu */
	if (numentries == -1) {
		E(xmenu_remove_entry(0, editmenuid, 0));
	} else {
		for (i = 0; i < numentries; i++) {
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

/*	if (strcmp(event->data.created.name,"filenames") == 0) {
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
	} */
	if (strcmp(event->data.created.name,"editmounts") == 0) {
		editmenuid = id_block->this_obj;
	}
	if (strcmp(event->data.created.name,"mainmenu") == 0) {
		mainmenuid = id_block->this_obj;
	}
	if (strcmp(event->data.created.name,"ProgInfo") == 0) {
		E(xproginfo_set_version(0, id_block->this_obj, Module_VersionString " (" Module_Date ")"));
	}

	return 0;
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

class mainwin:
	public window
{
public:
	writablefield server;
	writablefield exportname;
	writablefield username;
	writablefield password;
	writablefield uid;
	writablefield gids;
	writablefield umask;
	radiobutton    pcnfsd;
	radiobutton    nopcnfsd;
	writablefield leafname;

	filenameswin filenames;
	portswin ports;
	connectionwin connection;

	mainwin(const char *name) :
		window(name),

		server    (this, 0x0),
		exportname(this, 0x1),
		username  (this, 0x2),
		password  (this, 0x3),
		uid       (this, 0x6),
		gids      (this, 0x7),
		umask     (this, 0xa),
		pcnfsd    (this, 0x4),
		nopcnfsd  (this, 0x5),
		leafname  (this, 0x15),

		filenames("filenames"),
		ports("ports"),
		connection("connection")
		{ }

	void pcnfsd_toggle(void);
	osbool customevent(bits event_code);

protected:
	void abouttobeshown(void);
	void abouttobehidden(void);
};

void mainwin::abouttobeshown(void)
{
	char tmp[15];

	server = mount.server;
	exportname = mount.exportname;
	username = mount.username;
	password = mount.password;
	uid = mount.uid;
	gids = mount.gids;
	snprintf(tmp, sizeof(tmp), "%.3o", mount.umask);
	umask = tmp;
	pcnfsd = mount.usepcnfsd;
	nopcnfsd = !mount.usepcnfsd;
	leafname = mount.leafname;

	pcnfsd_toggle();
}

void mainwin::pcnfsd_toggle(void)
{
	osbool usepcnfsd = pcnfsd;

	username.faded(!usepcnfsd);
	password.faded(!usepcnfsd);
	uid.faded(usepcnfsd);
	gids.faded(usepcnfsd);
	umask.faded(usepcnfsd);
}


osbool mainwin::customevent(bits event_code)
{
	switch (event_code) {
	case 0x101:
		char filename[1024];

		/* Buffer overflow? Change to std:string? */
		strcpy(mount.server, server);
		strcpy(mount.exportname, exportname);
		strcpy(mount.username, username);
		strcpy(mount.password, password);
		mount.uid = uid;
		strcpy(mount.gids, gids);
		mount.umask = (int)strtol(umask, NULL, 8);
		mount.usepcnfsd = pcnfsd;
		strcpy(mount.leafname, leafname);

		if (mount.leafname[0]) {
			strcpy(filename, "<Sunfish$Write>.mounts.");
			strcat(filename, mount.leafname);
			mount_save(filename);

			strcpy(filename, "Filer_OpenDir Sunfish:mounts.");
			strcat(filename, mount.leafname);
			E(xwimp_start_task(filename, NULL));

			strcpy(filename, "Sunfish:mounts.");
			strcpy(filename, mount.leafname);
			add_mount(filename);
		} else {
			os_error err = {1,"Mount leafname must not be blank"};
			xwimp_report_error(&err, 0, "Sunfish", NULL);
		}
		return TRUE;
	case 0x104:
		pcnfsd_toggle();
	}
	return FALSE;
}

void mainwin::abouttobehidden(void)
{
	filenames.hide();
	ports.hide();
	connection.hide();
}

class application
{
public:
	application(const char *dirname);
	void poll(void);
private:
};

application::application(const char *dirname)
{
	int toolbox_events[] = {0};
	int wimp_messages[] = {0};
	static messagetrans_control_block messages;
	static toolbox_block id_block;

	if (xtoolbox_initialise(0, 310, (wimp_message_list*)wimp_messages,
							(toolbox_action_list*)toolbox_events,
							dirname, &messages, &id_block, 0, 0, 0)
							!= NULL) exit(EXIT_FAILURE);
	event_initialise(&id_block);
	event_set_mask(1+256);
}

void application::poll(void)
{
	wimp_event_no event_code;
	static wimp_block poll_block;
	event_poll(&event_code, &poll_block, 0);
}

class sunfishapp : public application
{
public:
	mainwin main;

	sunfishapp(const char *dirname) :
		application(dirname),
		main("mainwin")
		{ }
private:
};

sunfishapp app("<Sunfish$Dir>");


static osbool editmenu_selection(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(handle);

	if (id_block->this_obj != editmenuid) return 0;
	if (id_block->this_cmp > numentries) return 0;

	mainwin_setup(menuentry[id_block->this_cmp].text);

//	E(xtoolbox_show_object(0, mainwinid, toolbox_POSITION_CENTRED, NULL, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT));
	app.main.show();

	return 1;
}

int main(void)
{

	event_register_toolbox_handler(event_ANY, event_QUIT, quit, NULL);
	event_register_toolbox_handler(event_ANY, event_HELP,  help, NULL);
	event_register_toolbox_handler(event_ANY, event_EDITMENU,  editmenu_create, NULL);
	event_register_toolbox_handler(event_ANY, action_MENU_SELECTION, editmenu_selection, NULL);
//	event_register_toolbox_handler(event_ANY, event_main_SHOW, mainwin_open, NULL);
	event_register_toolbox_handler(event_ANY, event_NEWMOUNT, mainwin_newmount, NULL);
//	event_register_toolbox_handler(event_ANY, event_main_HIDE, mainwin_close, NULL);

	event_register_toolbox_handler(event_ANY, event_SHOWMOUNTS,  mount_showall, NULL);
	event_register_toolbox_handler(event_ANY, event_REMOVEICON,  mount_remove, NULL);

	event_register_toolbox_handler(event_ANY, action_OBJECT_AUTO_CREATED, autocreated, NULL);
	event_register_message_handler(message_QUIT, message_quit, NULL);

	event_register_message_handler(message_DATA_OPEN, message_data_open, NULL);
	event_register_message_handler(message_DATA_SAVE, message_data_save, NULL);
	event_register_message_handler(message_DATA_LOAD, message_data_load, NULL);

	E(xtoolbox_create_object(0, (toolbox_id)"unmounted", &unmountedicon));
	E(xtoolbox_show_object(0, unmountedicon, toolbox_POSITION_DEFAULT, NULL, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT));

	E(xtoolbox_create_object(0, (toolbox_id)"mainmenu", &mainmenuid));

	xosfile_create_dir("<Sunfish$Write>", 0);
	xosfile_create_dir("<Sunfish$Write>.mounts", 0);

	while (TRUE) app.poll();

}
