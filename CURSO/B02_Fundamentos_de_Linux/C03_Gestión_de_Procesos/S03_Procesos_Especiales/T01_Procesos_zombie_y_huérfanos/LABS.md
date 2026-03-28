# Labs — Procesos zombie y huerfanos

## Descripcion

Laboratorio practico para crear y observar procesos zombie y
huerfanos: identificar zombies con ps, entender re-parenting a
PID 1, y diagnosticar padres que no hacen wait().

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Procesos zombie | Crear zombie, identificar con ps, por que no se puede matar |
| 2 | Procesos huerfanos | Re-parenting a PID 1, proceso normal |
| 3 | Diagnostico | Contar zombies, identificar padre, solucionar |

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
