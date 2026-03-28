# Labs — dpkg

## Descripcion

Laboratorio practico para dominar dpkg como herramienta de bajo
nivel: consultar paquetes instalados (-l, -L, -S, -s), inspeccionar
y extraer un .deb sin instalar (-I, -c, -x, ar x), y verificar la
integridad de archivos (dpkg -V, dpkg-query).

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Consultar paquetes | dpkg -l, -L, -S, -s, prefijos ii/rc/pn |
| 2 | Inspeccionar .deb | dpkg -I, -c, -x, ar x, data.tar/control.tar |
| 3 | Verificar y diagnosticar | dpkg -V, dpkg-query -W, reparacion |

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

~20 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.
