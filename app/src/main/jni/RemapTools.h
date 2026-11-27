#pragma once

#include <cstdint>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/mman.h>
#include <vector>
#include <string>

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

    std::vector<ProcMapInfo> ListModulesWithName(const std::string& name, const std::string& exclude) {
        std::vector<ProcMapInfo> returnVal;

        char buffer[512];
        FILE *fp = fopen("/proc/self/maps", "re");
        if (fp != nullptr) {
            while (fgets(buffer, sizeof(buffer), fp)) {
                if (strstr(buffer, name.c_str()) != nullptr &&
                    (exclude.empty() || strstr(buffer, exclude.c_str()) == nullptr)) {
                    ProcMapInfo info{};
                    char perms[10] = {0};
                    char path[256] = {0};
                    char dev[32] = {0};

                    int parsed = sscanf(buffer, "%" SCNxPTR "-%" SCNxPTR " %9s %" SCNxPTR " %31s %" SCNu64 " %255s",
                                        &info.start, &info.end, perms, &info.offset, dev, &info.inode, path);

                    if (parsed >= 6) {
                        if (strchr(perms, 'r')) info.perms |= PROT_READ;
                        if (strchr(perms, 'w')) info.perms |= PROT_WRITE;
                        if (strchr(perms, 'x')) info.perms |= PROT_EXEC;

                        // Ignora segmentos executáveis para reduzir o risco de crash na RAM
                        if (info.perms & PROT_EXEC) {
                            continue;
                        }

                        info.dev = dev;
                        info.path = path;
                        returnVal.push_back(info);
                    }
                }
            }
            fclose(fp);
        }
        return returnVal;
    }

    void RemapLibrary(const std::string& name, const std::string& exclude = "") {
        std::vector<ProcMapInfo> maps = ListModulesWithName(name, exclude);

        for (const ProcMapInfo& info : maps) {
            void *address = (void *)info.start;
            size_t size = info.end - info.start;

            if (size == 0) continue;

            // 1. Aloca cópia temporária em memória anónima
            void *backup = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            if (backup == MAP_FAILED) {
                continue;
            }

            if ((info.perms & PROT_READ) == 0) {
                mprotect(address, size, info.perms | PROT_READ);
            }

            // 2. Copia o conteúdo original
            std::memcpy(backup, address, size);

            // 3. Remove o mapeamento ligado ao ficheiro no disco
            if (munmap(address, size) != 0) {
                munmap(backup, size);
                continue;
            }

            // 4. Recria página anónima no mesmo endereço exato
            void *new_map = mmap(address, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
            if (new_map == MAP_FAILED) {
                munmap(backup, size);
                continue;
            }

            // 5. Restaura os dados e aplica as permissões originais não executáveis
            std::memcpy(new_map, backup, size);
            munmap(backup, size);

            mprotect(new_map, size, info.perms);
        }
    }
}
