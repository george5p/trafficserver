/** @file

  A brief file description

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

/****************************************************************************

  ink_file.h

  File manipulation routines for libinktomi.a.

 ****************************************************************************/

#ifndef _ink_file_h_
#define	_ink_file_h_

#include <stdio.h>
#include <sys/types.h>

/*===========================================================================*

                            Function Prototypes

 *===========================================================================*/

#include <dirent.h>

// Cause ink_filepath_merge to fail if addpath is above rootpath
//
#define INK_FILEPATH_NOTABOVEROOT   0x01
// internal: Only meaningful with INK_FILEPATH_NOTABOVEROOT
#define INK_FILEPATH_SECUREROOTTEST 0x02
// Cause ink_filepath_merge to fail if addpath is above rootpath,
// even given a rootpath /foo/bar and an addpath ../bar/bash
//
#define INK_FILEPATH_SECUREROOT     0x03
// Fail ink_filepath_merge if the merged path is relative
#define INK_FILEPATH_NOTRELATIVE    0x04
// Fail ink_filepath_merge if the merged path is absolute
#define INK_FILEPATH_NOTABSOLUTE    0x08
// Return the file system's native path format (e.g. path delimiters
// of ':' on MacOS9, '\' on Win32, etc.)
#define INK_FILEPATH_NATIVE         0x10
// Resolve the true case of existing directories and file elements
// of addpath, (resolving any aliases on Win32) and append a proper
// trailing slash if a directory
//
#define INK_FILEPATH_TRUENAME       0x20

int ink_access_extension(char *base, char *ext, int amode);
int readdir_r(DIR * dirp, struct dirent *entry, struct dirent **pentry);
DIR *opendir(const char *path);
int closedir(DIR * d);

FILE *ink_fopen_extension(char *base, char *ext, char *mode);
FILE *ink_fopen(char *name, char *mode);
void ink_fclose(FILE * fp);
void ink_fseek(FILE * stream, long offset, int ptrname);
long ink_ftell(FILE * stream);
void ink_rewind(FILE * stream);
char *ink_fgets(char *s, int n, FILE * stream);
int  ink_fputln(FILE * stream, const char *s);
size_t ink_fread(void *ptr, size_t size, size_t nitems, FILE * stream);
size_t ink_fwrite(void *ptr, size_t size, size_t nitems, FILE * stream);
int ink_file_name_mtime(char *path, time_t * tp);
int ink_file_name_size(char *path, off_t * op);

/* these routines use fcntl arguments */

int ink_file_lock_raw(int fd, int cmd, int type, off_t offset, int whence, off_t len);
int ink_file_region_lock(int fd, int type, off_t start, off_t len);
int ink_file_region_trylock(int fd, int type, off_t start, off_t len);
int ink_file_lock(int fd, int type);
int ink_file_trylock(int fd, int type);

int ink_file_fd_readline(int fd, int bufsz, char *buf);
int ink_file_fd_writestring(int fd, const char *buf);
int ink_filepath_merge(char *buf, int bufsz, const char *rootpath,
                       const char *addpath, int flags);

#endif // _ink_file_h_
