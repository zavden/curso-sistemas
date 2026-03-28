# Labs — chroot y pivot_root

## Descripcion

Laboratorio practico para entender chroot y pivot_root: cambiar
la raiz del filesystem, las limitaciones de seguridad de chroot,
y por que Docker usa pivot_root en lugar de chroot.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | chroot basico | Cambiar raiz, /proc visible, limitaciones |
| 2 | Limitaciones de chroot | No aisla PIDs/red/usuarios, escape posible |
| 3 | pivot_root y comparacion | Por que Docker usa pivot_root, tabla comparativa |

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
