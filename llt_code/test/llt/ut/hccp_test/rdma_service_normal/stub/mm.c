#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
/*
void *mmap(void *addr, size_t length, int prot, int flags,
                  int fd, off_t offset)
{
	return malloc(length);
};

int munmap(void *addr, size_t length)
{
	free(addr);
};
*/

int memcpy_s(void * dest, size_t destMax, const void * src, size_t count)
{
	memcpy(dest, src, count);
	return 0;
}

int memset_s(void * dest, size_t destMax, int c, size_t count)
{
	memset(dest, c, count);
	return 0;
}

int strcpy_s(void * dest, size_t destMax, const void * src)
{
	if (src == NULL) {
		printf("src is null\n");
		return -1;
	}
	if (strlen(src) > destMax) {
		printf("error %d %d\n", strlen(src), destMax);
		return -1;
	}
	strcpy(dest, src);
	return 0;
}

int strncpy_s(char *strDest, size_t destMax, const char *strSrc, size_t count)
{
	strncpy(strDest, strSrc, count);
	return 0;
}

int sprintf_s(char *strDest, size_t destMax, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vsprintf(strDest, format, args);
	va_end (args);
	return 1;
}

int snprintf_s(char *strDest, size_t destMax, size_t count, const char *format, ...)
{
        va_list args;
        va_start(args, format);
        vsnprintf(strDest, count, format, args);
        va_end (args);
        return 1;
}

int strcat_s(char *strDest, size_t destMax, const char *strSrc)
{
    strcat(strDest, strSrc);
    return 0;
}

int sscanf_s(const char *buffer, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vsscanf(buffer, format, args);
	va_end (args);
	return 1;
}