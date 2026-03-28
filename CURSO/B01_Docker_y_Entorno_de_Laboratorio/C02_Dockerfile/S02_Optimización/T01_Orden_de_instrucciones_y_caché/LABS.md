# Labs — Orden de instrucciones y caché

## Descripción

Laboratorio práctico con 4 partes progresivas que demuestran cómo funciona la
caché de capas de Docker: reutilización de capas, invalidación en cascada,
impacto del orden de instrucciones, y rebuild forzado con `--no-cache`.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada:

```bash
docker pull alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | Caché en acción | Builds consecutivos reutilizan capas, `CACHED` en la salida |
| 2 | Invalidación en cascada | Un cambio en una capa invalida todas las posteriores |
| 3 | Orden correcto vs incorrecto | Impacto del orden de instrucciones en la eficiencia de la caché |
| 4 | --no-cache | Forzar rebuild completo, cuándo usarlo |

## Archivos

```
labs/
├── README.md              ← Guía paso a paso (documento principal del lab)
├── app.sh                 ← Script que simula código fuente (cambia frecuentemente)
├── deps.txt               ← Manifest de dependencias (cambia raramente)
├── Dockerfile.good-order  ← Orden correcto: dependencias primero, código después
├── Dockerfile.bad-order   ← Orden incorrecto: código primero, dependencias después
└── Dockerfile.cascade     ← Para demostrar invalidación en cascada
```

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
| `lab-cascade` | 2 | Sí |
| `lab-good-order` | 3 | Sí |
| `lab-bad-order` | 3 | Sí |

Ninguna imagen persiste después de completar el lab.
