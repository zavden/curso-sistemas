# Bloque 19: Bases de Datos

**Objetivo**: Dominar SQL y la administración de PostgreSQL y Redis. Conectar
bases de datos desde C (libpq, sqlite3) y Rust (tokio-postgres, rusqlite)
para cerrar el ciclo con el resto del curso.

## Ubicación en la ruta de estudio

```
B02 → ★ B19 ★
  ↑         ↘
Linux    Recomendado antes de B20 (Cloud RDS)
```

### Prerequisitos

| Bloque | Aporta a B19 |
|--------|-------------|
| B02 (Linux) | Instalar servicios, systemd, permisos, filesystem |

### Recomendado

- **B09 (Redes)**: conexiones TCP, puertos, DNS — útil para acceso remoto a BD.
- **B03 + B05**: solo necesarios para C04 (SQLite/PostgreSQL desde código).

---

## Capítulo 1: Fundamentos de SQL [A]

### Sección 1: Modelo Relacional
- **T01 - Tablas, filas, columnas**: relaciones, atributos, tuplas, tipos de datos comunes
- **T02 - Claves primarias y foráneas**: PK, FK, constraints, integridad referencial, CASCADE
- **T03 - Normalización**: 1NF, 2NF, 3NF, cuándo desnormalizar, trade-offs

### Sección 2: DDL y DML
- **T01 - CREATE, ALTER, DROP**: tablas, tipos, constraints, IF EXISTS, comentarios
- **T02 - INSERT, UPDATE, DELETE**: RETURNING, ON CONFLICT (upsert), TRUNCATE, bulk insert
- **T03 - SELECT básico**: WHERE, ORDER BY, LIMIT/OFFSET, DISTINCT, LIKE, IN, BETWEEN
- **T04 - Funciones agregadas**: COUNT, SUM, AVG, MIN, MAX, GROUP BY, HAVING

### Sección 3: Consultas Avanzadas
- **T01 - JOINs**: INNER, LEFT, RIGHT, FULL, CROSS, self-join, múltiples JOINs
- **T02 - Subqueries**: IN, EXISTS, subquery en FROM, correlated subqueries, performance
- **T03 - CTEs y Window Functions**: WITH, ROW_NUMBER, RANK, PARTITION BY, LAG/LEAD
- **T04 - Views e índices**: CREATE VIEW, materialized views, CREATE INDEX, B-tree, cuándo indexar

### Sección 4: Transacciones y Concurrencia
- **T01 - ACID**: atomicidad, consistencia, aislamiento, durabilidad — qué garantiza cada una
- **T02 - BEGIN/COMMIT/ROLLBACK**: transacciones explícitas, savepoints, autocommit
- **T03 - Niveles de aislamiento**: READ COMMITTED, REPEATABLE READ, SERIALIZABLE, phantom reads
- **T04 - Deadlocks**: detección, prevención, timeouts, lock ordering

## Capítulo 2: PostgreSQL Administración [M]

### Sección 1: Instalación y Configuración
- **T01 - Instalación**: apt/dnf, Docker (postgres image), initdb, directorios de datos
- **T02 - Roles y autenticación**: CREATE ROLE, LOGIN, SUPERUSER, pg_hba.conf (peer, md5, scram)
- **T03 - Bases de datos y schemas**: CREATE DATABASE, schemas, search_path, template databases
- **T04 - Configuración**: postgresql.conf, listen_addresses, logging, reload vs restart

### Sección 2: Operaciones Cotidianas
- **T01 - psql**: \d, \dt, \l, \x, \copy, scripting con -f y -c, .psqlrc
- **T02 - Backup y restore**: pg_dump (plain, custom, directory), pg_restore, pg_dumpall, scheduling
- **T03 - VACUUM y ANALYZE**: autovacuum, dead tuples, pg_stat_user_tables, bloat, VACUUM FULL
- **T04 - Monitoreo**: pg_stat_activity (queries activas), pg_stat_user_tables, slow query log, pg_stat_statements

### Sección 3: Rendimiento
- **T01 - EXPLAIN y EXPLAIN ANALYZE**: seq scan vs index scan, cost, buffers, timing, nested loops
- **T02 - Tipos de índices**: B-tree, Hash, GIN (full-text), GiST (geoespacial), partial indexes
- **T03 - Connection pooling**: PgBouncer (transaction/session mode), por qué max_connections es un problema
- **T04 - Tuning básico**: shared_buffers, work_mem, effective_cache_size, checkpoint_completion_target

### Sección 4: Alta Disponibilidad
- **T01 - Streaming replication**: primary/standby, WAL shipping, pg_basebackup, synchronous vs async
- **T02 - Failover**: pg_promote, timeline, Patroni overview, health checks
- **T03 - Logical replication**: CREATE PUBLICATION/SUBSCRIPTION, casos de uso, limitaciones

## Capítulo 3: Redis [M]

### Sección 1: Fundamentos
- **T01 - Modelo de datos**: key-value, in-memory, single-threaded, por qué es rápido
- **T02 - Tipos de datos**: strings, lists, sets, sorted sets, hashes, streams
- **T03 - Comandos esenciales**: GET/SET, EXPIRE/TTL, KEYS vs SCAN, DEL, EXISTS
- **T04 - redis-cli**: connect, monitor, info, debug, --pipe para bulk import

### Sección 2: Patrones de Uso
- **T01 - Cache**: set con TTL, cache-aside pattern, write-through, invalidación, stampede
- **T02 - Contadores y rate limiting**: INCR, EXPIRE, sliding window con sorted sets
- **T03 - Pub/Sub**: SUBSCRIBE, PUBLISH, pattern subscribe, limitaciones (no persistencia)
- **T04 - Colas de trabajo**: LPUSH/BRPOP, reliable queue pattern, dead letter

### Sección 3: Persistencia y Operaciones
- **T01 - RDB snapshots**: SAVE/BGSAVE, configuración, ventajas (backup) y desventajas (pérdida)
- **T02 - AOF**: Append Only File, fsync policies (always/everysec/no), rewrite, AOF + RDB combo
- **T03 - Sentinel**: alta disponibilidad, failover automático, quorum, configuración
- **T04 - Redis en producción**: maxmemory, eviction policies, AUTH, security, monitoring

## Capítulo 4: Bases de Datos desde C y Rust [M]

### Sección 1: SQLite
- **T01 - SQLite como librería**: no es servidor, single-file, zero-config, cuándo usarlo
- **T02 - API de C**: sqlite3_open, sqlite3_prepare_v2, sqlite3_step, sqlite3_finalize, error handling
- **T03 - rusqlite en Rust**: Connection, execute, query_map, params![], transactions
- **T04 - Cuándo usar SQLite**: embedded apps, testing, prototyping, read-heavy, límites de concurrencia

### Sección 2: PostgreSQL desde Código
- **T01 - libpq en C**: PQconnectdb, PQexec, PQgetvalue, PQclear, prepared statements, connection string
- **T02 - tokio-postgres en Rust**: connect, query, execute, transactions, connection pooling (deadpool)
- **T03 - Migraciones**: concepto, sqlx migrate, schema versionado, up/down, CI integration
- **T04 - SQL injection prevention**: prepared statements siempre, nunca concatenar, ejemplos de ataques
