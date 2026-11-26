#include <stdio.h>
#include <string>
#include <jni.h>
#include <dlfcn.h>
#include <unistd.h>

#include "Include/Logger.h"
#include "RemapTools.h"

extern "C" {
    JNIEXPORT void JNICALL
    Java_com_reveny_remap_hide_MainActivity_Load(JNIEnv *env, jobject thiz, jstring native_lib_dir) {
        LOGI("Load injector in pid %d", getpid());

        const char* dir = env->GetStringUTFChars(native_lib_dir, nullptr);
        if (dir == nullptr) {
            LOGE("Failed to get native_lib_dir");
            return;
        }

        // Nome da biblioteca alvo que será carregada e ocultada na RAM
        const std::string target_lib = "libTest.so";
        std::string path = std::string(dir) + "/" + target_lib;

        // Libera a string JNI corretamente
        env->ReleaseStringUTFChars(native_lib_dir, dir);

        // Carrega a biblioteca na memória
        void *handle = dlopen(path.c_str(), RTLD_NOW);
        if (handle == nullptr) {
            LOGE("Error opening %s: %s", target_lib.c_str(), dlerror());
            return;
        }

        // Executa o remapeamento anônimo para esconder do /proc/self/maps
        RemapTools::RemapLibrary(target_lib);
        LOGI("Successfully remapped %s", target_lib.c_str());
    }
}
