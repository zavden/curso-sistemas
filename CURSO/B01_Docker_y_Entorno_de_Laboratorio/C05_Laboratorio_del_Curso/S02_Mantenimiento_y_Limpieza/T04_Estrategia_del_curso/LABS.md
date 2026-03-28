# Labs — Estrategia del curso

## Descripcion

Laboratorio practico para establecer la rutina de mantenimiento del
laboratorio del curso: exec vs run, ciclo diario, limpieza segura, y
verificar que el lab es resiliente a la limpieza.

## Prerequisitos

- Docker y Docker Compose instalados
- Entorno del curso levantado (compose.yml con ambos contenedores corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | exec vs run | Por que exec no acumula contenedores |
| 2 | Ciclo diario | up, exec, stop/start, down |
| 3 | Limpieza segura | Rutina que no afecta el lab activo |

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

**Nota**: este lab requiere que el entorno del curso este levantado
con Docker Compose.

## Tiempo estimado

~15 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Contenedores de `run` | Contenedor | 1 | Si |

Este lab usa los contenedores existentes del entorno del curso.
