# Labs — Init / PID 1

## Descripcion

Laboratorio practico para entender PID 1: inspeccionar el
proceso raiz, sus responsabilidades (reaping, senales,
inicializacion), y el comportamiento especial en contenedores.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Inspeccionar PID 1 | Que es, /proc/1, systemd |
| 2 | Reaping de huerfanos | PID 1 adopta y limpia |
| 3 | PID 1 en contenedores | Proteccion de senales, tini |

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
