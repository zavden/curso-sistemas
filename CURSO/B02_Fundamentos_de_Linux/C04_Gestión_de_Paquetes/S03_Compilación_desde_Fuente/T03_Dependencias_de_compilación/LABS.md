# Labs — Dependencias de compilacion

## Descripcion

Laboratorio practico para entender la diferencia entre paquetes
de desarrollo (-dev/-devel) y runtime, los meta-paquetes de
compilacion (build-essential, Development Tools), pkg-config, y
apt build-dep / dnf builddep.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Paquetes -dev vs runtime | Headers, libs estaticas, convenciones |
| 2 | Toolchains base | build-essential, Development Tools |
| 3 | pkg-config y build-dep | Buscar flags, instalar deps automaticamente |

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
