#ifndef PLUGIN_API_H
#define PLUGIN_API_H

typedef struct {
    const char *name;
    const char *version;
    int (*init)(void);
    void (*process)(const char *input);
    void (*cleanup)(void);
} Plugin;

Plugin *plugin_create(void);

#endif
