#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <android/log.h>
#include "RemapTools.h"

#define HIDE_LOG(...) __android_log_print(ANDROID_LOG_ERROR, "HideMap", __VA_ARGS__)

// Le o nome do pacote atual direto do /proc/self/cmdline usando std::string
static std::string GetCurrentPackageName() {
    std::ifstream file("/proc/self/cmdline");
    if (!file.is_open()) return "";
    std::string cmdline;
    std::getline(file, cmdline, '\0');
    return cmdline;
}

// Verifica se o pacote atual esta no packages.txt
static bool ShouldHideForCurrentApp(const std::string& pkg) {
    if (pkg.empty()) return false;

    std::ifstream file("/data/adb/modules/zygisk_hidemap/packages.txt");
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Remove espacos e quebras de linha (\r e \n)
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (!line.empty() && pkg.find(line) != std::string::npos) {
            return true;
        }
    }
    return false;
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
