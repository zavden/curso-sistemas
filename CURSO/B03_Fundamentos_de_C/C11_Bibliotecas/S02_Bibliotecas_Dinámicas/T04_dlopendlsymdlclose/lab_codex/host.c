#include "plugin_api.h"

#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PLUGINS 16

typedef struct {
    void *handle;
    Plugin *plugin;
} LoadedPlugin;

static int compare_names(const void *lhs, const void *rhs) {
    const char *const *a = (const char *const *)lhs;
    const char *const *b = (const char *const *)rhs;
    return strcmp(*a, *b);
}

static int has_so_suffix(const char *name) {
    size_t len = strlen(name);
    return len > 3 && strcmp(name + len - 3, ".so") == 0;
}

static int load_plugin(const char *path, LoadedPlugin *slot) {
    Plugin *(*create_fn)(void);
    char *error;
    void *raw_symbol;

    slot->handle = dlopen(path, RTLD_NOW);
    if (!slot->handle) {
        fprintf(stderr, "dlopen(%s): %s\n", path, dlerror());
        return -1;
    }

    dlerror();
    raw_symbol = dlsym(slot->handle, "plugin_create");
    memcpy(&create_fn, &raw_symbol, sizeof(create_fn));
    error = dlerror();
    if (error != NULL) {
        fprintf(stderr, "dlsym(%s): %s\n", path, error);
        dlclose(slot->handle);
        slot->handle = NULL;
        return -1;
    }

    slot->plugin = create_fn();
    if (slot->plugin->init() != 0) {
        fprintf(stderr, "plugin init failed: %s\n", path);
        dlclose(slot->handle);
        slot->handle = NULL;
        slot->plugin = NULL;
        return -1;
    }

    printf("loaded plugin: %s v%s\n", slot->plugin->name, slot->plugin->version);
    return 0;
}

int main(void) {
    DIR *dir;
    struct dirent *entry;
    LoadedPlugin plugins[MAX_PLUGINS];
    char plugin_names[MAX_PLUGINS][256];
    char *sorted_names[MAX_PLUGINS];
    int count = 0;
    int i;

    dir = opendir("plugins");
    if (!dir) {
        perror("opendir");
        return 1;
    }

    while ((entry = readdir(dir)) != NULL && count < MAX_PLUGINS) {
        if (!has_so_suffix(entry->d_name)) {
            continue;
        }

        snprintf(plugin_names[count], sizeof(plugin_names[count]), "%s", entry->d_name);
        sorted_names[count] = plugin_names[count];
        count++;
    }

    closedir(dir);

    qsort(sorted_names, count, sizeof(sorted_names[0]), compare_names);

    for (i = 0; i < count; i++) {
        char path[512];

        snprintf(path, sizeof(path), "plugins/%s", sorted_names[i]);
        if (load_plugin(path, &plugins[i]) != 0) {
            plugins[i].handle = NULL;
            plugins[i].plugin = NULL;
        }
    }

    for (i = 0; i < count; i++) {
        if (plugins[i].plugin == NULL) {
            continue;
        }

        plugins[i].plugin->process("Linux C Rust");
    }

    for (i = 0; i < count; i++) {
        if (plugins[i].plugin == NULL) {
            continue;
        }

        plugins[i].plugin->cleanup();
        dlclose(plugins[i].handle);
    }

    return 0;
}
