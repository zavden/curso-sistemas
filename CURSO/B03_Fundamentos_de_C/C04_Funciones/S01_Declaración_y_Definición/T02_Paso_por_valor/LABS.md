# Labs -- Paso por valor

## Descripcion

Laboratorio para comprobar que C pasa todos los argumentos por valor (copia),
y practicar como simular paso por referencia usando punteros. Se explora el
comportamiento con escalares, structs y arrays.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Paso por valor | La funcion recibe una copia; modificarla no afecta al original |
| 2 | Simular paso por referencia | Pasar punteros para modificar el original (swap) |
| 3 | Structs por valor vs por puntero | Costo de copiar structs grandes, diferencia entre `.` y `->` |
| 4 | Arrays como argumentos | Los arrays decaen a punteros; la funcion modifica el original |

## Archivos

```
labs/
├── README.md        <- Ejercicios paso a paso
├── pass_by_value.c  <- Programa para la parte 1
├── swap.c           <- Swap incorrecto y correcto para la parte 2
├── struct_pass.c    <- Struct pequeno y grande para la parte 3
└── array_decay.c    <- Decaimiento de arrays para la parte 4
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~20 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| Ejecutables compilados | Binarios | Si (limpieza final) |
