/*
 * Copyright (C) 2005, Andrew Turner, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name(s) of the author(s) may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <assert.h>
#include <stdlib.h>

#include "pkg.h"
#include "pkg_private.h"

struct pkg_list *
pkg_list_add(struct pkg_list *list, struct pkg_object *obj)
{
	struct pkg_list *new;

	/*
	 * If this assert fails the object you are trying to insert
	 * dosn't have a free callback. You should fix that otherwise
	 * there will be a memory leak.
	 */
	assert(obj->free != NULL);
	new = malloc(sizeof(struct pkg_list));
	if (!new) {
		return NULL;
	}

	new->next = list;
	new->obj = obj;

	new->pkg_object.data = NULL;
	new->pkg_object.free = NULL;

	return new;
}

int
pkg_list_free(struct pkg_list *list)
{
	struct pkg_list *current;
	struct pkg_list *next;

	current = list;

	while (current != NULL) {
		next = current->next;

		if (current->obj)
			pkg_object_free(current->obj);
		free(current);
		current = next;
	}

	return PKG_OK;
}
