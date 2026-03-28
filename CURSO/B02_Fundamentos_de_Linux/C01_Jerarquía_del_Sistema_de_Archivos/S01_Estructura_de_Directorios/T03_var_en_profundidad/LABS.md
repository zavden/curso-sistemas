# Labs — /var en profundidad

## Descripcion

Laboratorio practico para explorar /var y sus subdirectorios: logs,
cache, spool, lib y tmp. Comparar las diferencias de logs entre Debian
y AlmaLinux.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Estructura de /var | Subdirectorios y su proposito |
| 2 | /var/log | Logs del sistema, diferencias entre distros |
| 3 | /var/cache y /var/lib | Cache regenerable vs estado critico |
| 4 | /tmp vs /var/tmp | Temporales volatiles vs persistentes |

## Archivos

```
labs/
└── README.md    ← Guia paso a paso (documento principal del lab)
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~15 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.
