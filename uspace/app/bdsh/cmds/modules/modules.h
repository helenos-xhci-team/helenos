/*
 * Copyright (c) 2008 Tim Post
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MODULES_H
#define MODULES_H

/* Each built in function has two files, one being an entry.h file which
 * prototypes the run/help entry functions, the other being a .def file
 * which fills the modules[] array according to the cmd_t structure
 * defined in cmds.h.
 *
 * To add or remove a module, just make a new directory in cmds/modules
 * for it and copy the 'show' example for basics, then include it here.
 * (or reverse the process to remove one)
 *
 * NOTE: See module_ aliases.h as well, this is where aliases (commands that
 * share an entry point with others) are indexed */

#include "config.h"

/* Prototypes for each module's entry (help/exec) points */

#include "help/entry.h"
#include "mkdir/entry.h"
#include "mkfile/entry.h"
#include "rm/entry.h"
#include "cat/entry.h"
#include "touch/entry.h"
#include "ls/entry.h"
#include "pwd/entry.h"
#include "sleep/entry.h"
#include "cp/entry.h"
#include "mv/entry.h"
#include "mount/entry.h"
#include "unmount/entry.h"
#include "kcon/entry.h"
#include "printf/entry.h"
#include "echo/entry.h"
#include "cmp/entry.h"

/* Each .def function fills the module_t struct with the individual name, entry
 * point, help entry point, etc. You can use config.h to control what modules
 * are loaded based on what libraries exist on the system. */

module_t modules[] = {
#include "help/help_def.h"
#include "mkdir/mkdir_def.h"
#include "mkfile/mkfile_def.h"
#include "rm/rm_def.h"
#include "cat/cat_def.h"
#include "touch/touch_def.h"
#include "ls/ls_def.h"
#include "pwd/pwd_def.h"
#include "sleep/sleep_def.h"
#include "cp/cp_def.h"
#include "mv/mv_def.h"
#include "mount/mount_def.h"
#include "unmount/unmount_def.h"
#include "kcon/kcon_def.h"
#include "printf/printf_def.h"
#include "echo/echo_def.h"
#include "cmp/cmp_def.h"

	{NULL, NULL, NULL, NULL}
};

#endif
