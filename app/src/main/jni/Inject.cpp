#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <android/log.h>
#include "zygisk.hpp"
#include "RemapTools.h"

#define HIDE_LOG(...) __android_log_print(ANDROID_LOG_ERROR, "HideMap", __VA_ARGS__)

using zygisk::Api;
using zygisk::AppSpecializeArgs;

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

        int dir_fd = api->getModuleDir();
        if (dir_fd >= 0) {
            int file_fd = openat(dir_fd, "packages.txt", O_RDONLY);
            if (file_fd >= 0) {
                std::string token;
                char ch;
                while (read(file_fd, &ch, 1) == 1) {
                    if (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t') {
                        if (!token.empty()) {
                            if (current_pkg.find(token) != std::string::npos) {
                                should_hide = true;
                                break;
                            }
                            token.clear();
                        }
                    } else {
                        token.push_back(ch);
                    }
                }
                if (!should_hide && !token.empty() && current_pkg.find(token) != std::string::npos) {
                    should_hide = true;
                }
                close(file_fd);
            }
            close(dir_fd);
        }

        if (!should_hide) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (should_hide) {
            HIDE_LOG("Limpando modulos para o alvo...");
            // Remapeia todos os modulos em /data/adb/modules, exceto ele mesmo
            RemapTools::RemapLibrary("/data/adb/modules", "zygisk_hidemap");
            HIDE_LOG("Limpeza concluida!");
        }
        // O Magisk descarrega o proprio zygisk_hidemap logo apos retornar
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool should_hide = false;
};

REGISTER_ZYGISK_MODULE(HideMapModule)
