# Labs — ACLs

## Descripcion

Laboratorio practico para usar Access Control Lists: establecer
permisos por usuario y grupo con setfacl, verificar con getfacl,
configurar ACLs por defecto en directorios, y entender la mascara
efectiva.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | ACLs basicas | setfacl, getfacl, el + en ls -la |
| 2 | ACLs por defecto | Herencia en directorios, propagacion |
| 3 | Mascara y limpieza | Mascara efectiva, eliminar ACLs |

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
