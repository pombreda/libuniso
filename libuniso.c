/* libuniso.c - Unpack ISO9660 File System from a stream
 *
 * Copyright (C) 2011 Timo Teräs <timo.teras@iki.fi>
 * Copyright (C) 2011 Natanael Copa <ncopa@alpinelinux.org>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation. 
 *
 * See http://www.gnu.org/licenses/lgpl-2.1.html for details.
 */

/*
 * TODO:
 * - fix unaligned 16-bit accesses from iso headers (ARM / MIPS)
 */

/* needed for SPLICE_F_MOVE */
#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <wchar.h>
#include <malloc.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include <sys/stat.h>

#include "uniso.h"

/**/

#ifndef container_of
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#define LIST_POISON1 (void *) 0xdeadbeef
#define LIST_POISON2 (void *) 0xabbaabba

struct list_head {
	struct list_head *next, *prev;
};

static inline void list_init(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}

static inline int list_hashed(const struct list_head *n)
{
	return n->next != n && n->next != NULL;
}

#define list_entry(ptr, type, member) container_of(ptr,type,member)

/**/

#define ISOFS_BLOCK_SIZE		2048
#define ISOFS_TMPBUF_SIZE		(32*ISOFS_BLOCK_SIZE)

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define endianess	le
#else
#define endianess	be
#endif

struct isofs723 {
	u_int16_t le;
	u_int16_t be;
};

struct isofs731 {
	u_int32_t le;
};

struct isofs732 {
	u_int32_t be;
};

struct isofs733 {
	u_int32_t le;
	u_int32_t be;
};

#define ISOFS_VD_PRIMARY		1
#define ISOFS_VD_SUPPLEMENTARY		2
#define ISOFS_VD_END			255
#define ISOFS_STANDARD_ID		"CD001"

struct isofs_volume_descriptor {
	unsigned char type;
	unsigned char id[5];
	unsigned char version;
	unsigned char data[2040];
};

/* Primary or Supplementary Volume Descriptor
 * (they are about the same, Supplementary just has few more fields
 *  and the date they point to might be encoded differently) */
struct isofs_pri_sup_descriptor {
	unsigned char 	type;
	unsigned char 	id[5];
	unsigned char 	version;
	char		flags;			/* unused1 in primary */
	char		system_id[32];
	char		volume_id[32];
	char		unused2[8];
	struct isofs733	volume_space_size;
	char		escape[32];		/* unused3 in primary */
	struct isofs723	volume_set_size;
	struct isofs723	volume_sequence_number;
	struct isofs723	logical_block_size;
	struct isofs733	path_table_size;
	struct isofs731	type_l_path_table, opt_type_l_path_table;
	struct isofs732	type_m_path_table, opt_type_m_path_table;
	char		root_directory_record[34];
	char		volume_set_id[128];
	char		publisher_id[128];
	char		preparer_id[128];
	char		application_id[128];
	char		copyright_file_id[37];
	char		abstract_file_id[37];
	char		bibliographic_file_id[37];
	char		creation_date[17];
	char		modification_date[17];
	char		expiration_date[17];
	char		effective_date[17];
	char		file_structure_version;
	char		unused4[1];
	char		application_data[512];
	char		unused5[653];
};

struct isofs_directory_record {
	unsigned char	length;
	unsigned char	ext_attr_length;
	struct isofs733	extent;
	struct isofs733	size;
	char		date[7];
	char		flags;
#define ISOFS_DR_FLAG_DIRECTORY		(1<<1)
	unsigned char	file_unit_size;
	unsigned char	interleave;
	struct isofs723	volume_sequence_number;
	unsigned char	name_len;
	char		name[0];
} __attribute__((packed));

struct uniso_reader;
struct uniso_context;

typedef int (*uniso_handler_f)(struct uniso_context *, struct uniso_reader *);

struct uniso_reader {
	struct list_head parser_list;
	size_t offset;
	uniso_handler_f handler;
};

struct uniso_dirent {
	struct uniso_reader reader;
	u_int32_t size;
	u_int32_t flags;
	char name[0];
};

struct uniso_context {
	int stream_fd, null_fd;
	int skip_method, copy_method;
	int joliet_level, loglevel;
	size_t pos, last_queued_read;
	struct list_head parser_head;
	struct uniso_reader volume_desc_reader;
	unsigned char *tmpbuf;
	size_t expected_size;
	void (*update_progress)(size_t completed, size_t expected_size,
				const char *filename, void *user_data);
	void *user_data;
	char *current_file;
};

static void update_progress(struct uniso_context *ctx)
{
	if (ctx->update_progress != NULL)
		ctx->update_progress(ctx->pos, ctx->expected_size, ctx->current_file, ctx->user_data);
}

static int do_splice(struct uniso_context *ctx, int to_fd, int bytes)
{
	int r;

	do {
		r = splice(ctx->stream_fd, NULL, to_fd, NULL, bytes,
			   SPLICE_F_MOVE);
		if (r < 0)
			return -errno;

		bytes -= r;
		ctx->pos += r;
		update_progress(ctx);
	} while (bytes != 0);

	return 0;
}

static int do_read(struct uniso_context *ctx, unsigned char *buf, int bytes)
{
	int r;

	do {
		r = read(ctx->stream_fd, buf, bytes);
		if (r < 0) {
			perror("read");
			return -errno;
		}
		bytes -= r;
		buf += r;
		ctx->pos += r;
		update_progress(ctx);
	} while (bytes != 0);

	return 0;
}

static int do_write(int tofd, const unsigned char *buf, int bytes)
{
	int r;

	do {
		r = write(tofd, buf, bytes);
		if (r < 0) {
			perror("write");
			return -errno;
		}
		bytes -= r;
		buf += r;
	} while (bytes != 0);

	return 0;
}

static int do_skip(struct uniso_context *ctx, unsigned int bytes)
{
	int r, now;

	switch (ctx->skip_method) {
	case 0:
		if (lseek(ctx->stream_fd, bytes, SEEK_CUR) != (off_t) -1) {
			ctx->pos += bytes;
			update_progress(ctx);
			break;
		}
		ctx->skip_method = 1;
	case 1:
		if (do_splice(ctx, ctx->null_fd, bytes) == 0)
			break;
		ctx->skip_method = 2;
	case 2:
		while (bytes) {
			now = bytes;
			if (now > ISOFS_TMPBUF_SIZE)
				now = ISOFS_TMPBUF_SIZE;

			r = do_read(ctx, ctx->tmpbuf, now);
			if (r < 0)
				return r;

			bytes -= now;
		}
		break;
	}
	return 0;
}

static int do_copy(struct uniso_context *ctx, int tofd, unsigned int bytes)
{
	int r, now;

	switch (ctx->copy_method) {
	case 0:
		if (do_splice(ctx, tofd, bytes) == 0)
			break;
		ctx->skip_method = 1;
	case 1:
		/* FIXME: Use mmaped IO */
		ctx->skip_method = 2;
	case 2:
		while (bytes) {
			now = bytes;
			if (now > ISOFS_TMPBUF_SIZE)
				now = ISOFS_TMPBUF_SIZE;

			r = do_read(ctx, ctx->tmpbuf, now);
			if (r < 0)
				return r;

			r = do_write(tofd, ctx->tmpbuf, now);
			if (r < 0)
				return r;

			bytes -= now;
		}
		break;
	}
	return 0;
}

static int queue_reader(
		struct uniso_context *ctx, struct uniso_reader *reader,
		size_t offset, uniso_handler_f handler)
{
	struct list_head *n;
	struct uniso_reader *r;

	if (offset < ctx->pos) {
		fprintf(stderr, "ERROR: non-linear reads are not supported "
				"(asked %zx, we are at %zx)\n",
			offset, ctx->last_queued_read);
		return -1;
	}

	reader->offset = offset;
	reader->handler = handler;
	list_init(&reader->parser_list);

	if (offset >= ctx->last_queued_read || !list_hashed(&ctx->parser_head)) {
		ctx->last_queued_read = offset;
		list_add_tail(&reader->parser_list, &ctx->parser_head);
		return 0;
	}

	for (n = ctx->parser_head.prev; n != &ctx->parser_head; n = n->prev) {
		r = list_entry(n, struct uniso_reader, parser_list);
		if (r->offset < offset)
			break;
	}
	list_add(&reader->parser_list, n);

	return 0;
}

static int queue_dirent(struct uniso_context *ctx, void *isode, const char *name);

static int link_or_clone(const char *src, const char *dst, size_t bytes)
{
	int src_fd, dst_fd;
	int r;
	static char buf[ISOFS_TMPBUF_SIZE];

	if (link(src, dst) == 0)
		return 0;

	src_fd = open(src, O_RDONLY);
	if (src_fd < 0)
		return src_fd;

	dst_fd = open(dst, O_WRONLY | O_TRUNC | O_CREAT, 0777);
	if (dst_fd < 0) {
		close(src_fd);
		return dst_fd;
	}

	while (bytes) {
		size_t now = bytes;
		if (now > sizeof(buf))
			now = sizeof(buf);
		r = read(src_fd, buf, now);
		if (r < 0)
			return r;

		r = write(dst_fd, buf, now);
		if (r < 0)
			return r;
		bytes -= now;
	}
	close(dst_fd);
	close(src_fd);
	return 0;
}

static int uniso_read_file(struct uniso_context *ctx,
			   struct uniso_reader *rd)
{
	struct uniso_dirent *dir = container_of(rd, struct uniso_dirent, reader);
	int fd, rc;
	static size_t prev_offset = 0;
	static char prev_name[512];
	static size_t prev_size = 0;

	// FIXME: seems that hardlinked files get the same file extent
	// shared. need to fix extraction of such files.
	if (ctx->pos > rd->offset) {
		rc = 1;
		if (rd->offset == prev_offset)
			rc = link_or_clone(prev_name, dir->name, prev_size);
		if (ctx->loglevel > 0 && rc != 0)
			fprintf(stderr, "WARNING: Not extracting '%s' as "
					"it's sharing file-extents with "
					"another file\n", dir->name);
		return 0;
	}

	prev_offset = rd->offset;
	strncpy(prev_name, dir->name, sizeof(prev_name));
	prev_size = dir->size;

	fd = open(dir->name, O_WRONLY | O_TRUNC | O_CREAT, 0777);
	if (fd < 0)
		return -errno;

	ctx->current_file = dir->name;
	if (ctx->loglevel > 1)
		fprintf(stderr, "%s : %d bytes, flags 0x%08x\n",
			dir->name, dir->size, dir->flags);
	rc = do_copy(ctx, fd, dir->size);
	close(fd);

	return rc;
}

static int uniso_read_directory(struct uniso_context *ctx,
				struct uniso_reader *rd)
{
	struct uniso_dirent *dir = container_of(rd, struct uniso_dirent, reader);
	struct isofs_directory_record *ide;
	char buf[512], *name;
	unsigned char *tmp;
	int r, i, offs;

	if (ctx->loglevel > 1)
		fprintf(stderr, "%s : DIR, flags 0x%08x\n",
			dir->name, dir->flags);
	mkdir(dir->name, 0777);

	tmp = malloc(dir->size);
	if (tmp == NULL) {
		r = -ENOMEM;
		goto ret_nomem;
	}

	r = do_read(ctx, tmp, dir->size);
	if (r < 0)
		goto ret;

	strcpy(buf, dir->name);
	strcat(buf, "/");

	for (offs = 0; offs < dir->size; ) {
		ide = (void *) &tmp[offs];

		offs += (ide->length + 1) & ~1;
		if (ide->length == 0) {
			offs &= ~(ISOFS_BLOCK_SIZE-1);
			offs += ISOFS_BLOCK_SIZE;
			continue;
		}

		/* Skip '.' and '..' entries */
		if ((ide->name_len == 0) ||
		    (ide->name_len == 1 && ide->name[0] <= 1))
			continue;

		name = &buf[strlen(dir->name) + 1];
		if (ctx->joliet_level) {
			u_int16_t *wname = (u_int16_t *) ide->name;
			for (i = 0; i < ide->name_len; i += 2, wname++) {
				r = wctomb(name, be16toh(*wname));
				if (r == -1)
					*name++ = '?';
				else
					name += r;
			}
			*name = 0;
		} else {
			memcpy(name, ide->name, ide->name_len);
			name[ide->name_len] = 0;
		}

		queue_dirent(ctx, ide, buf);
	}
	r = 0;

ret:
	free(tmp);
ret_nomem:
	free(dir);
	return r;
}

static int queue_dirent(struct uniso_context *ctx, void *isode, const char *name)
{
	struct isofs_directory_record *ide = isode;
	struct uniso_dirent *dir;

	dir = calloc(1, sizeof(*dir) + strlen(name) + 1);
	if (dir == NULL)
		return -ENOMEM;

	dir->size = ide->size.endianess;
	dir->flags = ide->flags;
	strcpy(dir->name, name);

	return queue_reader(ctx, &dir->reader,
			    ide->extent.endianess * ISOFS_BLOCK_SIZE,
			    (ide->flags & ISOFS_DR_FLAG_DIRECTORY) ?
			    uniso_read_directory : uniso_read_file);
}

static int uniso_read_volume_descriptor(struct uniso_context *ctx,
					struct uniso_reader *rd)
{
	unsigned char buf[ISOFS_BLOCK_SIZE];
	struct isofs_volume_descriptor *vd = (void*) buf;
	struct isofs_pri_sup_descriptor *pd = (void*) buf;
	char root_dir[sizeof(pd->root_directory_record)];
	int r;

	do {
		r = do_read(ctx, buf, ISOFS_BLOCK_SIZE);
		if (r < 0)
			return r;

		if (memcmp(vd->id, ISOFS_STANDARD_ID, sizeof(vd->id)) != 0)
			return -EMEDIUMTYPE;

		switch (vd->type) {
		case ISOFS_VD_PRIMARY:
			if (ctx->joliet_level == 0)
				memcpy(root_dir, pd->root_directory_record, sizeof(root_dir));
			break;
		case ISOFS_VD_SUPPLEMENTARY:
			ctx->expected_size = pd->volume_space_size.endianess * pd->logical_block_size.endianess;
			if (pd->escape[0] == 0x25 && pd->escape[1] == 0x2f) {
				switch (pd->escape[2]) {
				case 0x45:
					ctx->joliet_level++;
				case 0x43:
					ctx->joliet_level++;
				case 0x40:
					ctx->joliet_level++;
					memcpy(root_dir, pd->root_directory_record, sizeof(root_dir));
				}
			}
			break;
		}
	} while (vd->type != ISOFS_VD_END);

	if (ctx->joliet_level && ctx->loglevel > 1)
		fprintf(stderr, "INFO: Microsoft Joliet Level %d\n",
			ctx->joliet_level);

	return queue_dirent(ctx, root_dir, ".");
}

int uniso(int fd, void (*progress_callback)(size_t, size_t, const char*, void *),
		void *user_data)
{
	struct uniso_context context, *ctx = &context;
	struct uniso_reader *rd;
	int r;

	memset(ctx, 0, sizeof(*ctx));
	list_init(&ctx->parser_head);
	ctx->stream_fd = fd;
	ctx->null_fd = open("/dev/null", O_RDWR);
	ctx->tmpbuf = malloc(ISOFS_TMPBUF_SIZE);
	ctx->loglevel = 1;
	ctx->update_progress = progress_callback;
	ctx->user_data = user_data;

	queue_reader(ctx, &ctx->volume_desc_reader,
		     16 * ISOFS_BLOCK_SIZE,
		     uniso_read_volume_descriptor);

	while (list_hashed(&ctx->parser_head)) {
		rd = list_entry(ctx->parser_head.next, struct uniso_reader, parser_list);
		list_del(&rd->parser_list);

		r = rd->offset - ctx->pos;
		if (r > 0) {
			if ((r = do_skip(ctx, r)) < 0)
				return r;
		}
		r = rd->handler(ctx, rd);
		if (r != 0)
			return r;
	}
	ctx->pos = ctx->expected_size;
	ctx->current_file = "";
	update_progress(ctx);

	free(ctx->tmpbuf);
	close(ctx->null_fd);
	return 0;
}

