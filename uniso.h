#ifndef UNISO_H
#define UNISO_H

int uniso(int fd, void (*progress_callback)(size_t current, size_t total,
			const char *filename, void *user_data), void *user_data);

#endif
