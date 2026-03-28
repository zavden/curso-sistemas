# Labs — rwx y notación octal

## Descripcion

Laboratorio practico para interpretar permisos en formato simbolico
y octal, entender la diferencia entre permisos en archivos y
directorios, y verificar como el kernel evalua los permisos.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Interpretar permisos | ls -la, stat, tipo de archivo |
| 2 | Notacion octal | Conversion simbolica-octal, permisos comunes |
| 3 | Permisos en directorios | r vs x, w sin x, eliminacion sin ownership |

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
