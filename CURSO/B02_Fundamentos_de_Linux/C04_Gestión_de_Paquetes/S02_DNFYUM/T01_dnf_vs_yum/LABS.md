# Labs — dnf vs yum

## Descripcion

Laboratorio practico para entender la evolucion yum → dnf: verificar
que yum es un symlink a dnf, comparar comandos, explorar la
configuracion de dnf, y entender libsolv.

## Prerequisitos

- Entorno del curso levantado (alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Evolucion yum → dnf | Symlink, versiones, transicion |
| 2 | Comandos equivalentes | Mismos comandos, diferencias sutiles |
| 3 | Configuracion de dnf | dnf.conf, opciones, plugins |

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
