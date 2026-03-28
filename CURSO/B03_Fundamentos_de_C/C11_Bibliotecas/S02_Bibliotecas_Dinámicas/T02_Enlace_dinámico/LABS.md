# Labs -- Enlace dinamico

## Descripcion

Laboratorio para compilar un programa contra una biblioteca compartida (.so),
observar y resolver el error "cannot open shared object file", y dominar los
mecanismos de resolucion de bibliotecas en runtime: LD_LIBRARY_PATH, -rpath
(RUNPATH), ldconfig, ldd y LD_DEBUG.

## Prerequisitos

- GCC instalado (`gcc --version`)
- `ldd`, `readelf`, `objdump` disponibles
- Terminal con acceso al directorio `labs/`
- Acceso root solo para la parte 4 (ldconfig); el resto funciona sin root

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Compilar contra .so | Crear libmathops.so con -fPIC -shared, enlazar con -L -l, error "cannot open shared object file" |
| 2 | LD_LIBRARY_PATH | Resolver la carga indicando el directorio por variable de entorno |
| 3 | RUNPATH con -rpath | Embeber la ruta de busqueda en el binario con -Wl,-rpath y $ORIGIN |
| 4 | ldconfig y /etc/ld.so.conf | Instalar la biblioteca en el cache del sistema (requiere root) |
| 5 | Diagnostico con ldd y LD_DEBUG | Inspeccionar dependencias y trazar la busqueda del loader |
| 6 | Lazy binding vs immediate | Diferencia entre resolucion perezosa y LD_BIND_NOW |

## Archivos

```
labs/
├── README.md         <- Ejercicios paso a paso
├── mathops.h         <- Header de la biblioteca (add, mul)
├── mathops.c         <- Implementacion de la biblioteca
└── main_dynlink.c    <- Programa que usa libmathops.so
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~35 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| libmathops.so | Biblioteca compartida | Si (limpieza final) |
| main_dynlink, main_runpath | Ejecutables | Si (limpieza final) |
| libdir/ | Directorio auxiliar | Si (limpieza final) |
| /usr/local/lib/libmathops.so (parte 4) | Biblioteca instalada al sistema | Si (limpieza parte 4) |
| /etc/ld.so.conf.d/lab-mathops.conf (parte 4) | Configuracion ldconfig | Si (limpieza parte 4) |
