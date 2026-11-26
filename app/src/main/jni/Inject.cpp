#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
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

    void postAppSpecialize(const AppSpecializeArgs *args) override {
        // Esconde da memória do app qualquer .so carregado de /data/adb/modules
        RemapTools::RemapLibrary("/data/adb/modules");

        // Descarrega a biblioteca deste próprio módulo auxiliar para não deixar rastro
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api;
    JNIEnv *env;
};

REGISTER_ZYGISK_MODULE(HideMapModule)
