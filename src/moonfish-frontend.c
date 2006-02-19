/*
	$Id$
	$URL$

	Frontend for creating exports


	Copyright (C) 2006 Alex Waugh
	
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
#include "oslib/stringset.h"
#include "oslib/numberrange.h"
#include "oslib/window.h"
#include "oslib/iconbar.h"
#include "oslib/proginfo.h"
#include "oslib/quit.h"
#include "oslib/scrolllist.h"

#include "oslib/osfile.h"
#include "oslib/osfscontrol.h"
#include "oslib/osgbpb.h"
#include "oslib/wimp.h"
#include "oslib/filer.h"

#include "moonfishdefs.h"
#include "moonfish.h"

#define event_QUIT 0x103
#define event_HELP 0x108

#define event_exports_ADD    0x200
#define event_exports_EDIT   0x201
#define event_exports_DELETE 0x202
#define event_exports_SAVE   0x203
#define event_exports_SHOW   0x204
#define event_exports_LIST   0x205
#define event_exports_HIDE   0x206

#define event_edit_SHOW 0x300
#define event_edit_SET  0x301
#define event_edit_HIDE 0x302

#define event_options_SHOW     0x400
#define event_options_SET      0x401

#define gadget_exports_ADD    0xD
#define gadget_exports_EDIT   0xE
#define gadget_exports_DELETE 0xF
#define gadget_exports_LIST   0xC
#define gadget_exports_AUTOLOAD 0x12

#define gadget_edit_DIR        0x17
#define gadget_edit_EXPORTNAME 0x1
#define gadget_edit_ACCESS     0x15

#define gadget_options_UMASK        0x14
#define gadget_options_UNUMASK      0x12
#define gadget_options_UDPSIZE      0x8
#define gadget_options_FAKEDIRTIMES 0x11
#define gadget_options_READONLY     0x2
#define gadget_options_UID          0xC
#define gadget_options_GID          0xD
#define gadget_options_IMAGEFS      0x5
#define gadget_options_FILETYPE     0x16
#define gadget_options_ADDEXTALWAYS 0x18
#define gadget_options_ADDEXTNEEDED 0x19
#define gadget_options_ADDEXTNEVER  0x1A

#define UNUSED(x) ((void)x)

#define E(x) do { \
	os_error *err=x;\
	if (err!=NULL) {\
		os_error err2;\
		err2 = *err;\
		snprintf(err2.errmess, 252, "%s (line %d)", err->errmess, __LINE__);\
		xwimp_report_error(&err2,0,"Moonfish",NULL);\
		exit(EXIT_FAILURE);\
	}\
} while (0)

#define U(x) do { \
	if ((x) == NULL) { \
		os_error err3 = {1, "Unable to allocate memory"}; \
		E(&err3); \
	} \
} while (0)

#define error(x) do { \
	os_error err = {1, ""}; \
	strcpy(err.errmess, x);\
	xwimp_report_error(&err, 0, "Moonfish", NULL);\
} while (0)

#define AUTOLOADPATH "<Boot$ToBeTasks>.Moonfish"

#define STRMAX 256

struct export {
	char directory[STRMAX];
	char exportname[STRMAX];
	char access[STRMAX];
	char uid[STRMAX];
	char gid[STRMAX];
	char umask[STRMAX];
	char unumask[STRMAX];
	osbool fakedirtimes;
	int udpsize;
	osbool readonly;
	osbool imagefs;
	char filetype[STRMAX];
	int addext;
};

#define MAX_EXPORTS 128

static struct export *exports[MAX_EXPORTS];

static int numexports = 0;
static int editingexport;

static toolbox_o exportsid;
static toolbox_o optionsid;
static toolbox_o editid;
static toolbox_o quitid;

static int unsaveddata = 0;
static int configureplugin = 0;


static struct export *create_export(void)
{
	struct export *export;

	U(export = malloc(sizeof(struct export)));

	export->fakedirtimes = FALSE;
	export->udpsize = 4096;
	export->readonly= FALSE;
	export->imagefs = FALSE;
	export->addext = 1;
	export->directory[0] = '\0';
	export->exportname[0] = '\0';
	strcpy(export->access, "*");
	export->uid[0] = '\0';
	export->gid[0] = '\0';
	export->umask[0] = '\0';
	export->unumask[0] = '\0';
	strcpy(export->filetype, "FFF");

	return export;
}

#define CHECK(str) (strncasecmp(opt,str,sizeof(str) - 1)==0)

static struct export *parse_line(char *line)
{
	char *basedir;
	char *exportname;
	char *host;
	struct export *export;

	while (isspace(*line)) line++;
	if (*line == '#' || *line == '\0') return NULL;
	basedir = line;
	while (*line && !isspace(*line)) line++;
	if (*line) *line++ = '\0';
	while (isspace(*line)) line++;
	exportname = line;
	while (*line && !isspace(*line)) line++;
	if (*line) *line++ = '\0';
	while (isspace(*line)) line++;
	host = line;
	while (*line && *line != '(') line++;
	if (*line) *line++ = '\0';

	export = create_export();

	strcpy(export->directory, basedir);
	strcpy(export->exportname, exportname);
	strcpy(export->access, host);

	while (*line) {
		char *opt;
		while (isspace(*line)) line++;
		opt = line;
		while (*line && *line != ')' && *line != ',') line++;
		if (*line) *line++ = '\0';
		if (CHECK("rw")) {
			export->readonly = 0;
		} else if (CHECK("ro")) {
			export->readonly = 1;
		} else if (CHECK("uid") && opt[3] == '=') {
			strcpy(export->uid, opt + 4);
		} else if (CHECK("gid") && opt[3] == '=') {
			strcpy(export->gid, opt + 4);
		} else if (CHECK("udpsize") && opt[7] == '=') {
			export->udpsize = atoi(opt + 8);
			if (export->udpsize > 8192) export->udpsize = 8192;
		} else if (CHECK("imagefs")) {
			export->imagefs = 1;
		} else if (CHECK("fakedirtimes")) {
			export->fakedirtimes = 1;
		} else if (CHECK("umask") && opt[5] == '=') {
			strcpy(export->umask, opt + 6);
		} else if (CHECK("unumask") && opt[7] == '=') {
			strcpy(export->unumask, opt + 8);
		} else if (CHECK("defaultfiletype") && opt[15] == '=') {
			strcpy(export->filetype, opt + 16);
		} else if (CHECK("addext") && opt[6] == '=') {
			export->addext = atoi(opt + 7);
		}
	}

	return export;
}

static void parse_exports_file(void)
{
	char line[2048];
	FILE *file;

	file = fopen(EXPORTSREAD, "r");
	if (file == NULL) return; /* File probably doesn't exist yet */

	while (numexports < MAX_EXPORTS && fgets(line, sizeof(line), file)) {
		struct export *export;

		export = parse_line(line);
		if (export) exports[numexports++] = export;
	}
	fclose(file);
}

static osbool exports_save(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	FILE *file;
	int i;

	xosfile_create_dir("<Choices$Write>.Moonfish", 0);

	file = fopen("<Choices$Write>.Moonfish.exports", "w");
	if (file == NULL) {
		xwimp_report_error((os_error*)_kernel_last_oserror(),0,"Moonfish",NULL);
		return 1;
	}

	fprintf(file,"# Exports file written by Moonfish " Module_VersionString " (" Module_Date ")\n\n");
	for (i = 0; i < numexports; i++) {
		fprintf(file,"%s %s %s(", exports[i]->directory, exports[i]->exportname, exports[i]->access);

		fprintf(file, "%s", exports[i]->readonly ? "ro" : "rw");
		if (exports[i]->uid[0]) fprintf(file, ",uid=%s", exports[i]->uid);
		if (exports[i]->gid[0]) fprintf(file, ",gid=%s", exports[i]->gid);
		fprintf(file, ",udpsize=%d", exports[i]->udpsize);
		if (exports[i]->imagefs) fprintf(file, ",imagefs");
		if (exports[i]->fakedirtimes) fprintf(file, ",fakedirtimes");
		if (exports[i]->umask[0]) fprintf(file, ",umask=%s", exports[i]->umask);
		if (exports[i]->unumask[0]) fprintf(file, ",unumask=%s", exports[i]->unumask);
		if (exports[i]->filetype[0]) fprintf(file, ",defaultfiletype=%s", exports[i]->filetype);
		fprintf(file, ",addext=%d", exports[i]->addext);
		fprintf(file, ")\n");
	}

	fclose(file);

	unsaveddata = 0;

	/* Reload the module so it rereads the new exports file */
	system("RMKill Moonfish");
	system("RMLoad <Moonfish$Dir>.Moonfish");

	if (configureplugin) {
		osbool autoload;
		fileswitch_object_type type;

		E(xoptionbutton_get_state(0, exportsid, gadget_exports_AUTOLOAD, &autoload));

		if (xosfile_read_no_path(AUTOLOADPATH, &type, NULL, NULL, NULL, NULL)) {
			type = fileswitch_NOT_FOUND;
		}
		if (autoload && type == fileswitch_NOT_FOUND) {
			/* Create */
			file = fopen(AUTOLOADPATH, "w");
			if (file == NULL) {
				xwimp_report_error((os_error*)_kernel_last_oserror(),0,"Moonfish",NULL);
				return 1;
			}
			fprintf(file, "|Auto created by Moonfish\nIFThere <Moonfish$Dir>.Moonfish RMLoad <Moonfish$Dir>.Moonfish\n");
			fclose(file);
			xosfile_set_type(AUTOLOADPATH, 0xFEB);
		} else if (!autoload && type == fileswitch_IS_FILE) {
			/* Remove */
			xosfile_delete(AUTOLOADPATH, &type, NULL, NULL, NULL, NULL);
		}

	}
	return 1;
}

static osbool help(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	E(xwimp_start_task("Filer_Run <Moonfish$Dir>.!Help", NULL));

	return 1;
}

static osbool options_show(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	E(xwritablefield_set_value(0, optionsid, gadget_options_UMASK,    exports[editingexport]->umask));
	E(xwritablefield_set_value(0, optionsid, gadget_options_UNUMASK,  exports[editingexport]->unumask));
	E(xwritablefield_set_value(0, optionsid, gadget_options_UID,      exports[editingexport]->uid));
	E(xwritablefield_set_value(0, optionsid, gadget_options_GID,      exports[editingexport]->gid));
	E(xwritablefield_set_value(0, optionsid, gadget_options_FILETYPE, exports[editingexport]->filetype));

	E(xoptionbutton_set_state(0, optionsid, gadget_options_FAKEDIRTIMES, exports[editingexport]->fakedirtimes));
	E(xoptionbutton_set_state(0, optionsid, gadget_options_READONLY,     exports[editingexport]->readonly));
	E(xoptionbutton_set_state(0, optionsid, gadget_options_IMAGEFS,      exports[editingexport]->imagefs));

	E(xnumberrange_set_value(0, optionsid, gadget_options_UDPSIZE, exports[editingexport]->udpsize));

	switch (exports[editingexport]->addext) {
		case 2:
			E(xradiobutton_set_state(0, optionsid, gadget_options_ADDEXTALWAYS, TRUE));
			break;
		case 1:
			E(xradiobutton_set_state(0, optionsid, gadget_options_ADDEXTNEEDED, TRUE));
			break;
		default:
			E(xradiobutton_set_state(0, optionsid, gadget_options_ADDEXTNEVER, TRUE));
			break;
	}

	return 1;
}

static osbool options_set(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	toolbox_c selected;

	E(xwritablefield_get_value(0, optionsid, gadget_options_UMASK,    exports[editingexport]->umask, STRMAX, NULL));
	E(xwritablefield_get_value(0, optionsid, gadget_options_UNUMASK,  exports[editingexport]->unumask, STRMAX, NULL));
	E(xwritablefield_get_value(0, optionsid, gadget_options_UID,      exports[editingexport]->uid, STRMAX, NULL));
	E(xwritablefield_get_value(0, optionsid, gadget_options_GID,      exports[editingexport]->gid, STRMAX, NULL));
	E(xwritablefield_get_value(0, optionsid, gadget_options_FILETYPE, exports[editingexport]->filetype, STRMAX, NULL));

	E(xoptionbutton_get_state(0, optionsid, gadget_options_FAKEDIRTIMES, &(exports[editingexport]->fakedirtimes)));
	E(xoptionbutton_get_state(0, optionsid, gadget_options_READONLY,     &(exports[editingexport]->readonly)));
	E(xoptionbutton_get_state(0, optionsid, gadget_options_IMAGEFS,      &(exports[editingexport]->imagefs)));

	E(xnumberrange_get_value(0, optionsid, gadget_options_UDPSIZE, &(exports[editingexport]->udpsize)));

	E(xradiobutton_get_state(0, optionsid, gadget_options_ADDEXTALWAYS, NULL, &selected));
	switch (selected) {
		case gadget_options_ADDEXTALWAYS:
			exports[editingexport]->addext = 2;
			break;
		case gadget_options_ADDEXTNEEDED:
			exports[editingexport]->addext = 1;
			break;
		default:
			exports[editingexport]->addext = 0;
			break;
	}

	return 1;
}

static osbool exports_add(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(id_block);
	UNUSED(event);
	UNUSED(handle);

	if (numexports >= MAX_EXPORTS) {
		error("Cannot support any more exports");
		return 1;
	}

	editingexport = numexports;
	exports[editingexport] = create_export();

	E(xtoolbox_show_object(0, editid, toolbox_POSITION_AT_POINTER, NULL, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT));

	return 1;
}

static osbool exports_edit(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	int offset;

	E(xscrolllist_get_selected(0, exportsid, gadget_exports_LIST, -1, &offset));
	if (offset >= 0 && offset < numexports) {
		editingexport = offset;
		E(xtoolbox_show_object(0, editid, toolbox_POSITION_AT_POINTER, NULL, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT));
	}

	return 1;
}

static osbool exports_delete(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	gadget_flags flags;
	int offset;

	E(xscrolllist_get_selected(0, exportsid, gadget_exports_LIST, -1, &offset));
	if (offset >= 0 && offset < numexports) {
		free(exports[offset]);
		memmove(exports + offset, exports + offset + 1, (numexports - 1) * sizeof (struct export *));
		numexports--;

		E(xscrolllist_delete_items(0, exportsid, gadget_exports_LIST, offset, offset));

		E(xgadget_get_flags(0, exportsid, gadget_exports_EDIT, &flags));
		E(xgadget_set_flags(0, exportsid, gadget_exports_EDIT, flags | gadget_FADED));

		E(xgadget_get_flags(0, exportsid, gadget_exports_DELETE, &flags));
		E(xgadget_set_flags(0, exportsid, gadget_exports_DELETE, flags | gadget_FADED));

		unsaveddata = 1;
	}

	return 1;
}

static osbool exports_list(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	gadget_flags flags;
	toolbox_info info;

	E(xtoolbox_get_object_info(0, editid, &info));
	if ((info & toolbox_INFO_SHOWING) == 0) {
		E(xgadget_get_flags(0, exportsid, gadget_exports_EDIT, &flags));
		E(xgadget_set_flags(0, exportsid, gadget_exports_EDIT, flags & ~gadget_FADED));

		E(xgadget_get_flags(0, exportsid, gadget_exports_DELETE, &flags));
		E(xgadget_set_flags(0, exportsid, gadget_exports_DELETE, flags & ~gadget_FADED));
	}

	return 1;
}

static osbool edit_show(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	gadget_flags flags;

	E(xwritablefield_set_value(0, editid, gadget_edit_DIR, exports[editingexport]->directory));
	E(xwritablefield_set_value(0, editid, gadget_edit_EXPORTNAME, exports[editingexport]->exportname));
	E(xstringset_set_selected_string(0, editid, gadget_edit_ACCESS, exports[editingexport]->access));

	/* Fade the buttons to prevent editing multiple exports at once */
	E(xgadget_get_flags(0, exportsid, gadget_exports_ADD, &flags));
	E(xgadget_set_flags(0, exportsid, gadget_exports_ADD, flags | gadget_FADED));

	E(xgadget_get_flags(0, exportsid, gadget_exports_EDIT, &flags));
	E(xgadget_set_flags(0, exportsid, gadget_exports_EDIT, flags | gadget_FADED));

	E(xgadget_get_flags(0, exportsid, gadget_exports_DELETE, &flags));
	E(xgadget_set_flags(0, exportsid, gadget_exports_DELETE, flags | gadget_FADED));

	return 1;
}

static osbool edit_hide(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	gadget_flags flags;
	int offset;

	E(xgadget_get_flags(0, exportsid, gadget_exports_ADD, &flags));
	E(xgadget_set_flags(0, exportsid, gadget_exports_ADD, flags & ~gadget_FADED));

	E(xscrolllist_get_selected(0, exportsid, gadget_exports_LIST, -1, &offset));
	if (offset != -1) {
		E(xgadget_get_flags(0, exportsid, gadget_exports_EDIT, &flags));
		E(xgadget_set_flags(0, exportsid, gadget_exports_EDIT, flags & ~gadget_FADED));

		E(xgadget_get_flags(0, exportsid, gadget_exports_DELETE, &flags));
		E(xgadget_set_flags(0, exportsid, gadget_exports_DELETE, flags & ~gadget_FADED));
	}

	E(xtoolbox_hide_object(0, optionsid));

	return 1;
}

static osbool edit_set(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	char dir[STRMAX];
	char name[STRMAX];
	char access[STRMAX];
	fileswitch_object_type type;
	os_error *err;

	E(xwritablefield_get_value(0, editid, gadget_edit_DIR, dir, STRMAX, NULL));
	E(xwritablefield_get_value(0, editid, gadget_edit_EXPORTNAME, name, STRMAX, NULL));
	E(xstringset_get_selected_string(0, editid, gadget_edit_ACCESS, access, STRMAX, NULL));

	err = xosfile_read_no_path(dir, &type, NULL, NULL, NULL, NULL);
	if (err || type == fileswitch_NOT_FOUND || type == fileswitch_IS_FILE) {
		error("Directory field must contain a valid directory path");
		return 1;
	}

	if (name[0] == '\0') {
		error("Export name field must not be empty");
		return 1;
	}

	if (access[0] == '\0') {
		error("Allowed hosts field must not be empty");
		return 1;
	}

	if ((strchr(access, '*') && strlen(access) != 1)) {
		error("Allowed hosts field is invalid");
		return 1;
	}

	strcpy(exports[editingexport]->directory, dir);
	strcpy(exports[editingexport]->exportname, name);
	strcpy(exports[editingexport]->access, access);

	if (editingexport < numexports) {
		E(xscrolllist_delete_items(0, exportsid, gadget_exports_LIST, editingexport, editingexport));
	} else {
		numexports++;
	}

	E(xscrolllist_add_item(0, exportsid, gadget_exports_LIST, exports[editingexport]->directory, NULL, NULL, editingexport));

	E(xtoolbox_hide_object(0, editid));

	unsaveddata = 1;

	return 1;
}

static osbool exports_show(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	gadget_flags flags;
	int i;
	toolbox_info info;

	E(xtoolbox_get_object_info(0, exportsid, &info));
	if ((info & toolbox_INFO_SHOWING) == 0) {

		E(xscrolllist_delete_items(0, exportsid, gadget_exports_LIST, 0, MAX_EXPORTS));

		if (configureplugin) {
			fileswitch_object_type type;

			E(xgadget_get_flags(0, exportsid, gadget_exports_AUTOLOAD, &flags));
			E(xgadget_set_flags(0, exportsid, gadget_exports_AUTOLOAD, flags & ~gadget_FADED));

			if ((xosfile_read_no_path(AUTOLOADPATH, &type, NULL, NULL, NULL, NULL) == NULL) && (type == fileswitch_IS_FILE)) {
				E(xoptionbutton_set_state(0, exportsid, gadget_exports_AUTOLOAD, 1));
			}

		}

		E(xgadget_get_flags(0, exportsid, gadget_exports_ADD, &flags));
		E(xgadget_set_flags(0, exportsid, gadget_exports_ADD, flags & ~gadget_FADED));

		E(xgadget_get_flags(0, exportsid, gadget_exports_EDIT, &flags));
		E(xgadget_set_flags(0, exportsid, gadget_exports_EDIT, flags | gadget_FADED));

		E(xgadget_get_flags(0, exportsid, gadget_exports_DELETE, &flags));
		E(xgadget_set_flags(0, exportsid, gadget_exports_DELETE, flags | gadget_FADED));

		parse_exports_file();
		unsaveddata = 0;

		for (i = 0; i < numexports; i++) {
			E(xscrolllist_add_item(0, exportsid, gadget_exports_LIST, exports[i]->directory, NULL, NULL, i));
		}
	}

	return 1;
}

static osbool exports_hide(bits event_code, toolbox_action *event, toolbox_block *id_block, void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	int i;

	E(xtoolbox_hide_object(0, editid));

	for (i = 0; i < numexports; i++) {
		free(exports[i]);
	}
	numexports = 0;

	if (configureplugin) exit(EXIT_SUCCESS);

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

	if (strcmp(event->data.created.name,"editexport") == 0) {
		editid = id_block->this_obj;
	} else if (strcmp(event->data.created.name,"options") == 0) {
		optionsid = id_block->this_obj;
	} else if (strcmp(event->data.created.name,"ProgInfo") == 0) {
		E(xproginfo_set_version(0, id_block->this_obj, Module_VersionString " (" Module_Date ")"));
	} else if (strcmp(event->data.created.name,"Quit") == 0) {
		quitid = id_block->this_obj;
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

static osbool message_data_load(wimp_message *message, void *handle)
{
	UNUSED(handle);

	wimp_w window;

	E(xwindow_get_wimp_handle(0, editid, &window));

	if (message->data.data_xfer.w == window) {
		char exportname[STRMAX];

		E(xwritablefield_set_value(0, editid, gadget_edit_DIR, message->data.data_xfer.file_name));
		E(xwritablefield_get_value(0, editid, gadget_edit_EXPORTNAME, exportname, STRMAX, NULL));
		if (exportname[0] == '\0') {
			char *dot;
			dot = strrchr(message->data.data_xfer.file_name, '.');
			if (dot) {
				dot++;
				E(xwritablefield_set_value(0, editid, gadget_edit_EXPORTNAME, dot));
			}
		}

		message->your_ref = message->my_ref;
		message->action = message_DATA_LOAD_ACK;

		E(xwimp_send_message(wimp_USER_MESSAGE, message, message->sender));
	}

	return 1;
}

static int plugindescriptor;
static wimp_t quitsender;

static osbool message_plugin_quit(wimp_message *message, void *handle)
{
	UNUSED(handle);

	toolbox_info info;

	E(xtoolbox_get_object_info(0, editid, &info));
	if (configureplugin && (unsaveddata || ((info & toolbox_INFO_SHOWING) == 1))) {
		plugindescriptor = message->data.reserved[0];
		quitsender = message->sender;
		message->your_ref = message->my_ref;
		E(xwimp_send_message(wimp_USER_MESSAGE_ACKNOWLEDGE, message, message->sender));

		/* Open quit dialogue */
		E(xtoolbox_show_object(0, quitid, toolbox_POSITION_CENTRED, NULL, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT));
	}

	return 1;
}


static osbool quit_cancelled(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	if (configureplugin) {
		wimp_message message;

		message.your_ref = message.my_ref = 0;
		message.size = 24;
		message.action = 0x50D82;
		message.data.reserved[0] = plugindescriptor;
		E(xwimp_send_message(wimp_USER_MESSAGE, &message, 0));
	}

	return 1;
}

static osbool quit_quit(bits event_code, toolbox_action *event, toolbox_block *id_block,void *handle)
{
	UNUSED(event_code);
	UNUSED(event);
	UNUSED(id_block);
	UNUSED(handle);

	if (configureplugin) {
		wimp_message message;

		message.your_ref = message.my_ref = 0;
		message.size = 24;
		message.action = 0x50D81;
		message.data.reserved[0] = plugindescriptor;
		E(xwimp_send_message(wimp_USER_MESSAGE, &message, quitsender));
	}

	exit(EXIT_SUCCESS);

	return 1;
}

static osbool message_plugin_openwindow(wimp_message *message, void *handle)
{
	UNUSED(handle);
	UNUSED(message);

	E(xtoolbox_show_object(0, exportsid, toolbox_POSITION_DEFAULT, NULL, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT));

	return 1;
}

int main(int argc, char *argv[])
{
	int toolbox_events[] = {0};
	int wimp_messages[] = {0};
	wimp_event_no event_code;
	wimp_block poll_block;
	messagetrans_control_block messages;
	toolbox_block id_block;
	toolbox_o unmountedicon;

	E(xtoolbox_initialise(0, 310, (wimp_message_list*)wimp_messages,
							(toolbox_action_list*)toolbox_events,
							"<Moonfish$Dir>", &messages, &id_block, 0, 0, 0));
	event_initialise(&id_block);
	event_set_mask(1+256);

	event_register_toolbox_handler(event_ANY, event_QUIT, quit, NULL);
	event_register_toolbox_handler(event_ANY, event_HELP,  help, NULL);

	event_register_toolbox_handler(event_ANY, event_exports_ADD, exports_add, NULL);
	event_register_toolbox_handler(event_ANY, event_exports_EDIT, exports_edit, NULL);
	event_register_toolbox_handler(event_ANY, event_exports_LIST, exports_list, NULL);
	event_register_toolbox_handler(event_ANY, event_exports_DELETE, exports_delete, NULL);
	event_register_toolbox_handler(event_ANY, event_exports_SHOW, exports_show, NULL);
	event_register_toolbox_handler(event_ANY, event_exports_HIDE, exports_hide, NULL);
	event_register_toolbox_handler(event_ANY, event_exports_SAVE, exports_save, NULL);

	event_register_toolbox_handler(event_ANY, event_edit_SHOW, edit_show, NULL);
	event_register_toolbox_handler(event_ANY, event_edit_HIDE, edit_hide, NULL);
	event_register_toolbox_handler(event_ANY, event_edit_SET, edit_set, NULL);

	event_register_toolbox_handler(event_ANY, event_options_SHOW, options_show, NULL);
	event_register_toolbox_handler(event_ANY, event_options_SET, options_set, NULL);

	event_register_toolbox_handler(event_ANY, action_QUIT_QUIT, quit_quit, NULL);
	event_register_toolbox_handler(event_ANY, action_QUIT_DIALOGUE_COMPLETED, quit_cancelled, NULL);

	event_register_toolbox_handler(event_ANY, action_OBJECT_AUTO_CREATED, autocreated, NULL);
	event_register_message_handler(message_QUIT, message_quit, NULL);
	event_register_message_handler(message_DATA_LOAD, message_data_load, NULL);
	event_register_message_handler(0x50D80, message_plugin_quit, NULL);
	event_register_message_handler(0x50D83, message_plugin_openwindow, NULL);

	E(xtoolbox_create_object(0, (toolbox_id)"exports", &exportsid));

	if ((argc == 4) && (strcmp(argv[1], "-openat") == 0)) {
		/* Opened as a configure plugin */
		toolbox_position pos;

		configureplugin = 1;
		pos.top_left.x = atoi(argv[2]);
		pos.top_left.y = atoi(argv[3]);

		E(xtoolbox_show_object(0, exportsid, toolbox_POSITION_TOP_LEFT, &pos, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT));
	} else {
		/* Not a configure plugin, so create an iconbar icon */
		E(xtoolbox_create_object(0, (toolbox_id)"iconbar", &unmountedicon));
		E(xtoolbox_show_object(0, unmountedicon, toolbox_POSITION_DEFAULT, NULL, toolbox_NULL_OBJECT, toolbox_NULL_COMPONENT));
	}

	while (TRUE) event_poll(&event_code, &poll_block, 0);

}
