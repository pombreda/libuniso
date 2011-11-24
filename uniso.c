/* uniso.c - Unpack ISO9660 File System from a stream
 *
 * Copyright (C) 2011 Timo Teräs <timo.traas@iki.fi>
 * Copyright (C) 2011 Natanael Copa <ncopa@alpinelinux.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation. 
 *
 * See http://www.gnu.org/licenses/lgpl-2.1.html for details.
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <uniso.h>

struct progress_context {
	const char *filename;
};

static void update_progress_cb(size_t current, size_t total, const char *filename,
			    void *userdata)
{
	struct progress_context *ctx = (struct progress_context *)userdata;
	double percent;

	if (ctx->filename == filename)
		return;

	percent = (double)current * 100 / (double)total;
	printf("(%5.1f%%) %s\n", percent, filename?filename:"");
	ctx->filename = filename;
}

int main(int argc, char *argv[])
{
	int opt;
	void (*callback)(size_t, size_t, const char*, void*) = NULL;
	struct progress_context ctx = { NULL };

	while ((opt = getopt(argc, argv, "v")) != -1) {
		switch (opt) {
		case 'v':
			callback = &update_progress_cb;
			break;
		}
	}

	return uniso(STDIN_FILENO, callback, &ctx);
}
