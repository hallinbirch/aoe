/* Copyright 2016-2018 the Age of Empires Free Software Remake authors. See LEGAL for legal info */

#include "drs.h"
#include "fs.h"

#include "../setup/dbg.h"
#include "../setup/def.h"

#include <errno.h>
#include <string.h>

#include <stdlib.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#define DRS_MAX 16

struct drs_hdr {
	char copyright[40];
	char version[16];
	uint32_t nlist;
	uint32_t listend; // XXX go figure
};

struct drs_map {
	int fd;
	void *data;
	struct stat st;
	char *name;
};

struct drs_files {
	struct drs_map *data;
	size_t cap, count;
} drs_files;

void drs_map_init(struct drs_map *drs, const char *name);
void drs_map_open(struct drs_map *drs);
void drs_map_free(struct drs_map *drs);

void drs_files_init(struct drs_files *lst, size_t cap)
{
	lst->data = fmalloc(cap * sizeof(struct drs_map));
	lst->cap = cap;
	lst->count = 0;
}

void drs_files_free(struct drs_files *lst)
{
	for (size_t i = 0, n = lst->count; i < n; ++i)
		drs_map_free(&lst->data[i]);
	free(lst->data);
}

void drs_files_add(struct drs_files *lst, const char *name)
{
	if (lst->count == lst->cap) {
		size_t n = 2 * lst->cap;
		if (!n)
			n = 8;

		lst->data = frealloc(lst->data, n * sizeof(struct drs_map));
		lst->cap = n;
	}
	drs_map_init(&lst->data[lst->count++], name);
}

void drs_files_open(struct drs_files *lst)
{
	for (size_t i = 0, n = lst->count; i < n; ++i)
		drs_map_open(&lst->data[i]);
}

void drs_map_init(struct drs_map *drs, const char *name)
{
	char buf[FS_BUFSZ];
	fs_data_path(buf, sizeof buf, name);
	drs->name = strdup(buf);
}

void drs_map_open(struct drs_map *drs)
{
	if ((drs->fd = open(drs->name, O_RDONLY)) == -1
		|| fstat(drs->fd, &drs->st) == -1)
		goto fail;

	drs->data = mmap(NULL, drs->st.st_size, PROT_READ, MAP_SHARED | MAP_FILE, drs->fd, 0);
	if (drs->data == MAP_FAILED)
fail:
		panicf("%s: %s\n", drs->name, strerror(errno));

	// verify data
	const struct drs_hdr *hdr = drs->data;
	if (strncmp(hdr->version, "1.00tribe", strlen("1.00tribe")))
		panicf("%s: not a DRS file\n", drs->name);

}

void drs_map_free(struct drs_map *drs)
{
	free(drs->name);

	if (drs->data != MAP_FAILED)
		munmap(drs->data, drs->st.st_size);

	if (drs->fd != -1)
		close(drs->fd);
}

void drs_add(const char *name)
{
	drs_files_add(&drs_files, name);
}

void drs_init(void)
{
	drs_files_open(&drs_files);
}

void drs_free(void)
{
	drs_files_free(&drs_files);
}

#define drs_map_size(map) ((size_t)map->st.st_size)

int drs_files_map(const struct drs_files *lst, unsigned type, unsigned res_id, off_t *offset, char **dblk, size_t *count)
{
	for (size_t i = 0, n = lst->count; i < n; ++i) {
		struct drs_map *map = &lst->data[i];
		struct drs_hdr *hdr = map->data;

		struct drs_list *list = (struct drs_list*)((char*)hdr + sizeof(struct drs_hdr));
		for (size_t j = 0; j < hdr->nlist; ++j, ++list) {
			if (list->type != type)
				continue;
			if (list->offset > drs_map_size(map)) {
				fputs("bad item offset\n", stderr);
				goto fail;
			}
			struct drs_item *item = (struct drs_item*)((char*)hdr + list->offset);
			for (unsigned k = 0; k < list->size; ++k, ++item) {
				if (list->offset + (k + 1) * sizeof(struct drs_item) > drs_map_size(map)) {
					fputs("bad list\n", stderr);
					goto fail;
				}
				if (item->id == res_id) {
					dbgf("drs map: %u from %s\n", res_id, map->name);
					*offset = item->offset;
					*dblk = map->data;
					*count = item->size;
					return 0;
				}
			}
		}
	}
fail:
	dbgf("%s: not found: type=%X, res_id=%d\n", __func__, type, res_id);
	return 1;
}

const void *drs_get_item(unsigned type, unsigned id, size_t *count)
{
	char *data;
	off_t offset;

	if (drs_files_map(&drs_files, type, id, &offset, &data, count) || !data)
		panicf("Cannot find resource %d\n", id);

	return data + offset;
}

void slp_read(struct slp *dst, const void *data)
{
	dst->hdr = (void*)data;
	dst->info = (struct slp_frame_info*)((char*)data + sizeof(struct slp_header));
}