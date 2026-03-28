/* plugin.h -- common interface for all plugins */
#ifndef PLUGIN_H
#define PLUGIN_H

typedef struct {
    const char *name;
    const char *version;
    int (*init)(void);
    void (*process)(const char *input);
    void (*cleanup)(void);
} Plugin;

/* Every plugin .so must export this function: */
/* Plugin *plugin_create(void);                */

#endif
