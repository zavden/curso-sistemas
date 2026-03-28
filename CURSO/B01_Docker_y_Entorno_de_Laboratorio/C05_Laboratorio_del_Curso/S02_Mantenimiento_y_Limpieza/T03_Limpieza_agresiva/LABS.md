# Labs — Limpieza agresiva

## Descripcion

Laboratorio practico para entender `docker system prune` y sus variantes:
que elimina cada opcion, como medir el impacto, y cuando es seguro
(o peligroso) usarlo.

## Prerequisitos

- Docker instalado y funcionando
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | docker system prune | Que elimina por defecto, que conserva |
| 2 | Variantes -a y --volumes | Impacto de cada flag |
| 3 | Reconstruir despues de prune | Medir el costo de rebuild |

## Archivos

```
labs/
├── README.md            ← Guia paso a paso (documento principal del lab)
└── Dockerfile.rebuild   ← Dockerfile para medir tiempos de rebuild
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
| `lab-aggressive-*` | Contenedores | 1 | Si |
| `lab-aggressive` | Imagen | 1 | Si |
| `lab-aggressive-data` | Volumen | 2 | Si |
| `lab-rebuild` | Imagen | 3 | Si |

Ningun recurso del lab persiste despues de completar la limpieza.
