#ifndef EBOOK_READER_FS_H
#define EBOOK_READER_FS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool FS_FileExists(const char *path);
bool FS_DirExists(const char *path);
int FS_RecursiveMakeDir(const char *dir);

#ifdef __cplusplus
}
#endif

#endif