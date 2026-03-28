# Labs — /usr merge

## Descripcion

Laboratorio practico para verificar el estado del /usr merge en ambas
distribuciones, demostrar que los paths tradicionales siguen funcionando,
y entender que /usr/local permanece separado.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Verificar el merge | Symlinks, inodos compartidos |
| 2 | Compatibilidad | Paths tradicionales siguen funcionando |
| 3 | /usr/local separado | No participa del merge |

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

~10 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.
