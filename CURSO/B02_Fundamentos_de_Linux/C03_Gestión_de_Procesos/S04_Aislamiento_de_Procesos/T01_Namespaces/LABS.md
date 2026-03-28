# Labs — Namespaces

## Descripcion

Laboratorio practico para explorar namespaces de Linux: los 8
tipos, /proc/[pid]/ns/, lsns, y comparar los namespaces del
contenedor con los conceptos del host.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Ver namespaces | /proc/self/ns/, lsns, inode IDs |
| 2 | Los 8 tipos | Que aisla cada uno, como verificar |
| 3 | Namespaces y contenedores | PID namespace, aislamiento observable |

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
