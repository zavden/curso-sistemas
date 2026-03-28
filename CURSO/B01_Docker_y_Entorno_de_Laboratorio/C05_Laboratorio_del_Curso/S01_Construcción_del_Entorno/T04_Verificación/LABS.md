# Labs — Verificacion

## Descripcion

Laboratorio practico para crear, ejecutar y entender el script de
verificacion del entorno del curso: aprender a leer la salida, provocar
fallos intencionalmente, y diagnosticar problemas comunes.

## Prerequisitos

- Docker y Docker Compose instalados
- Entorno del curso levantado (compose.yml del T03 con ambos contenedores corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Examinar el script | Estructura, funciones check/check_output |
| 2 | Ejecutar la verificacion | Salida esperada, interpretar resultados |
| 3 | Provocar fallos | Diagnosticar SYS_PTRACE, Rust, volumenes |

## Archivos

```
labs/
├── README.md        ← Guia paso a paso (documento principal del lab)
└── verify-env.sh    ← Script de verificacion del entorno
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

**Nota**: este lab requiere que el entorno del curso (T03) este
levantado. El script se ejecuta desde el directorio donde esta
el compose.yml.

## Tiempo estimado

~15 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno nuevo | — | — | — |

Este lab no crea recursos propios. Usa los contenedores del
compose.yml del T03.
