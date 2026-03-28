# Labs — Dockerfile Alma dev

## Descripcion

Laboratorio practico para construir la imagen de desarrollo AlmaLinux 9
del curso, comparar las diferencias de paquetes con Debian, y verificar
paridad funcional entre ambas distribuciones.

## Prerequisitos

- Docker instalado y funcionando
- Conexion a internet (para descargar paquetes y Rust)
- Haber completado el lab de T01 (Dockerfile Debian dev) o tener la imagen `lab-debian-dev` disponible
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Diferencias con Debian | Paquetes, EPEL, dnf vs apt |
| 2 | Construir la imagen | Build, EPEL, tamano |
| 3 | Verificar herramientas | Compiladores, debugging, Rust |
| 4 | Paridad funcional | Mismo programa en ambas distros |

## Archivos

```
labs/
├── README.md            ← Guia paso a paso (documento principal del lab)
└── Dockerfile.alma-dev  ← Dockerfile completo de la imagen AlmaLinux dev
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~20 minutos (sin contar el tiempo de build).

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `lab-alma-dev` | Imagen | 2 | Si |
| `lab-debian-dev` | Imagen | 4 | Si (si no existia previamente) |
| Contenedores temporales | Contenedor | 3-4 | Si (--rm) |

Ningun recurso del lab persiste despues de completar la limpieza.
