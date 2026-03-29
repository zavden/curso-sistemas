# Process substitution y named pipes

## 1. El problema: conectar procesos de formas no lineales

Los pipes Unix (`|`) conectan stdout de un proceso con stdin de otro en una
cadena **lineal**:

```
cmd1 | cmd2 | cmd3     ← lineal, un solo flujo
```

Pero muchos problemas requieren conexiones más complejas:
- Comparar la salida de **dos** comandos
- Alimentar un comando que espera **archivos**, no stdin
- Dividir un flujo en **múltiples** destinos
- Conectar procesos que no esperan pipes

Process substitution (`<()`, `>()`) y named pipes (`mkfifo`) resuelven estos
problemas.

---

## 2. Process substitution: <( ) y >( )

### 2.1 <( ) — Input substitution

`<(command)` crea un pseudo-archivo que contiene la salida del comando:

```bash
# diff espera DOS archivos. ¿Cómo comparar output de dos comandos?
diff <(ls /etc) <(ls /usr)

# Bash crea algo como:
# diff /dev/fd/63 /dev/fd/62
# Donde /dev/fd/63 contiene output de "ls /etc"
# y /dev/fd/62 contiene output de "ls /usr"
```

```
Sin process substitution:              Con process substitution:
ls /etc > /tmp/a.txt                   diff <(ls /etc) <(ls /usr)
ls /usr > /tmp/b.txt                   # una línea, sin archivos temporales
diff /tmp/a.txt /tmp/b.txt
rm /tmp/a.txt /tmp/b.txt
```

`<(cmd)` se reemplaza por un path a un file descriptor especial:

```bash
echo <(true)     # /dev/fd/63  (o similar)
echo <(true) <(true)  # /dev/fd/63 /dev/fd/62
```

### 2.2 >( ) — Output substitution

`>(command)` crea un pseudo-archivo donde puedes **escribir**, y lo escrito
se convierte en stdin del comando:

```bash
# Enviar output a un comando que procesa como archivo
echo "hello" > >(tr a-z A-Z)    # HELLO

# Logging: enviar stderr a un logger sin perder stdout
command 2> >(logger -t myapp)

# Equivalente a: command 2>&1 | logger -t myapp
# Pero sin redirigir stdout
```

---

## 3. Casos de uso de <( )

### 3.1 Comparar outputs de dos comandos

```bash
# Comparar paquetes instalados en dos servidores
diff <(ssh server1 'dpkg -l') <(ssh server2 'dpkg -l')

# Comparar archivos ordenados sin crear temporales
diff <(sort file1.txt) <(sort file2.txt)

# Comparar configuración antes y después
config_before=$(cat /etc/nginx/nginx.conf)
# ... hacer cambios ...
diff <(echo "$config_before") /etc/nginx/nginx.conf
```

### 3.2 Alimentar comandos que esperan archivos

```bash
# paste combina archivos línea por línea
# Combinar output de dos comandos como si fueran archivos
paste <(cut -d: -f1 /etc/passwd) <(cut -d: -f7 /etc/passwd)
# root    /bin/bash
# daemon  /usr/sbin/nologin
# ...

# join requiere archivos ordenados
join <(sort file1.csv) <(sort file2.csv)

# source un script generado dinámicamente
source <(generate_config)
```

### 3.3 Evitar subshell en while-read

Ya vimos esto en T01 (Funciones), pero aquí está la explicación completa:

```bash
count=0

# MAL: pipe crea subshell
cat file.txt | while read -r line; do
    (( count++ ))    # modifica copia en subshell
done
echo "$count"        # 0 (original no modificada)

# BIEN: process substitution, while en shell principal
while read -r line; do
    (( count++ ))    # modifica variable principal
done < <(cat file.txt)
echo "$count"        # valor correcto
```

```
Pipe (subshell):
  cat file.txt ─pipe─→ while read   ← subshell, cambios se pierden
                        (( count++ ))

Process substitution (mismo shell):
  while read            ← shell principal, cambios persisten
  (( count++ ))
    ↑
  <(cat file.txt) ─fd─→ stdin del while
```

### 3.4 Múltiples inputs simultáneos

```bash
# Leer de tres fuentes a la vez con paste
paste <(seq 1 5) <(seq 10 50 10) <(seq 100 500 100)
# 1   10   100
# 2   20   200
# 3   30   300
# 4   40   400
# 5   50   500

# Combinar headers + datos
paste -d',' <(echo "name"; cut -d: -f1 /etc/passwd | head -3) \
            <(echo "shell"; cut -d: -f7 /etc/passwd | head -3)
# name,shell
# root,/bin/bash
# daemon,/usr/sbin/nologin
# bin,/usr/sbin/nologin
```

---

## 4. Casos de uso de >( )

### 4.1 Tee a múltiples procesos

```bash
# Enviar output a stdout Y a un archivo comprimido
some_command | tee >(gzip > output.gz) | process_further

# Enviar a múltiples destinos
some_command | tee >(grep ERROR > errors.log) \
                   >(grep WARN > warnings.log) \
                   >(wc -l > count.txt) \
                   > /dev/null
```

```
                  ┌──→ grep ERROR > errors.log
some_command ──→ tee ──→ grep WARN > warnings.log
                  ├──→ wc -l > count.txt
                  └──→ /dev/null (descartar stdout principal)
```

### 4.2 Logging separado de stderr

```bash
# Stderr a un logger, stdout intacto
my_command 2> >(while read -r line; do
    echo "[$(date +%H:%M:%S)] $line" >> error.log
done)

# Más simple:
my_command 2> >(ts '[%H:%M:%S]' >> error.log)
```

### 4.3 Procesamiento en pipeline paralelo

```bash
# Calcular suma Y promedio del mismo flujo sin recorrer dos veces
generate_numbers | tee >(awk '{s+=$1} END{print "Sum:", s}') \
                      >(awk '{s+=$1; n++} END{print "Avg:", s/n}') \
                      > /dev/null
```

---

## 5. Named pipes (FIFO)

Un named pipe es un archivo especial en el filesystem que actúa como un
pipe Unix, pero con nombre persistente:

### 5.1 Crear y usar

```bash
# Crear
mkfifo /tmp/mypipe

# Terminal 1: escribir (se bloquea hasta que alguien lea)
echo "hello" > /tmp/mypipe

# Terminal 2: leer (se bloquea hasta que alguien escriba)
cat /tmp/mypipe    # hello

# Limpiar
rm /tmp/mypipe
```

```
Named pipe (FIFO):
  Escritor → /tmp/mypipe → Lector

  - Se bloquea el escritor si no hay lector
  - Se bloquea el lector si no hay escritor
  - Los datos fluyen directamente (no se almacenan en disco)
  - El archivo /tmp/mypipe solo es un "punto de encuentro"
```

### 5.2 Diferencia con archivos regulares

| Aspecto | Archivo regular | Named pipe |
|---------|----------------|------------|
| Almacenamiento | En disco | Solo en memoria (buffer del kernel) |
| Lectura | Múltiples veces | Una vez (datos desaparecen al leer) |
| Bloqueo | No | Sí (escritor espera lector y viceversa) |
| Tamaño en disco | Variable | 0 bytes siempre |
| `ls -l` | `-rw-r--r--` | `prw-r--r--` (la `p` indica pipe) |

### 5.3 Patrón con cleanup

```bash
#!/bin/bash
set -euo pipefail

PIPE=$(mktemp -u)    # -u = solo generar nombre, no crear archivo
mkfifo "$PIPE"
trap 'rm -f "$PIPE"' EXIT

# Productor en background
generate_data > "$PIPE" &

# Consumidor en foreground
while read -r line; do
    process "$line"
done < "$PIPE"

wait    # esperar al productor
```

---

## 6. Named pipes: comunicación entre scripts

### 6.1 Productor-consumidor

```bash
# producer.sh
#!/bin/bash
PIPE="${1:?Usage: producer.sh <pipe>}"

for i in {1..10}; do
    echo "item_$i"
    sleep 0.5
done > "$PIPE"
echo "Producer done" >&2
```

```bash
# consumer.sh
#!/bin/bash
PIPE="${1:?Usage: consumer.sh <pipe>}"

while read -r item; do
    echo "Processing: $item"
done < "$PIPE"
echo "Consumer done" >&2
```

```bash
# Ejecución:
mkfifo /tmp/work_pipe
./consumer.sh /tmp/work_pipe &    # lanzar consumidor (se bloquea esperando datos)
./producer.sh /tmp/work_pipe      # lanzar productor (envía datos)
rm /tmp/work_pipe

# Output:
# Processing: item_1
# Processing: item_2
# ...
# Processing: item_10
# Producer done
# Consumer done
```

### 6.2 Bidireccional con dos pipes

```bash
mkfifo /tmp/request_pipe /tmp/response_pipe
trap 'rm -f /tmp/request_pipe /tmp/response_pipe' EXIT

# Servidor: lee requests, envía responses
server() {
    while read -r request; do
        echo "Processing: $request" >&2
        echo "RESULT:${request^^}" > /tmp/response_pipe
    done < /tmp/request_pipe
}

server &
SERVER_PID=$!

# Cliente: envía requests, lee responses
for word in hello world foo; do
    echo "$word" > /tmp/request_pipe
    read -r response < /tmp/response_pipe
    echo "Got: $response"
done

kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null || true
```

---

## 7. Process substitution vs named pipes vs temp files

```
                 Process sub      Named pipe       Temp file
                 <(cmd)           mkfifo           mktemp
Crear            Automático       mkfifo path      mktemp
Limpiar          Automático       rm path          rm path
Reusar           No (un solo uso) Sí (persistente) Sí
Multiple readers No               Un lector a la vez Sí
Seekable         No               No               Sí (lseek)
Disk I/O         No               No               Sí
Script cruzado   No               Sí               Sí
Bloqueo          No*              Sí               No
Portabilidad     Bash/Zsh         POSIX            POSIX
```

**Cuándo usar cada uno**:
- **Process substitution**: comparar outputs, alimentar comandos con archivos, evitar subshell. Caso común, usar por defecto.
- **Named pipe**: comunicación entre scripts/procesos independientes, servidor-cliente.
- **Temp file**: datos que necesitan re-lectura, seek, o acceso desde múltiples procesos.

---

## 8. Pipelines complejos

### 8.1 Diamond pattern (split + merge)

```bash
# Procesar un flujo de dos formas diferentes y combinar
mkfifo /tmp/branch_a /tmp/branch_b
trap 'rm -f /tmp/branch_a /tmp/branch_b' EXIT

# Lanzar las dos ramas
grep "ERROR" < /tmp/branch_a | wc -l > /tmp/count_errors &
grep "WARN"  < /tmp/branch_b | wc -l > /tmp/count_warns &

# Alimentar ambas ramas desde el mismo input
tee /tmp/branch_a < logfile.txt > /tmp/branch_b

wait
echo "Errors: $(cat /tmp/count_errors)"
echo "Warnings: $(cat /tmp/count_warns)"
```

```
                 ┌──→ grep ERROR → wc -l → count_errors
logfile.txt → tee
                 └──→ grep WARN  → wc -l → count_warns
```

Con process substitution (más simple):

```bash
tee >(grep "ERROR" | wc -l > /tmp/count_errors) \
    < logfile.txt \
    | grep "WARN" | wc -l > /tmp/count_warns

wait
```

### 8.2 Diff de comandos remotos

```bash
# Comparar configuración entre dos servidores
diff --color \
    <(ssh prod-server 'cat /etc/nginx/nginx.conf') \
    <(ssh staging-server 'cat /etc/nginx/nginx.conf')

# Comparar listado de paquetes
comm -3 \
    <(ssh server1 'dpkg --get-selections | sort') \
    <(ssh server2 'dpkg --get-selections | sort')
```

### 8.3 Merge de múltiples logs ordenados por timestamp

```bash
sort -m \
    <(grep "2026-03-28" /var/log/app1.log) \
    <(grep "2026-03-28" /var/log/app2.log) \
    <(grep "2026-03-28" /var/log/app3.log)

# sort -m = merge de archivos ya ordenados (O(n), no O(n log n))
```

---

## 9. Heredoc y herestring como alternativas

A veces no necesitas process substitution — heredoc o herestring son
suficientes:

```bash
# Herestring: alimentar un string a stdin
grep "error" <<< "$log_content"

# Equivalente con process sub (overkill para strings):
grep "error" <(echo "$log_content")

# Heredoc: alimentar bloque multilínea
mysql mydb <<'EOF'
SELECT name, email
FROM users
WHERE active = 1;
EOF

# Heredoc como "archivo" para diff
diff /etc/hosts - <<'EOF'
127.0.0.1 localhost
::1       localhost
EOF
# - = leer de stdin (el heredoc)
```

### Cuándo usar cada uno

| Necesidad | Herramienta |
|-----------|------------|
| String en variable → stdin | Herestring: `<<< "$var"` |
| Bloque literal → stdin | Heredoc: `<<'EOF'` |
| Output de comando → como archivo | Process sub: `<(cmd)` |
| Dos outputs → comparar | Process sub: `diff <(cmd1) <(cmd2)` |
| Comunicación entre scripts | Named pipe: `mkfifo` |

---

## 10. Coproc: pipes bidireccionales (Bash 4.0+)

`coproc` lanza un proceso con pipes bidireccionales (stdin y stdout
conectados al shell):

```bash
# Lanzar un coprocess
coproc BC { bc -l; }

# Enviar datos al coprocess
echo "scale=4; 22/7" >&"${BC[1]}"

# Leer resultado del coprocess
read -r result <&"${BC[0]}"
echo "Pi ≈ $result"    # Pi ≈ 3.1428

# Más cálculos
echo "sqrt(2)" >&"${BC[1]}"
read -r result <&"${BC[0]}"
echo "√2 ≈ $result"   # √2 ≈ 1.4142

# Cerrar
kill "$BC_PID" 2>/dev/null
```

```
Shell ──write──→ BC[1] ──stdin──→ bc -l
Shell ←──read─── BC[0] ←─stdout── bc -l

BC es un array: [0]=fd de lectura, [1]=fd de escritura
BC_PID = PID del coprocess
```

### Coproc simple (sin nombre)

```bash
coproc tr a-z A-Z

echo "hello" >&"${COPROC[1]}"
exec {COPROC[1]}>&-              # cerrar escritura (señala EOF al coprocess)
read -r result <&"${COPROC[0]}"
echo "$result"                    # HELLO
```

**Limitación**: solo un coproc sin nombre a la vez. Los nombrados (`coproc NAME { cmd; }`)
permiten múltiples.

---

## 11. File descriptors manuales

Para control máximo sobre I/O, puedes manejar file descriptors directamente:

```bash
# Abrir fd 3 para lectura desde un proceso
exec 3< <(generate_data)

# Abrir fd 4 para escritura a un proceso
exec 4> >(process_data)

# Usar:
while read -r line <&3; do
    echo "processed: $line" >&4
done

# Cerrar:
exec 3<&-    # cerrar fd 3
exec 4>&-    # cerrar fd 4
```

### Intercambiar stdout y stderr

```bash
# Capturar stderr en variable, dejar stdout pasar
{ error=$(command 2>&1 1>&3-); } 3>&1
echo "Stderr was: $error"
```

### Redirect stderr a archivo sin perder stdout

```bash
exec 3>&2              # fd 3 = copia de stderr original
exec 2>/var/log/err    # stderr a archivo
echo "This goes to stdout"
echo "This goes to err file" >&2
exec 2>&3              # restaurar stderr
exec 3>&-              # cerrar fd 3
```

---

## 12. Errores comunes

| Error | Problema | Solución |
|-------|----------|----------|
| `<()` en `sh` o POSIX shell | Process substitution es Bash-only | Usar temp files en POSIX sh |
| Leer `<()` dos veces | El fd se agota en la primera lectura | Guardar en variable o temp file |
| Named pipe sin lector | Escritor se bloquea indefinidamente | Asegurar lector antes de escribir |
| Named pipe sin escritor | Lector se bloquea indefinidamente | Timeout: `read -t 5` |
| Olvidar `rm` de named pipe | Pipe queda en filesystem | `trap 'rm -f "$pipe"' EXIT` |
| `mkfifo` en directorio no-writable | Falla silenciosamente sin set -e | `mkfifo path \|\| exit 1` |
| Race condition con `>()` | El proceso puede no terminar antes de que continúe el script | `wait` después de usar `>()` |
| Coproc sin cerrar fd de escritura | El coprocess nunca ve EOF, se bloquea | `exec {COPROC[1]}>&-` |
| `diff <(cmd) file` con cmd lento | diff espera a que cmd termine | Normal, pero puede confundir |
| Asumir orden de `tee >()` | Los procesos en `>()` son asíncronos | `wait` o no depender del orden |

---

## 13. Ejercicios

### Ejercicio 1 — diff de dos comandos

**Predicción**: ¿qué muestra este comando?

```bash
diff <(echo -e "a\nb\nc\nd") <(echo -e "a\nc\nd\ne")
```

<details>
<summary>Respuesta</summary>

```
2d1
< b
4a4
> e
```

- Línea 2 del primer input (`b`) no está en el segundo → `< b`
- Línea 4 del segundo input (`e`) no está en el primero → `> e`

`diff` recibe dos "archivos" que en realidad son outputs de los `echo`.
Process substitution los presenta como `/dev/fd/63` y `/dev/fd/62`, que
`diff` abre y lee normalmente.

Sin process substitution, necesitarías dos archivos temporales.
</details>

---

### Ejercicio 2 — Evitar subshell

**Predicción**: ¿cuál es la diferencia?

```bash
# Versión A
total=0
cat numbers.txt | while read -r n; do
    (( total += n ))
done
echo "A: $total"

# Versión B
total=0
while read -r n; do
    (( total += n ))
done < <(cat numbers.txt)
echo "B: $total"
```

Si `numbers.txt` contiene:
```
10
20
30
```

<details>
<summary>Respuesta</summary>

```
A: 0
B: 60
```

- **Versión A**: el pipe crea un subshell para el `while`. `total` se modifica
  dentro del subshell (copia), pero la variable del shell principal queda en 0.

- **Versión B**: `< <(cat numbers.txt)` alimenta stdin del `while` que corre
  en el shell principal. `total` se modifica directamente. 10+20+30 = 60.

Nota: `< <(cat numbers.txt)` es equivalente a `< numbers.txt` en este caso.
La process substitution es útil cuando el input viene de un comando que no
es un simple archivo.
</details>

---

### Ejercicio 3 — Múltiples inputs con paste

**Predicción**: ¿qué imprime?

```bash
paste -d'|' <(echo -e "A\nB\nC") <(seq 1 3) <(echo -e "x\ny\nz")
```

<details>
<summary>Respuesta</summary>

```
A|1|x
B|2|y
C|3|z
```

`paste` combina líneas de múltiples "archivos" con el delimitador `|`.
Cada `<(...)` produce un pseudo-archivo:
- Archivo 1: A, B, C
- Archivo 2: 1, 2, 3
- Archivo 3: x, y, z

`paste` lee la primera línea de cada uno, las une con `|`, y repite.
Es como un `zip` de tres listas.
</details>

---

### Ejercicio 4 — tee con process substitution

**Predicción**: ¿qué archivos se crean y con qué contenido?

```bash
echo -e "INFO ok\nERROR fail\nINFO done\nERROR crash" | \
    tee >(grep ERROR > /tmp/errors.txt) \
        >(wc -l > /tmp/count.txt) \
    > /dev/null
sleep 0.1    # esperar a que los procesos en >() terminen
```

<details>
<summary>Respuesta</summary>

`/tmp/errors.txt`:
```
ERROR fail
ERROR crash
```

`/tmp/count.txt`:
```
4
```

`tee` envía su input a:
1. `>(grep ERROR > /tmp/errors.txt)` → filtra errores → 2 líneas
2. `>(wc -l > /tmp/count.txt)` → cuenta todas las líneas → 4
3. `> /dev/null` → stdout principal descartado

El `sleep 0.1` es necesario porque los procesos en `>()` son asíncronos
— pueden no haber terminado de escribir cuando el script continúa.
En scripts de producción, usar `wait` en su lugar.
</details>

---

### Ejercicio 5 — Named pipe bloqueo

**Predicción**: ¿qué pasa al ejecutar esto en una sola terminal?

```bash
mkfifo /tmp/test_pipe
echo "hello" > /tmp/test_pipe
echo "This line reached?"
```

<details>
<summary>Respuesta</summary>

El script se **bloquea** en la línea `echo "hello" > /tmp/test_pipe` y
"This line reached?" nunca se imprime.

Un named pipe bloquea al escritor hasta que un lector abre el otro extremo.
Como no hay otro proceso leyendo de `/tmp/test_pipe`, el `echo` espera
indefinidamente.

Para que funcione, necesitas un lector en otro proceso:

```bash
# Terminal 1:
echo "hello" > /tmp/test_pipe    # se bloquea esperando lector

# Terminal 2:
cat /tmp/test_pipe               # lee "hello", ambos se desbloquean
```

O en un solo script:
```bash
mkfifo /tmp/test_pipe
cat /tmp/test_pipe &    # lector en background
echo "hello" > /tmp/test_pipe    # ya no se bloquea
wait
echo "This line reached?"  # ✓
```
</details>

---

### Ejercicio 6 — Process substitution no reutilizable

**Predicción**: ¿qué imprime?

```bash
data=<(echo -e "line1\nline2\nline3")

head -1 "$data"
wc -l "$data"
```

<details>
<summary>Respuesta</summary>

```
line1
0
```

(O posiblemente `0 /dev/fd/63`.)

`head -1` lee la primera línea y cierra el fd. Cuando `wc -l` intenta leer
del mismo fd, ya está agotado (EOF). Resultado: 0 líneas.

Process substitution crea un **flujo unidireccional no-seekable**. Una vez
que los datos pasan, desaparecen. No puedes "rebobinar".

Solución: leer a variable o archivo temporal si necesitas acceso múltiple:
```bash
content=$(echo -e "line1\nline2\nline3")
head -1 <<< "$content"
wc -l <<< "$content"
```
</details>

---

### Ejercicio 7 — Coproc básico

**Predicción**: ¿qué imprime?

```bash
coproc UPPER { tr a-z A-Z; }

echo "hello" >&"${UPPER[1]}"
echo "world" >&"${UPPER[1]}"
exec {UPPER[1]}>&-             # cerrar escritura → EOF para tr

cat <&"${UPPER[0]}"
```

<details>
<summary>Respuesta</summary>

```
HELLO
WORLD
```

1. `coproc UPPER { tr a-z A-Z; }` — lanza `tr` con pipes bidireccionales
2. `echo "hello"` → escribe al fd de entrada de `tr`
3. `echo "world"` → escribe otra línea
4. `exec {UPPER[1]}>&-` → cierra la escritura → `tr` recibe EOF
5. `cat <&"${UPPER[0]}"` → lee todo el output de `tr`

Sin el `exec {UPPER[1]}>&-`, `tr` nunca vería EOF y `cat` se bloquearía
esperando más datos. Cerrar el fd de escritura es **esencial** para señalar
al coprocess que no habrá más input.
</details>

---

### Ejercicio 8 — Comunicación entre scripts con named pipe

**Predicción**: ¿cuál es el output si ejecutamos ambos scripts?

```bash
# writer.sh
#!/bin/bash
for i in {1..3}; do
    echo "msg_$i"
    sleep 0.3
done > /tmp/comm_pipe

# reader.sh
#!/bin/bash
while read -r msg; do
    echo "Received: $msg"
done < /tmp/comm_pipe
```

```bash
mkfifo /tmp/comm_pipe
./reader.sh &
./writer.sh
wait
rm /tmp/comm_pipe
```

<details>
<summary>Respuesta</summary>

```
Received: msg_1
Received: msg_2
Received: msg_3
```

Secuencia:
1. `mkfifo` crea el named pipe
2. `reader.sh` se lanza en background, abre `/tmp/comm_pipe` para lectura → se bloquea
3. `writer.sh` abre `/tmp/comm_pipe` para escritura → ambos se desbloquean
4. writer envía "msg_1" → reader imprime "Received: msg_1"
5. writer duerme 0.3s, luego envía "msg_2" → reader imprime
6. writer envía "msg_3" → reader imprime
7. writer termina, cierra el pipe → reader lee EOF → while termina
8. `wait` espera a que reader termine

Los datos fluyen en tiempo real: reader procesa cada mensaje conforme
llega, no espera a que writer termine. Es un streaming pipe.
</details>

---

### Ejercicio 9 — Diamond pattern

**Predicción**: ¿qué valores tienen count_a y count_b?

```bash
input="INFO ok
ERROR fail
INFO done
WARN maybe
ERROR crash
INFO start"

count_a=$(grep -c "ERROR" <(echo "$input"))
count_b=$(echo "$input" | tee >(grep -c "INFO" > /tmp/info_count) | grep -c "WARN")

echo "Errors: $count_a"
echo "Warns: $count_b"
sleep 0.1
echo "Infos: $(cat /tmp/info_count)"
```

<details>
<summary>Respuesta</summary>

```
Errors: 2
Warns: 1
Infos: 3
```

- `count_a`: `grep -c "ERROR"` sobre las 6 líneas → 2 matches (ERROR fail, ERROR crash)
- `count_b`: `tee` envía el input a `>(grep -c "INFO")` y a stdout. El stdout
  va al segundo `grep -c "WARN"` → 1 match (WARN maybe)
- `/tmp/info_count`: `grep -c "INFO"` dentro del `>()` cuenta 3 matches
  (INFO ok, INFO done, INFO start)

El `tee` divide el flujo: una copia va al process substitution para contar
INFOs, otra copia continúa por el pipe para contar WARNs.
</details>

---

### Ejercicio 10 — sort -m con process substitution

**Predicción**: ¿qué imprime?

```bash
sort -m <(echo -e "1\n3\n5\n7") <(echo -e "2\n4\n6\n8")
```

<details>
<summary>Respuesta</summary>

```
1
2
3
4
5
6
7
8
```

`sort -m` hace **merge** de archivos ya ordenados. No reordena internamente,
sino que intercala las líneas manteniendo el orden:

```
Archivo 1: 1, 3, 5, 7
Archivo 2: 2, 4, 6, 8

Merge:
  Comparar: 1 vs 2 → emitir 1
  Comparar: 3 vs 2 → emitir 2
  Comparar: 3 vs 4 → emitir 3
  Comparar: 5 vs 4 → emitir 4
  Comparar: 5 vs 6 → emitir 5
  ...
```

Es O(n) en vez de O(n log n) porque asume que los inputs ya están ordenados.
Process substitution permite hacer esto sin crear archivos temporales.

Uso real: mergear logs de múltiples servidores ordenados por timestamp.
</details>