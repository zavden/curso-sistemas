# Labs — ARG vs ENV

## Descripción

Laboratorio práctico con 5 partes progresivas que demuestran las diferencias
entre ARG y ENV: ciclo de vida (build vs runtime), scope de ARG en multi-stage
y antes de FROM, el patrón ARG+ENV para persistir valores de build, y las
implicaciones de seguridad de usar ARG para secrets.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada:

```bash
docker pull alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | ARG: solo en build | ARG disponible durante build pero no en runtime, `--build-arg` para sobrescribir |
| 2 | ENV: persiste en runtime | ENV disponible en build y runtime, sobrescribir con `-e` |
| 3 | ARG + ENV combo | Patrón para persistir valores de build en runtime |
| 4 | Scope de ARG | ARG antes de FROM, scope por stage, re-declaración |
| 5 | Seguridad de ARG | Valores de ARG visibles en `docker history` |

## Archivos

```
labs/
├── README.md                 ← Guía paso a paso (documento principal del lab)
├── Dockerfile.arg-only       ← ARG solo, no persiste en runtime (Parte 1)
├── Dockerfile.env-only       ← ENV persiste en runtime (Parte 2)
├── Dockerfile.arg-env        ← Patrón ARG + ENV combo (Parte 3)
├── Dockerfile.arg-before-from ← ARG antes de FROM (Parte 4)
├── Dockerfile.arg-scope      ← Scope de ARG en multi-stage (Parte 4)
└── Dockerfile.arg-security   ← ARG visible en docker history (Parte 5)
```

## Cómo ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~20 minutos (las 5 partes completas).

## Imágenes Docker que se crean

| Imagen | Parte | Se elimina al final |
|---|---|---|
| `lab-arg-only` | 1 | Sí |
| `lab-arg-custom` | 1 | Sí |
| `lab-env-only` | 2 | Sí |
| `lab-arg-env` | 3 | Sí |
| `lab-arg-before-from` | 4 | Sí |
| `lab-arg-scope` | 4 | Sí |
| `lab-arg-security` | 5 | Sí |

Ninguna imagen persiste después de completar el lab. La limpieza se hace
al final de cada parte y en la sección de limpieza final.
