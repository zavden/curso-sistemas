# Labs — checkinstall

## Descripcion

Laboratorio practico para entender el problema de make install
(no deja registro), como checkinstall lo resuelve creando un
.deb/.rpm, DESTDIR para instalacion en directorio temporal, y
alternativas como fpm y dpkg-deb.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | El problema de make install | Sin registro, sin desinstalacion limpia |
| 2 | checkinstall | Crear .deb desde make install, verificar en dpkg |
| 3 | DESTDIR y alternativas | Instalacion temporal, fpm, dpkg-deb |

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
