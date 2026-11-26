#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <android/log.h>
#include "zygisk.hpp"
#include "RemapTools.h"

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

        // Abre a pasta do modulo com privilegio do Zygisk antes de entrar no app
        int dir_fd = api->getModuleDir();
        if (dir_fd >= 0) {
            int file_fd = openat(dir_fd, "packages.txt", O_RDONLY);
            if (file_fd >= 0) {
                std::string content;
                char ch;
                while (read(file_fd, &ch, 1) == 1) {
                    if (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t') {
                        if (!content.empty()) {
                            if (content == current_pkg) {
                                should_hide = true;
                                break;
                            }
                            content.clear();
                        }
                    } else {
                        content.push_back(ch);
                    }
                }
                if (!should_hide && !content.empty() && content == current_pkg) {
                    should_hide = true;
                }
                close(file_fd);
            }
            close(dir_fd);
        }

        // Se o app nao estiver no packages.txt, descarrega o modulo imediatamente
        if (!should_hide) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (should_hide) {
            // Esconde qualquer .so carregado de /data/adb/modules apenas neste app
            RemapTools::RemapLibrary("/data/adb/modules");
        }
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool should_hide = false;
};

REGISTER_ZYGISK_MODULE(HideMapModule)
