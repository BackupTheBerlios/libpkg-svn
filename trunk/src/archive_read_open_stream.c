/*-
 * Copyright (c) 2003-2004 Tim Kientzle
 * Copyright (c) 2005 Andrew Turner
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

//#include "archive_platform.h"
//__FBSDID("$FreeBSD$");

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "archive.h"

struct read_stream_data {
	FILE	*fd;
	size_t	 block_size;
	void	*buffer;
};

int archive_read_open_stream(struct archive *, FILE *, size_t);

static int	stream_close(struct archive *, void *);
static int	stream_open(struct archive *, void *);
static ssize_t	stream_read(struct archive *, void *, const void **buff);

int
archive_read_open_stream(struct archive *a, FILE *fd, size_t block_size)
{
	struct read_stream_data *mine;

	mine = malloc(sizeof(*mine));
	if (mine == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		return (ARCHIVE_FATAL);
	}
	mine->block_size = block_size;
	mine->buffer = malloc(mine->block_size);
	if (mine->buffer == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		free(mine);
		return (ARCHIVE_FATAL);
	}
	mine->fd = fd;
	return (archive_read_open(a, mine, stream_open, stream_read, stream_close));
}

static int
stream_open(struct archive *a, void *client_data)
{
	struct read_stream_data *mine = client_data;

	(void)a; /* UNUSED */
	if (mine->fd == NULL) {
		archive_set_error(a, EINVAL, "Bad FILE pointer");
		free(mine->buffer);
		free(mine);
		return (ARCHIVE_FATAL);
	}
	return (ARCHIVE_OK);
}

static ssize_t
stream_read(struct archive *a, void *client_data, const void **buff)
{
	struct read_stream_data *mine = client_data;

	(void)a; /* UNUSED */
	*buff = mine->buffer;
	return fread(mine->buffer, 1, mine->block_size, mine->fd);
}

static int
stream_close(struct archive *a, void *client_data)
{
	struct read_stream_data *mine = client_data;

	(void)a; /* UNUSED */
	free(mine->buffer);
	free(mine);
	return (ARCHIVE_OK);
}
