#pragma once

#include <jni.h>

#define ZYGISK_API_VERSION 4

namespace zygisk {

struct Api;
struct AppSpecializeArgs;
struct ServerSpecializeArgs;

class ModuleBase {
public:
    virtual void onLoad([[maybe_unused]] Api *api, [[maybe_unused]] JNIEnv *env) {}
    virtual void preAppSpecialize([[maybe_unused]] AppSpecializeArgs *args) {}
    virtual void postAppSpecialize([[maybe_unused]] const AppSpecializeArgs *args) {}
    virtual void preServerSpecialize([[maybe_unused]] ServerSpecializeArgs *args) {}
    virtual void postServerSpecialize([[maybe_unused]] const ServerSpecializeArgs *args) {}
};

struct AppSpecializeArgs {
    jint &uid;
    jint &gid;
    jintArray &gids;
    jint &runtime_flags;
    jobjectArray &rlimits;
    jint &mount_external;
    jstring &se_info;
    jstring &nice_name;
    jstring &instruction_set;
    jstring &app_data_dir;

    jintArray *const fds_to_ignore;
    jboolean *const is_child_zygote;
    jboolean *const is_top_app;
    jobjectArray *const pkg_data_dir_list;
    jobjectArray *const whitelisted_data_info_list;
    jboolean *const mount_data_storage;
    jboolean *const mount_storage_or_media;
    jboolean *const allow_unload;

    AppSpecializeArgs() = delete;
};

struct ServerSpecializeArgs {
    jint &uid;
    jint &gid;
    jintArray &gids;
    jint &runtime_flags;
    jlong &permitted_capabilities;
    jlong &effective_capabilities;

    ServerSpecializeArgs() = delete;
};

enum Option : int {
    FORCE_DENYLIST_UNMOUNT = 0,
    DLCLOSE_MODULE_LIBRARY = 1,
};

enum StateFlag : uint32_t {
    PROCESS_GRANTED_ROOT = (1u << 0),
    PROCESS_ON_DENYLIST = (1u << 1),
};

namespace internal {
struct ModuleAbi {
    long api_version;
    ModuleBase *impl;

    void (*preAppSpecialize)(ModuleBase *, AppSpecializeArgs *);
    void (*postAppSpecialize)(ModuleBase *, const AppSpecializeArgs *);
    void (*preServerSpecialize)(ModuleBase *, ServerSpecializeArgs *);
    void (*postServerSpecialize)(ModuleBase *, const ServerSpecializeArgs *);

    ModuleAbi(ModuleBase *module) : api_version(ZYGISK_API_VERSION), impl(module) {
        preAppSpecialize = [](auto self, auto args) { self->preAppSpecialize(args); };
        postAppSpecialize = [](auto self, auto args) { self->postAppSpecialize(args); };
        preServerSpecialize = [](auto self, auto args) { self->preServerSpecialize(args); };
        postServerSpecialize = [](auto self, auto args) { self->postServerSpecialize(args); };
    }
};

struct ApiTable {
    void *impl;
    bool (*registerModule)(ApiTable *, ModuleAbi *);

    void (*hookJniNativeMethods)(JNIEnv *, const char *, JNINativeMethod *, int);
    void (*pltHookRegister)(dev_t, ino_t, const char *, void *, void **);
    bool (*pltHookCommit)();
    int (*connectCompanion)(void *);
    void (*setOption)(void *, Option);
    int (*getModuleDir)(void *);
    uint32_t (*getFlags)(void *);
};
} // namespace internal

struct Api {
    void hookJniNativeMethods(JNIEnv *env, const char *className, JNINativeMethod *methods, int numMethods) {
        tbl->hookJniNativeMethods(env, className, methods, numMethods);
    }
    void pltHookRegister(dev_t dev, ino_t inode, const char *symbol, void *newFunc, void **oldFunc) {
        tbl->pltHookRegister(dev, inode, symbol, newFunc, oldFunc);
    }
    bool pltHookCommit() {
        return tbl->pltHookCommit();
    }
    int connectCompanion() {
        return tbl->connectCompanion(tbl->impl);
    }
    void setOption(Option opt) {
        tbl->setOption(tbl->impl, opt);
    }
    int getModuleDir() {
        return tbl->getModuleDir(tbl->impl);
    }
    uint32_t getFlags() {
        return tbl->getFlags(tbl->impl);
    }

private:
    internal::ApiTable *tbl;
    template<class T> friend void ::zygisk_module_entry(void *, void *);
};

} // namespace zygisk

extern "C" {

[[gnu::visibility("default")]]
void zygisk_module_entry(void *table, void *env);

[[gnu::visibility("default")]]
void zygisk_companion_entry(int client);

} // extern "C"

#define REGISTER_ZYGISK_MODULE(clazz) \
void zygisk_module_entry(void *table, void *env) { \
    static clazz module; \
    static zygisk::internal::ModuleAbi abi(&module); \
    static zygisk::Api api; \
    api.tbl = static_cast<zygisk::internal::ApiTable *>(table); \
    if (api.tbl->registerModule(api.tbl, &abi)) { \
        module.onLoad(&api, static_cast<JNIEnv *>(env)); \
    } \
}

#define REGISTER_ZYGISK_COMPANION(func) \
void zygisk_companion_entry(int client) { \
    func(client); \
}
