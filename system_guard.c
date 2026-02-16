#include "system_guard.h"
#include <windows.h>
#include <stdio.h>

#define MIN_DISK_SPACE 524288000 

void check_system_status(const char* folder_path, char* status_buffer, int buffer_size) {
    char issues[256] = "";
    int issue_found = 0;

    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        if (memInfo.dwMemoryLoad > 90) {
            strcat_s(issues, 256, "[WARN] High RAM Usage! ");
            issue_found = 1;
        }
    }

    ULARGE_INTEGER freeBytes, totalBytes, totalFree;
    if (GetDiskFreeSpaceExA(folder_path, &freeBytes, &totalBytes, &totalFree)) {
        if (freeBytes.QuadPart < MIN_DISK_SPACE) {
            strcat_s(issues, 256, "[WARN] Low Disk Space! ");
            issue_found = 1;
        }
    }

    if (issue_found) {
        snprintf(status_buffer, buffer_size, "SYSTEM ALERT: %s", issues);
    }
    else {
        snprintf(status_buffer, buffer_size, "Optimal Performance");
    }
}