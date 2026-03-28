# Labs -- Dependencias automaticas

## Descripcion

Laboratorio para demostrar el bug silencioso de dependencias de headers en Make,
resolverlo con los flags de GCC (-MMD -MP), y verificar que los cambios en
headers provocan la recompilacion correcta de los .o afectados.

## Prerequisitos

- GCC instalado (`gcc --version`)
- GNU Make instalado (`make --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | El bug de headers | Makefile sin dependencias de headers: cambiar un .h no provoca recompilacion |
| 2 | gcc -MM | Usar GCC para generar la lista de dependencias en formato Makefile |
| 3 | -MMD -MP y -include | Generar archivos .d como efecto secundario de la compilacion e incluirlos en el Makefile |
| 4 | Verificar recompilacion selectiva | Comprobar que touch sobre un .h recompila exactamente los .o que dependen de el |
| 5 | -MP ante headers eliminados | Demostrar que -MP evita el error cuando un header se borra del proyecto |
| 6 | Inspeccion de archivos .d | Examinar el contenido de los .d generados y entender su formato |

## Archivos

```
labs/
├── README.md         <- Ejercicios paso a paso
├── config.h          <- Header con APP_NAME y VERSION
├── logger.h          <- Header con declaracion de log_message()
├── logger.c          <- Implementacion de log_message() (usa config.h)
├── engine.h          <- Header con declaracion de run_engine()
├── engine.c          <- Implementacion de run_engine() (usa logger.h)
├── main.c            <- Programa principal (usa config.h, engine.h)
├── Makefile.broken   <- Makefile SIN dependencias automaticas (tiene el bug)
└── Makefile.fixed    <- Makefile CON dependencias automaticas (-MMD -MP)
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~25 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| app | Ejecutable | Si (limpieza final) |
| *.o | Archivos objeto | Si (limpieza final) |
| *.d | Archivos de dependencias | Si (limpieza final) |
