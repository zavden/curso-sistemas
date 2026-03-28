# Labs — Dockerfile Debian dev

## Descripcion

Laboratorio practico para construir la imagen de desarrollo Debian del
curso, verificar que todas las herramientas estan instaladas, y entender
cada decision del Dockerfile.

## Prerequisitos

- Docker instalado y funcionando
- Conexion a internet (para descargar paquetes y Rust)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Examinar el Dockerfile | Cada seccion y su proposito |
| 2 | Construir la imagen | Build, capas, tamano |
| 3 | Verificar herramientas | Compiladores, debugging, Rust, man pages |
| 4 | SYS_PTRACE | Por que gdb/strace necesitan esta capability |

## Archivos

```
labs/
├── README.md              ← Guia paso a paso (documento principal del lab)
└── Dockerfile.debian-dev  ← Dockerfile completo de la imagen Debian dev
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~20 minutos (sin contar el tiempo de build, que depende de la conexion).

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `lab-debian-dev` | Imagen | 2 | Si |
| Contenedores temporales | Contenedor | 3-4 | Si (--rm) |

Ningun recurso del lab persiste despues de completar la limpieza.
