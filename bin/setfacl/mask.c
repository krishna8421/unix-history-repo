/*
 * Copyright (c) 2001 Chris D. Faulhaber
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE VOICES IN HIS HEAD BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include "setfacl.h"

/* set the appropriate mask the given ACL's */
int
set_acl_mask(acl_t prev_acl)
{
	acl_t acl;
	int i;

	/*
	 * ... if a mask entry is specified, then the permissions of the mask
	 * entry in the resulting ACL shall be set to the permissions in the
	 * specified ACL mask entry.
	 */
	if (have_mask)
		return 0;

	acl = acl_dup(prev_acl);
	if (!acl)
		err(EX_OSERR, "acl_dup() failed");

	if (!n_flag) {
		/*
		 * If no mask entry is specified and the -n option is not
		 * specified, then the permissions of the resulting ACL mask
		 * entry shall be set to the union of the permissions
		 * associated with all entries which belong to the file group
		 * class in the resulting ACL
		 */
		if (acl_calc_mask(&acl)) {
			warn("acl_calc_mask() failed");
			acl_free(acl);
			return -1;
		}
	} else {
		/*
		 * If no mask entry is specified and the -n option is
		 * specified, then the permissions of the resulting ACL
		 * mask entry shall remain unchanged ...
		 */
		for (i = 0; i < acl->acl_cnt; i++)
			if (acl->acl_entry[i].ae_tag == ACL_MASK) {
				acl_free(acl);
				return 0;
			}

		/*
		 * If no mask entry is specified, the -n option is specified,
		 * and no ACL mask entry exists in the ACL associated with the
		 * file, then write an error message to standard error and
		 * continue with the next file.
		 */
		warnx("warning: no mask entry");
		acl_free(acl);
		return 0;
	}

	*prev_acl = *acl;
	acl_free(acl);

	return 0;
}
