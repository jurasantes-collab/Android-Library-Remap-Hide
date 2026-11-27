#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
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
            // Remapeia segmentos não executáveis dos outros módulos em /data/adb/modules
            // mantendo o próprio zygisk_hidemap fora para não gerar SIGSEGV
            RemapTools::RemapLibrary("/data/adb/modules", "zygisk_hidemap");
        }
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api = nullptr;
    JNIEnv *env = nullptr;
    bool should_hide = false;
};

REGISTER_ZYGISK_MODULE(HideMapModule)
