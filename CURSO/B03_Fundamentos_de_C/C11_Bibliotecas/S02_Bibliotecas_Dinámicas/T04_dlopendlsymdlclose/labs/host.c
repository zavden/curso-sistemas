/* host.c -- loads all .so plugins from plugins/ directory */
#include "plugin.h"
#include <dlfcn.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>

#define MAX_PLUGINS 16

typedef struct {
    void   *handle;
    Plugin *plugin;
} LoadedPlugin;

static int load_plugin(const char *path, LoadedPlugin *lp) {
    lp->handle = dlopen(path, RTLD_NOW);
    if (!lp->handle) {
        fprintf(stderr, "dlopen(%s): %s\n", path, dlerror());
        return -1;
    }

    dlerror(); /* clear */
    Plugin *(*create)(void);
    void *sym = dlsym(lp->handle, "plugin_create");
    memcpy(&create, &sym, sizeof(create));
    char *err = dlerror();
    if (err) {
        fprintf(stderr, "dlsym(%s): %s\n", path, err);
        dlclose(lp->handle);
        return -1;
    }

    lp->plugin = create();
    if (lp->plugin->init() != 0) {
        fprintf(stderr, "Plugin %s init failed\n", path);
        dlclose(lp->handle);
        return -1;
    }

    printf("Loaded plugin: %s v%s\n", lp->plugin->name, lp->plugin->version);
    return 0;
}

int main(void) {
    LoadedPlugin plugins[MAX_PLUGINS];
    int count = 0;

    DIR *dir = opendir("plugins");
    if (!dir) {
        perror("opendir(\"plugins\")");
        return 1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < MAX_PLUGINS) {
        size_t len = strlen(entry->d_name);
        if (len > 3 && strcmp(entry->d_name + len - 3, ".so") == 0) {
            char path[512];
            snprintf(path, sizeof(path), "./plugins/%s", entry->d_name);
            if (load_plugin(path, &plugins[count]) == 0) {
                count++;
            }
        }
    }
    closedir(dir);

    if (count == 0) {
        printf("No plugins found in plugins/\n");
        return 0;
    }

    printf("\n--- Processing \"Hello World\" with %d plugin(s) ---\n", count);
    for (int i = 0; i < count; i++) {
        plugins[i].plugin->process("Hello World");
    }

    printf("\n--- Cleanup ---\n");
    for (int i = 0; i < count; i++) {
        plugins[i].plugin->cleanup();
        dlclose(plugins[i].handle);
    }

    return 0;
}
