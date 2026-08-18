#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

extern "C" {

bool FS_FileExists(const char *path) {
    return (access(path, F_OK) == 0);
}

bool FS_DirExists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    return false;
}

int FS_RecursiveMakeDir(const char *dir) {
    char tmp[512];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strlen(tmp);
    if (len == 0) return -1;
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    return mkdir(tmp, 0777);
}

}