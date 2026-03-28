# Labs — Limpieza selectiva

## Descripcion

Laboratorio practico para aprender a limpiar recursos de Docker por tipo:
contenedores, imagenes, volumenes, build cache y redes, eligiendo
exactamente que eliminar en cada caso.

## Prerequisitos

- Docker instalado y funcionando
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Container prune | Limpiar contenedores detenidos |
| 2 | Image prune | Limpiar dangling y no usadas |
| 3 | Volume prune | Limpiar volumenes huerfanos (con precaucion) |
| 4 | Builder y network prune | Limpiar cache y redes |

## Archivos

```
labs/
├── README.md          ← Guia paso a paso (documento principal del lab)
└── Dockerfile.prune   ← Dockerfile simple para generar imagenes de prueba
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
| `lab-prune-*` | Contenedores | 1 | Si |
| `lab-prune` | Imagen | 2 | Si |
| `lab-prune-safe`, `lab-prune-important` | Volumenes | 3 | Si |
| `lab-prune-net` | Red | 4 | Si |

Ningun recurso del lab persiste despues de completar la limpieza.
