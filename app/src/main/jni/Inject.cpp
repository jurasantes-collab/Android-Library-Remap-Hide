#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <string>
#include <vector>
#include "zygisk.hpp"
#include "RemapTools.h"

using zygisk::Api;
using zygisk::AppSpecializeArgs;

static const std::vector<std::string> target_packages = {
    "ru.andr7e.deviceinfohw",
    "SUBSTITUA_PELO_PACOTE_ALVO"
};

struct SelfUnmapTask {
    uintptr_t starts[16];
    size_t lengths[16];
    int count;
};

// Executa fora da .so (em página anónima) e encerra a thread via SYS_exit sem regressar
static void* SelfDestructWorker(void* arg) {
    SelfUnmapTask* task = static_cast<SelfUnmapTask*>(arg);
    if (task == nullptr) {
        return nullptr;
    }

#if defined(__arm__)
    // Shellcode ARMv7 (32-bit):
    // Percorre task->starts[i] e task->lengths[i], chama munmap (r7 = 91)
    // e encerra a thread com exit(0) (r7 = 1) sem regressar à memória desmapeada.
    static const uint32_t arm_stub[] = {
        0xe92d4070, // push {r4, r5, r6, lr}
        0xe1a04000, // mov r4, r0          ; r4 = SelfUnmapTask*
        0xe3a05000, // mov r5, #0          ; r5 = índice i
        0xe5946080, // ldr r6, [r4, #128]  ; r6 = task->count (16*4 + 16*4 = 128)
        // loop:
        0xe1550006, // cmp r5, r6
        0xaa000007, // bge done
        0xe7940105, // ldr r0, [r4, r5, lsl #2]       ; starts[i]
        0xe2841040, // add r1, r4, #64
        0xe7911105, // ldr r1, [r1, r5, lsl #2]       ; lengths[i]
        0xe3a0705b, // mov r7, #91                    ; __NR_munmap (ARM EABI)
        0xef000000, // svc #0
        0xe2855001, // add r5, r5, #1
        0xeafffff6, // b loop
        // done:
        0xe3a00000, // mov r0, #0
        0xe3a07001, // mov r7, #1                     ; __NR_exit (ARM EABI)
        0xef000000  // svc #0
    };

    void* exec_stub = mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (exec_stub != MAP_FAILED) {
        std::memcpy(exec_stub, arm_stub, sizeof(arm_stub));
        __builtin___clear_cache(reinterpret_cast<char*>(exec_stub),
                              reinterpret_cast<char*>(exec_stub) + sizeof(arm_stub));
        using StubFn = void (*)(SelfUnmapTask*);
        reinterpret_cast<StubFn>(exec_stub)(task);
    }
#endif
    return nullptr;
}

class HideMapModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        should_hide = false;

        if (args->nice_name == nullptr) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        const char *pkg_raw = env->GetStringUTFChars(args->nice_name, nullptr);
        if (pkg_raw == nullptr) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        std::string current_pkg(pkg_raw);
        env->ReleaseStringUTFChars(args->nice_name, pkg_raw);

        for (const auto& target : target_packages) {
            if (!target.empty() && target != "SUBSTITUA_PELO_PACOTE_ALVO" &&
                current_pkg.find(target) != std::string::npos) {
                should_hide = true;
                break;
            }
        }

        if (!should_hide) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (should_hide) {
            // 1. Remapeia os outros módulos em /data/adb/modules de forma síncrona
            RemapTools::RemapLibrary("/data/adb/modules", "zygisk_hidemap");

            // 2. Coleta os endereços alinhados a página do próprio zygisk_hidemap
            long page_sz_raw = sysconf(_SC_PAGESIZE);
            size_t page_size = (page_sz_raw > 0) ? static_cast<size_t>(page_sz_raw) : 4096;
            auto self_maps = RemapTools::ListModulesWithName("zygisk_hidemap", "");

            if (!self_maps.empty()) {
                void* shared_buf = mmap(nullptr, sizeof(SelfUnmapTask), PROT_READ | PROT_WRITE,
                                        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
                if (shared_buf != MAP_FAILED) {
                    auto* task = static_cast<SelfUnmapTask*>(shared_buf);
                    task->count = 0;

                    for (const auto& seg : self_maps) {
                        if (task->count < 16 && seg.end > seg.start) {
                            uintptr_t a_start = RemapTools::PageAlignDown(seg.start, page_size);
                            uintptr_t a_end = RemapTools::PageAlignUp(seg.end, page_size);
                            if (a_end > a_start) {
                                task->starts[task->count] = a_start;
                                task->lengths[task->count] = static_cast<size_t>(a_end - a_start);
                                task->count++;
                            }
                        }
                    }

                    if (task->count > 0) {
                        pthread_t tid;
                        if (pthread_create(&tid, nullptr, SelfDestructWorker, task) == 0) {
                            pthread_detach(tid);
                        }
                    }
                }
            }
        }
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool should_hide = false;
};

REGISTER_ZYGISK_MODULE(HideMapModule)
