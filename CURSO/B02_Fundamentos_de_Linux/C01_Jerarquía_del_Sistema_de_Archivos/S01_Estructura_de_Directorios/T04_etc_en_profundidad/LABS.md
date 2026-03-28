# Labs — /etc en profundidad

## Descripcion

Laboratorio practico para explorar /etc: archivos de configuracion
fundamentales, el patron de directorios .d/, y comparar la
configuracion entre Debian y AlmaLinux.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Archivos fundamentales | hostname, hosts, passwd, resolv.conf |
| 2 | Patron .d/ | Configuracion modular, orden de carga |
| 3 | Comparar entre distros | Archivos especificos de cada familia |

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
