#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <thread>
#include <chrono>
#include <android/log.h>
#include "RemapTools.h"

#define HIDE_LOG(...) __android_log_print(ANDROID_LOG_ERROR, "HideMap", __VA_ARGS__)

// Le o nome do pacote atual direto do /proc/self/cmdline
static std::string GetCurrentPackageName() {
    FILE *fp = fopen("/proc/self/cmdline", "r");
    if (!fp) return "";
    char buf = {0};
    fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    return std::string(buf);
}

// Verifica se o pacote atual esta no packages.txt
static bool ShouldHideForCurrentApp(const std::string& pkg) {
    if (pkg.empty()) return false;

    FILE *fp = fopen("/data/adb/modules/zygisk_hidemap/packages.txt", "r");
    if (!fp) {
        HIDE_LOG("Nao conseguiu abrir packages.txt: %s", strerror(errno));
        return false;
    }

    char line = {0};
    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n \t")] = 0;
        if (strlen(line) > 0 && pkg.find(line) != std::string::npos) {
            found = true;
            break;
        }
    }
    fclose(fp);
    return found;
}

// Executa automaticamente assim que o Magisk carrega a .so no processo
__attribute__((constructor))
void init_hidemap() {
    std::string pkg = GetCurrentPackageName();

    if (!ShouldHideForCurrentApp(pkg)) {
        return;
    }

    HIDE_LOG("Iniciado no alvo: %s", pkg.c_str());

    // Espera 400ms para o targetedfix terminar de carregar e entao limpa tudo da memoria
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        HIDE_LOG("Remapeando modulos na memoria...");
        RemapTools::RemapLibrary("/data/adb/modules");
        HIDE_LOG("Concluido! Memoria limpa.");
    }).detach();
}
