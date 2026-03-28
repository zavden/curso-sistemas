# Labs — .dockerignore

## Descripción

Laboratorio práctico con 4 partes progresivas que demuestran el impacto de
`.dockerignore`: tamaño del contexto de build, protección contra secrets en
la imagen, el operador `!` para excepciones, e invalidación de caché causada
por archivos innecesarios como `.git/`.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada:

```bash
docker pull alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | Contexto de build | Impacto de archivos grandes en el tamaño del contexto |
| 2 | Secrets en la imagen | `.env` y credenciales incluidas sin .dockerignore |
| 3 | Operador ! (excepciones) | Excluir archivos y re-incluir excepciones |
| 4 | Caché e invalidación | `.git/` causa invalidación innecesaria de caché |

## Archivos

```
labs/
├── README.md          ← Guía paso a paso (documento principal del lab)
├── app.py             ← Script Python simple (simula código fuente)
└── Dockerfile.context ← Dockerfile básico con COPY . (para todas las partes)
```

Los archivos de prueba (`.env`, `node_modules/`, `.git/`, etc.) se crean
durante el lab y se eliminan en la limpieza.

## Cómo ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~20 minutos (las 4 partes completas).

## Imágenes Docker que se crean

| Imagen | Parte | Se elimina al final |
|---|---|---|
| `lab-no-ignore` | 1 | Sí |
| `lab-with-ignore` | 1 | Sí |
| `lab-leak` | 2 | Sí |
| `lab-safe` | 2 | Sí |
| `lab-except` | 3 | Sí |
| `lab-cache-test` | 4 | Sí |

Ninguna imagen persiste después de completar el lab.
