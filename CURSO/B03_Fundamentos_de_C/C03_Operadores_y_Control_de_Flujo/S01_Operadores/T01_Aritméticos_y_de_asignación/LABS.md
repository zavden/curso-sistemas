# Labs -- Operadores aritmeticos y de asignacion

## Descripcion

Laboratorio para explorar el comportamiento de los operadores aritmeticos y de
asignacion en C: division entera con truncamiento, modulo con operandos
negativos, operadores compuestos, pre/post incremento, promocion de enteros en
expresiones mixtas, y precedencia con asociatividad.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Division entera y modulo | Truncamiento hacia cero, signo del modulo, invariante (a/b)*b + a%b == a |
| 2 | Asignacion compuesta e incremento | +=, -=, *=, /=, %=, diferencia entre ++x y x++ |
| 3 | Promocion y conversiones | char + char se promueve a int, expresiones mixtas int/double, signed vs unsigned |
| 4 | Precedencia y asociatividad | Orden de evaluacion, asociatividad izquierda/derecha, efecto de parentesis |

## Archivos

```
labs/
├── README.md          <- Ejercicios paso a paso
├── int_division.c     <- Division entera, modulo, invariante, conversion a horas
├── compound_assign.c  <- Operadores compuestos, pre/post incremento/decremento
├── promotion.c        <- Promocion de enteros, conversiones mixtas, signed/unsigned
└── precedence.c       <- Precedencia, asociatividad, expresiones complejas
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
