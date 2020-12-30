/*
	Dialogue box for getting the UID/GID or username/password


	Copyright (C) 2006 Alex Waugh

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

#include "getuid.h"
#include "sunfish-frontend.h"


using namespace std;


getuid::getuid()
{
	title("Enter identification");
	close_icon(false);

	uid.text("",30).xbaseline(xbaseline_left);
	uid.min_size(point(300,0));
	gid.text("",30).xbaseline(xbaseline_left);
	cancel.text("Cancel");
	set.text("Set");
	save.text("Save");
	explain1.text("Some systems may not require specific ID");
	explain2.text("values, in which case leave them blank");
	uidlabel.xbaseline(xbaseline_right);
	uidlabel.xfit(false);
	gidlabel.xbaseline(xbaseline_right);
	gidlabel.xfit(false);
	layout1.margin(16).ygap(8);
	layout1.add(layout2);
	layout1.add(layout4);
	layout4.add(explain1);
	layout4.add(explain2);
	layout1.add(layout3);
	layout2.ygap(8);
	layout2.add(uidlabel,0,0);
	layout2.add(gidlabel,0,1);
	layout2.add(uid,1,0);
	layout2.add(gid,1,1);
	layout3.xgap(16);
	layout3.add(cancel);
	layout3.add(set);
	layout3.add(save);
	add(layout1);
}

void getuid::setup(const hostinfo& info, string name, bool tcp, int version, sunfish& app)
{
	mountchoices mountdetails;

	// Open window near mouse position.
	os::pointer_info_get blk;
	os::Wimp_GetPointerInfo(blk);
	if (blk.p.x() + min_bbox().xsize() + 4 > app.bbox().xmax()) {
		blk.p.x(app.bbox().xmax() - (min_bbox().xsize() + 4));
	} else {
		blk.p -= point(64,0);
	}
	if (blk.p.y() - min_bbox().ysize() < 0) {
		blk.p.y(min_bbox().ysize());
	}

	host = info;
	exportname = name;
	usetcp = tcp;
	nfsversion = version;
	string filename = mountdetails.genfilename(host.host, host.ip, exportname);
	mountdetails.load(filename);
	mountdetails.server = string(host.host);
	mountdetails.exportname = exportname;
	mountdetails.tcp = tcp;
	mountdetails.nfs3 = version == 3;
	if (mountdetails.uidvalid ||
	    mountdetails.username[0] ||
	    (info.mount1udpport == 111) ||
	    (info.mount1tcpport == 111) ||
	    (info.mount3udpport == 111) ||
	    (info.mount3tcpport == 111)) {
		// Assume that any server with the mount protocol on port 111
		// is a RISC OS machine, and therefore doesn't need a valid
		// uid/gid. If this assumption is wrong, then the user can
		// always set the uid/gid from the export choices.
		create_mount(mountdetails, app);
	} else if (info.pcnfsdudpport || info.pcnfsdtcpport) {
		uidlabel.text("Username");
		gidlabel.text("Password");
		uid.validation("Ktar;Pptr_write");
		gid.validation("D*;Ktar;Pptr_write");
		layout4.remove();
		app.add(*this,blk.p);
		uid.set_caret_position(point(),-1,0);
	} else {
		uidlabel.text("User ID");
		gidlabel.text("Group ID");
		uid.validation("A0-9;Ktar;Pptr_write");
		gid.validation("A0-9 ;Ktar;Pptr_write");
		layout1.add(layout4,1);
		app.add(*this,blk.p);
		uid.set_caret_position(point(),-1,0);
	}
	uid.text("");
	gid.text("");
}

void getuid::use(bool save)
{
	mountchoices mountdetails;
	string filename = mountdetails.genfilename(host.host, host.ip, exportname);
	mountdetails.load(filename);
	mountdetails.server = string(host.host);
	mountdetails.exportname = exportname;
	mountdetails.tcp = usetcp;
	mountdetails.nfs3 = nfsversion == 3;

	if (host.pcnfsdudpport || host.pcnfsdtcpport) {
		char *err;
		bool tcp = host.pcnfsdudpport == 0;
		int port = tcp ? host.pcnfsdtcpport : host.pcnfsdudpport;
		char gidstr[256];

		err = browse_lookuppassword(host.host, port, tcp, uid.text().c_str(), gid.text().c_str(), &mountdetails.uid, &mountdetails.umask, gidstr, sizeof(gidstr));
		if (err) throw err;
		mountdetails.gids = string(gidstr);
	} else {
		mountdetails.uid = atoi(uid.text().c_str());
		mountdetails.gids = gid.text();
	}
	mountdetails.uidvalid = true;
	if (save) mountdetails.save(filename);
	create_mount(mountdetails, *static_cast<sunfish *>(parent_application()));
}

void getuid::handle_event(events::mouse_click& ev)
{
	if (ev.buttons() == 2) {
	} else if (ev.target() == &save) {
		use(true);
		if (ev.buttons() == 4) remove();
	} else if (ev.target() == &set) {
		use(false);
		if (ev.buttons() == 4) remove();
	} else if (ev.target() == &cancel) {
		uid.text("");
		gid.text("");
		if (ev.buttons() == 4) remove();
	}
}

void getuid::handle_event(events::key_pressed& ev)
{
	if (ev.code() == 13) {
		// Return
		use(true);
		remove();
	} else if (ev.code() == 27) {
		// Escape
		remove();
	}
}

void getuid::create_mount(mountchoices& mountdetails, sunfish& app)
{
	string sf;
	string hostalias = app.hostaliases.getalias(mountdetails.server, mountdetails.exportname);

	if (hostalias == "") {
		hostalias = mountdetails.server + mountdetails.exportname;
		// Remove illegal characters for discnames
		int len = hostalias.length();
		for (int i = 0; i < len; i++) {
			switch (hostalias[i]) {
				case '.':
				hostalias[i] = '_';
				break;
				case ':':
				case '*':
				case ' ':
				case '#':
				case '$':
				case '&':
				case '@':
				case '^':
				case '%':
				case '\\':
				hostalias[i] = '?';
				break;
			}
		}
	}

	bool found;
	ibicon *icon = app.add_mounticon(hostalias, sf, mountdetails.tcp, mountdetails.nfs3 ? 3 : 2, found);
	try {
		icon->mount(mountdetails.stringsave().c_str());
		icon->opendir();
	} catch(...) {
		app.remove_mounticon(icon);
		throw;
	}
}

