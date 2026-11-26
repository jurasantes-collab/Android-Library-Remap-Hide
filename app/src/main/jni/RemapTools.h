//
// Adaptado para 32-bit (armeabi-v7a) e Kernel 3.10
//

#pragma once

#include <cstdint>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <link.h>
#include <sys/mman.h>
#include <vector>
#include <string>

#include "Include/Logger.h"

namespace RemapTools {
    struct ProcMapInfo {
        uintptr_t start;
        uintptr_t end;
        uintptr_t offset;
        uint8_t perms;
        uint64_t inode;
        std::string dev;
        std::string path;
    };

    std::vector<ProcMapInfo> ListModulesWithName(const std::string& name) {
        std::vector<ProcMapInfo> returnVal;

        char buffer;
        FILE *fp = fopen("/proc/self/maps", "re");
        if (fp != nullptr) {
            while (fgets(buffer, sizeof(buffer), fp)) {
                if (strstr(buffer, name.c_str())) {
                    ProcMapInfo info{};
                    char perms = {0};
                    char path = {0};
                    char dev = {0};

                    int parsed = sscanf(buffer, "%" SCNxPTR "-%" SCNxPTR " %9s %" SCNxPTR " %31s %" SCNu64 " %255s",
                                        &info.start, &info.end, perms, &info.offset, dev, &info.inode, path);

                    if (parsed >= 6) {
                        if (strchr(perms, 'r')) info.perms |= PROT_READ;
                        if (strchr(perms, 'w')) info.perms |= PROT_WRITE;
                        if (strchr(perms, 'x')) info.perms |= PROT_EXEC;

                        info.dev = dev;
                        info.path = path;

                        LOGI("Line: %s", buffer);
                        returnVal.push_back(info);
                    }
                }
            }
            fclose(fp);
        }
        return returnVal;
    }

    void RemapLibrary(const std::string& name) {
        std::vector<ProcMapInfo> maps = ListModulesWithName(name);

        for (const ProcMapInfo& info : maps) {
            void *address = (void *)info.start;
            size_t size = info.end - info.start;

            if (size == 0) continue;

            void *map = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            if (map == MAP_FAILED) {
                LOGE("Failed to Allocate Memory: %s", strerror(errno));
                continue;
            }

            if ((info.perms & PROT_READ) == 0) {
                LOGI("Removing protection: %s", info.path.c_str());
                mprotect(address, size, info.perms | PROT_READ);
            }

            LOGI("Allocated at address %p with size of %zu", map, size);

            std::memmove(map, address, size);

            void *res = mremap(map, size, size, MREMAP_MAYMOVE | MREMAP_FIXED, address);
            if (res == MAP_FAILED) {
                LOGE("mremap failed: %s", strerror(errno));
                munmap(map, size);
                continue;
            }

            mprotect(address, size, info.perms);
        }
    }
}
