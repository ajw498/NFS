/*
	$Id: $

	Routines for ImageEntry_Func
*/


#include "rpc.h"


struct file_handle {
	struct conn_info *conn;
	char fhandle[FHSIZE];
};


os_error *func_newimage(unsigned int fileswitchhandle, 	struct conn_info **myhandle);

os_error *func_closeimage(struct conn_info *conn);

os_error *func_readdirinfo(int info, const char *dirname, void *buffer, int numobjs, int start, int buflen, struct conn_info *conn, int *objsread, int *continuepos);

os_error *open_file(char *filename, int access, struct conn_info *conn, int *file_info_word, int *internal_handle, int *extent);

os_error *close_file(struct file_handle *handle, int load, int exec);

os_error *get_bytes(struct file_handle *handle, char *buffer, unsigned len, unsigned offset);

os_error *put_bytes(struct file_handle *handle, char *buffer, unsigned len, unsigned offset);

os_error *file_readcatinfo(char *filename, struct conn_info *conn, int *objtype, int *load, int *exec, int *len, int *attr);

os_error *file_writecatinfo(char *filename, int load, int exec, int attr, struct conn_info *conn);

os_error *file_savefile(char *filename, int load, int exec, char *buffer, char *buffer_end, struct conn_info *conn, char **leafname);

os_error *file_delete(char *filename, struct conn_info *conn, int *objtype, int *load, int *exec, int *len, int *attr);

os_error *func_rename(char *oldfilename, char *newfilename, struct conn_info *conn, int *renamefailed);
