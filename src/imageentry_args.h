/*
	Routines for ImageEntry_Args

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


#include "imageentry_common.h"


ENTRYFUNCDECL(os_error *, args_zeropad, (struct file_handle *handle, unsigned int offset, unsigned int size));

ENTRYFUNCDECL(os_error *, args_writeextent, (struct file_handle *handle, unsigned int extent));

ENTRYFUNCDECL(os_error *, args_ensuresize, (struct file_handle *handle, unsigned int size, unsigned int *actualsize));

ENTRYFUNCDECL(os_error *, args_readdatestamp, (struct file_handle *handle, unsigned int *load, unsigned int *exec));

ENTRYFUNCDECL(os_error *, args_readallocatedsize, (struct file_handle *handle, unsigned int *size));
