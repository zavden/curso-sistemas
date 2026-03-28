# Labs — Imagenes de confianza

## Descripcion

Laboratorio practico con 4 partes que ensena a evaluar la confiabilidad de
imagenes Docker: inspeccion de metadata, verificacion de firmas con Docker
Content Trust, escaneo de vulnerabilidades, y pull por digest para
reproducibilidad garantizada.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagenes descargadas:

```bash
docker pull nginx:latest
docker pull alpine:latest
```

- Acceso a Internet para consultas a Docker Hub
- Opcional: `docker scout` disponible para escaneo de vulnerabilidades

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Inspeccion de imagenes | Examinar metadata, labels, historial de capas y usuario configurado |
| 2 | Docker Content Trust | Verificar firmas de imagenes, habilitar/deshabilitar DCT |
| 3 | Escaneo de vulnerabilidades | Usar `docker scout` para detectar CVEs, comparar imagenes |
| 4 | Pull por digest | Garantizar reproducibilidad con hashes SHA256 en lugar de tags mutables |

## Archivos

```
labs/
└── README.md   <- Guia paso a paso (documento principal del lab)
```

Este laboratorio no requiere archivos de soporte. Todos los ejercicios
se realizan con comandos directos sobre imagenes publicas.

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte. Cada parte
incluye los comandos a ejecutar, las salidas esperadas, y el analisis de
resultados.

**Nota**: La Parte 3 (escaneo de vulnerabilidades) depende de que `docker
scout` este disponible. Si no lo esta, el lab proporciona las salidas
esperadas para analisis.

## Tiempo estimado

~20 minutos (las 4 partes completas).

## Recursos Docker creados

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `nginx:latest` | Imagen | 1, 2, 3 | No (imagen publica ya presente) |
| `alpine:latest` | Imagen | 1, 3 | No (imagen publica ya presente) |

Este laboratorio no crea imagenes custom ni contenedores de larga
duracion. Las imagenes publicas descargadas se mantienen como
referencia para otros laboratorios.
