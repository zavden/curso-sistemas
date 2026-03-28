# Labs — Monitoreo de espacio

## Descripcion

Laboratorio practico para aprender a medir el espacio que Docker consume
en el sistema, identificar los recursos que mas espacio ocupan, y
distinguir entre tamaño virtual y real de las imagenes.

## Prerequisitos

- Docker instalado y funcionando
- Algunas imagenes descargadas (al menos `alpine:latest`)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | docker system df | Resumen y modo verbose |
| 2 | Identificar consumidores | Dangling, huerfanos, build cache |
| 3 | Espacio real vs virtual | Capas compartidas, du vs docker |

## Archivos

```
labs/
├── README.md          ← Guia paso a paso (documento principal del lab)
└── Dockerfile.space   ← Dockerfile para generar consumo de espacio
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
| `lab-space-v1` | Imagen | 2 | Si |
| `lab-space-v2` | Imagen | 2 | Si |
| `lab-space-vol` | Contenedor | 2 | Si |
| `lab-space-data` | Volumen | 2 | Si |

Ningun recurso del lab persiste despues de completar la limpieza.
