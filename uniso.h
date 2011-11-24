#ifndef UNISO_H
#define UNISO_H

int uniso(int fd, void (*progress_callback)(size_t, size_t, char*, void *),
		void *user_data);

#endif
