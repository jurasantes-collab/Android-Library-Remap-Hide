#pragma once

#include <cstdint>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
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

    static inline uintptr_t PageAlignDown(uintptr_t addr, size_t page_size) {
        return addr & ~(static_cast<uintptr_t>(page_size) - 1);
    }

    static inline uintptr_t PageAlignUp(uintptr_t addr, size_t page_size) {
        uintptr_t mask = static_cast<uintptr_t>(page_size) - 1;
        return (addr + mask) & ~mask;
    }

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
        long page_sz_raw = sysconf(_SC_PAGESIZE);
        size_t page_size = (page_sz_raw > 0) ? static_cast<size_t>(page_sz_raw) : 4096;

        std::vector<ProcMapInfo> maps = ListModulesWithName(name, exclude);

        for (const ProcMapInfo& info : maps) {
            uintptr_t aligned_start = PageAlignDown(info.start, page_size);
            uintptr_t aligned_end = PageAlignUp(info.end, page_size);

            if (aligned_end <= aligned_start) continue;

            size_t size = static_cast<size_t>(aligned_end - aligned_start);
            void *address = reinterpret_cast<void *>(aligned_start);

            // 1. Garante leitura antes de copiar para o buffer anónimo
            if ((info.perms & PROT_READ) == 0) {
                mprotect(address, size, PROT_READ | PROT_WRITE);
            }

            // 2. Aloca cópia temporária na RAM e copia o conteúdo original
            void *backup = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                                MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            if (backup == MAP_FAILED) {
                continue;
            }

            std::memcpy(backup, address, size);

            // 3. Remove o mapeamento ligado ao ficheiro no disco
            if (munmap(address, size) != 0) {
                munmap(backup, size);
                continue;
            }

            // 4. Recria página anónima no mesmo endereço exato alinhado
            void *new_map = mmap(address, size, PROT_READ | PROT_WRITE,
                                 MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
            if (new_map == MAP_FAILED) {
                munmap(backup, size);
                continue;
            }

            // 5. Restaura os dados, limpa cache ARMv7 e reaplica permissões originais
            std::memcpy(new_map, backup, size);
            munmap(backup, size);

            if (info.perms & PROT_EXEC) {
                __builtin___clear_cache(reinterpret_cast<char *>(new_map),
                                      reinterpret_cast<char *>(new_map) + size);
            }

            int target_perms = (info.perms != 0) ? static_cast<int>(info.perms) : PROT_READ;
            mprotect(new_map, size, target_perms);
        }
    }
}
