/*
	$Id$

	Routines for ImageEntry_Func
*/


#include "rpc.h"


struct file_handle {
	struct conn_info *conn;
	char fhandle[FHSIZE];
	unsigned int extent;
	unsigned int load;
	unsigned int exec;
};


os_error *func_newimage(unsigned int fileswitchhandle, 	struct conn_info **myhandle);

os_error *func_closeimage(struct conn_info *conn);

os_error *func_readdirinfo(int info, char *dirname, void *buffer, int numobjs, int start, int buflen, struct conn_info *conn, int *objsread, int *continuepos);

os_error *open_file(char *filename, int access, struct conn_info *conn, unsigned int *file_info_word, struct file_handle **internal_handle, unsigned int *fileswitchbuffersize, unsigned int *extent, unsigned int *allocatedspace);

os_error *close_file(struct file_handle *handle, unsigned int load, unsigned int exec);

os_error *get_bytes(struct file_handle *handle, char *buffer, unsigned len, unsigned offset);

os_error *put_bytes(struct file_handle *handle, char *buffer, unsigned len, unsigned offset);

os_error *file_readcatinfo(char *filename, struct conn_info *conn, int *objtype, unsigned int *load, unsigned int *exec, int *len, int *attr);

os_error *file_writecatinfo(char *filename, unsigned int load, unsigned int exec, int attr, struct conn_info *conn);

os_error *file_savefile(char *filename, unsigned int load, unsigned int exec, char *buffer, char *buffer_end, struct conn_info *conn, char **leafname);

os_error *file_delete(char *filename, struct conn_info *conn, int *objtype, unsigned int *load, unsigned int *exec, int *len, int *attr);

os_error *func_rename(char *oldfilename, char *newfilename, struct conn_info *conn, int *renamefailed);

os_error *file_createfile(char *filename, unsigned int load, unsigned int exec, char *buffer, char *buffer_end, struct conn_info *conn);

os_error *file_createdir(char *filename, unsigned int load, unsigned int exec, struct conn_info *conn);

os_error *args_zeropad(struct file_handle *handle, unsigned int offset, unsigned int size);

os_error *args_writeextent(struct file_handle *handle, unsigned int extent);

os_error *args_ensuresize(struct file_handle *handle, unsigned int size, unsigned int *actualsize);

os_error *args_readdatestamp(struct file_handle *handle, unsigned int *load, unsigned int *exec);

os_error *args_readallocatedsize(struct file_handle *handle, unsigned int *size);

os_error *file_readblocksize(unsigned int *size);

