# Bloque 20: Kubernetes y Observabilidad

**Objetivo**: Desplegar y operar aplicaciones en Kubernetes, instrumentar con
Prometheus/Grafana, y centralizar logs. Todo ejecutable en local (minikube/kind).

## Ubicación en la ruta de estudio

```
B01 (Docker) → ★ B20 ★ → B21 (Cloud)
```

### Prerequisitos

| Bloque | Aporta a B20 |
|--------|-------------|
| B01 (Docker) | Contenedores, imágenes, Dockerfile, registries |

### Recomendado

- **B09 (Redes)**: TCP/IP, DNS — útil para K8s networking e Ingress.
- **B18 (Scripting)**: Bash/Python para scripts de CI/CD y automation.
- **B19 (BD)**: StatefulSets con PostgreSQL, Redis en K8s, backups.

---

## Capítulo 1: Kubernetes Fundamentos [A]

### Sección 1: Arquitectura
- **T01 - Qué es Kubernetes**: orquestador de contenedores, desired state, reconciliation loop, breve historia
- **T02 - Componentes**: control plane (api-server, etcd, scheduler, controller-manager), nodo (kubelet, kube-proxy, container runtime)
- **T03 - Instalación local**: minikube, kind, k3d — cuándo usar cada uno, recursos mínimos
- **T04 - kubectl**: get, describe, apply, delete, logs, exec, port-forward, explain, cheat sheet

### Sección 2: Workloads
- **T01 - Pods**: spec, containers, initContainers, lifecycle hooks, restartPolicy, multi-container patterns
- **T02 - Deployments**: replicas, strategy (RollingUpdate, Recreate), rollout history, rollback
- **T03 - Services**: ClusterIP, NodePort, LoadBalancer, headless, DNS interno (svc.namespace.svc.cluster.local)
- **T04 - Namespaces**: aislamiento lógico, ResourceQuotas, LimitRanges, default namespace, kube-system

### Sección 3: Configuración y Storage
- **T01 - ConfigMaps**: create from literal/file, mount como volumen, inject como env vars, immutable
- **T02 - Secrets**: create, tipos (Opaque, TLS, docker-registry), mount, env, encryption at rest
- **T03 - Volumes**: emptyDir, hostPath, PersistentVolume, PersistentVolumeClaim, StorageClass, dynamic provisioning
- **T04 - Resource limits**: requests vs limits, CPU (millicores) / memory (Mi), QoS classes (Guaranteed, Burstable, BestEffort)

### Sección 4: Networking
- **T01 - Modelo de red**: pod-to-pod (flat network), CNI plugins overview (Calico, Flannel, Cilium)
- **T02 - Ingress**: ingress controller (nginx-ingress), rules, hosts, path routing, TLS termination
- **T03 - Network Policies**: default deny, allow specific traffic, egress rules, namespace isolation

## Capítulo 2: Kubernetes Aplicado [M]

### Sección 1: Helm
- **T01 - Qué es Helm**: package manager para K8s, charts, releases, repositories
- **T02 - Usar charts existentes**: helm repo add, install, upgrade, values.yaml overrides, --set
- **T03 - Crear un chart**: helm create, templates, _helpers.tpl, values, Go template syntax
- **T04 - Chart lifecycle**: versioning (Chart.yaml), dependencies, hooks (pre-install, post-upgrade), testing

### Sección 2: Despliegues Avanzados
- **T01 - StatefulSets**: stable network identity, ordered deployment, volumeClaimTemplates, uso: databases
- **T02 - DaemonSets**: un pod por nodo, node selectors, tolerations, uso: logging agents, monitoring
- **T03 - Jobs y CronJobs**: batch processing, backoffLimit, completions, parallelism, schedule cron syntax
- **T04 - Health checks**: livenessProbe (restart), readinessProbe (traffic), startupProbe (slow start), HTTP/TCP/exec

### Sección 3: Seguridad
- **T01 - RBAC**: Role, ClusterRole, RoleBinding, ClusterRoleBinding, service accounts, least privilege
- **T02 - Pod Security Standards**: restricted, baseline, privileged profiles, PodSecurity admission controller
- **T03 - Service Accounts**: default SA, token automount, IRSA (IAM Roles for Service Accounts en EKS)

### Sección 4: Operaciones
- **T01 - kubectl avanzado**: -o jsonpath, custom-columns, kubectl patch, diff, dry-run=client, kustomize
- **T02 - Debugging**: events, describe, logs --previous, ephemeral containers, kubectl debug, CrashLoopBackOff
- **T03 - Scaling**: HPA (CPU, memory, custom metrics), VPA overview, Cluster Autoscaler, KEDA overview
- **T04 - Deployment strategies**: rolling update tuning (maxSurge, maxUnavailable), blue-green, canary conceptos

## Capítulo 3: Prometheus y Grafana [M]

### Sección 1: Prometheus
- **T01 - Arquitectura**: pull-based scraping, TSDB, targets, service discovery, federation
- **T02 - Tipos de métricas**: counter, gauge, histogram, summary, naming conventions, labels
- **T03 - PromQL**: selectors, rate(), irate(), increase(), aggregations (sum, avg, by, without), recording rules
- **T04 - Alertmanager**: alert rules (YAML), routing tree, receivers (email, Slack, PagerDuty), silences, inhibitions

### Sección 2: Grafana
- **T01 - Instalación y data sources**: Docker/Helm, añadir Prometheus, Loki, PostgreSQL como fuentes
- **T02 - Dashboards**: panels (timeseries, stat, table, heatmap), variables, templating, row repeat
- **T03 - Alerting en Grafana**: unified alerting, alert rules, contact points, notification policies
- **T04 - Dashboards esenciales**: Node Exporter Full, Kubernetes cluster, PostgreSQL, Redis — importar desde grafana.com

### Sección 3: Instrumentación
- **T01 - Node Exporter**: métricas de sistema (CPU, RAM, disco, red), instalación, systemd unit, textfile collector
- **T02 - kube-state-metrics y cAdvisor**: métricas de objetos K8s, métricas de contenedores, despliegue
- **T03 - Instrumentar app en Rust**: prometheus crate, /metrics endpoint, counter/gauge/histogram, actix-web/axum integration
- **T04 - Instrumentar app en C**: libprom o endpoint HTTP manual, expose métricas, formato Prometheus text

## Capítulo 4: Logging y Trazabilidad [M]

### Sección 1: Centralización de Logs
- **T01 - Problema del logging distribuido**: N instancias, correlación, retención, búsqueda
- **T02 - Loki**: arquitectura (ingesters, queriers), labels (no indexa contenido), LogQL, push API
- **T03 - Promtail / Grafana Alloy**: agent de recolección, scrape configs, pipelines, relabeling, K8s autodiscovery
- **T04 - Grafana + Loki**: Explore, LogQL queries, correlación con métricas Prometheus, split-and-filter

### Sección 2: Stack ELK/EFK (overview)
- **T01 - Elasticsearch**: indexing, mapping, full-text search, por qué es potente y costoso
- **T02 - Fluent Bit**: lightweight log processor, input/filter/output, Kubernetes filter, multiline
- **T03 - Kibana**: Discover, dashboards, index patterns, Lens
- **T04 - ELK vs Loki**: cuándo usar cada stack, costos, complejidad operativa, escala, búsqueda full-text vs labels

### Sección 3: Trazabilidad (overview)
- **T01 - Distributed tracing**: qué es, spans, traces, context propagation, cuándo importa
- **T02 - OpenTelemetry**: collector, SDK (traces + metrics + logs), vendor-neutral, auto-instrumentation
- **T03 - Grafana Tempo / Jaeger**: backends de tracing, integración con Grafana, trace-to-logs
- **T04 - Los tres pilares**: metrics + logs + traces, correlación, observability vs monitoring, culture
