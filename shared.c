#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define ENV_ALINES_SOCKET "ALINES_SOCKET"

#define PKID_TO_UI_DISCONNECT 0
#define PKID_TO_UI_MENU 1

#define PKID_FROM_UI_CLOSE 0
#define PKID_FROM_UI_SINGLE_SELECTION 1
#define PKID_FROM_UI_MULTI_SELECTION 2
#define PKID_FROM_UI_CUSTOM_ENTRY 3

#define MENU_FLAG_ALLOW_MULTI 1
#define MENU_FLAG_ALLOW_CUSTOM 2

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u8_MAX 255
#define u16_MAX 65535
#define u32_MAX 4294967295

void panic(char *str)
{
	fprintf(stderr, "critical: %s\n", str);
	exit(1);
}

void *xmalloc(size_t sz)
{
	void *mem = malloc(sz);
	if (!mem) {
		panic("malloc failed");
	}
	return mem;
}

void *xrealloc(void *ptr, size_t sz)
{
	void *mem = realloc(ptr, sz);
	if (!mem) {
		panic("realloc failed");
	}
	return mem;
}

void log_info(char *str)
{
	fprintf(stderr, "info: %s\n", str);
	fflush(stderr);
}

bool fdReadU8(int fd, u8 *out)
{
	return read(fd, out, 1) == 1;
}

bool fdReadU16(int fd, u16 *out)
{
	u16 tmp;
	if (read(fd, &tmp, 2) != 2) {
		return false;
	}
	*out = ntohs(tmp);
	return true;
}

// NOTE: caller needs to free
char *fdReadStr(int fd)
{
	u16 len;
	if (!fdReadU16(fd, &len)) {
		return NULL;
	}
	char *out = xmalloc(len + 1);
	if (read(fd, out, len) != len) {
		free(out);
		return NULL;
	}
	out[len] = 0;
	return out;
}

bool fdWriteU8(int fd, u8 n)
{
	return write(fd, &n, 1) == 1;
}

bool fdWriteU16(int fd, u16 n)
{
	const u16 tmp = htons(n);
	return write(fd, &tmp, 2) == 2;
}

size_t fdWriteStrLen(const char *str)
{
	return strlen(str) + 2;
}

bool fdWriteStr(int fd, const char *str)
{
	const int len = strlen(str);
	if (len > u16_MAX) {
		return false;
	}
	return fdWriteU16(fd, len) && write(fd, str, len) == len;
}
