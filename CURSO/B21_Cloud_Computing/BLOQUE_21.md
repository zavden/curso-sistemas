# Bloque 21: Cloud Computing y Terraform

**Objetivo**: Entender el modelo de cloud computing, dominar los servicios core
de AWS, y gestionar infraestructura como código con Terraform. Bloque final de
la pista DevOps — requiere cuenta AWS (free tier cubre la mayoría de prácticas).

## Ubicación en la ruta de estudio

```
B09 (Redes) + B18 (Scripting) → ★ B21 ★
                                    ↑
                              Bloque final de la pista DevOps
```

### Prerequisitos

| Bloque | Aporta a B21 |
|--------|-------------|
| B09 (Redes) | TCP/IP, DNS, subnets, firewalls — base para VPC y networking cloud |
| B18 (Scripting) | Bash/Python para aws cli, Terraform provisioners, automation |

### Recomendado

- **B01 (Docker)**: concepto de contenedores para entender ECS/EKS.
- **B19 (BD)**: entender RDS, ElastiCache, backups gestionados.
- **B20 (K8s)**: para desplegar en EKS lo que ya sabes hacer en local.

---

## Capítulo 1: Fundamentos de Cloud [A]

### Sección 1: Conceptos
- **T01 - IaaS, PaaS, SaaS**: qué es cada modelo, ejemplos (EC2 vs Lambda vs Gmail), responsabilidades
- **T02 - Regiones y Availability Zones**: latencia, redundancia, disaster recovery, edge locations
- **T03 - Modelo de responsabilidad compartida**: qué gestiona AWS, qué gestiona el cliente, grey zones
- **T04 - Pricing**: on-demand, reserved, spot, savings plans, free tier, cost estimation con calculator

### Sección 2: Networking en la Nube
- **T01 - VPC**: subnets públicas/privadas, route tables, internet gateway, NAT gateway, CIDR
- **T02 - Security Groups y NACLs**: stateful vs stateless, reglas (inbound/outbound), best practices
- **T03 - DNS**: Route53, hosted zones, record types (A, CNAME, ALIAS), routing policies
- **T04 - Load Balancers**: ALB (HTTP/S), NLB (TCP), target groups, health checks, TLS termination

## Capítulo 2: AWS Core Services [A]

### Sección 1: Compute
- **T01 - EC2**: instance types (t3, m5, c5), AMIs, key pairs, user-data, instance lifecycle
- **T02 - Security Groups y SSH**: configurar acceso, bastion host, Session Manager (sin SSH)
- **T03 - EBS**: volume types (gp3, io2), snapshots, encryption, IOPS, throughput
- **T04 - Auto Scaling**: launch templates, ASG, scaling policies (target tracking, step, scheduled)

### Sección 2: Storage
- **T01 - S3**: buckets, objects, versioning, lifecycle policies, storage classes (Standard, IA, Glacier)
- **T02 - S3 permisos**: bucket policies, ACLs, presigned URLs, Block Public Access, encryption
- **T03 - EFS y FSx**: shared filesystem NFS, mount targets, performance modes, cuándo usar cada uno

### Sección 3: Identity y Security
- **T01 - IAM**: users, groups, roles, policy documents (JSON), least privilege, policy simulator
- **T02 - IAM Roles**: EC2 instance profiles, cross-account access, AssumeRole, trust policies
- **T03 - Secrets Manager y SSM Parameter Store**: almacenar secrets, rotación automática, acceso desde EC2/Lambda

### Sección 4: Otros Servicios Esenciales
- **T01 - RDS**: managed PostgreSQL/MySQL, Multi-AZ, read replicas, automated backups, Aurora overview
- **T02 - CloudWatch**: metrics, alarms, logs, dashboards, custom metrics, log insights
- **T03 - SNS y SQS**: notificaciones (email, SMS, HTTP), colas (standard, FIFO), dead-letter queues, fan-out
- **T04 - CLI y SDKs**: aws cli (configure, profiles, output formats), boto3 overview, credential chain

## Capítulo 3: Terraform [M]

### Sección 1: Fundamentos
- **T01 - IaC y por qué importa**: reproducibilidad, versionado (git), drift detection, vs ClickOps manual
- **T02 - HCL basics**: bloques (resource, variable, output), argumentos, tipos, interpolación
- **T03 - Providers**: aws provider, configuración, versiones, Terraform Registry
- **T04 - Workflow**: terraform init, plan, apply, destroy, ciclo de vida completo

### Sección 2: Recursos y Estado
- **T01 - Resources**: aws_instance, aws_s3_bucket, aws_vpc, lifecycle (create_before_destroy, ignore_changes)
- **T02 - State**: terraform.tfstate, remote state en S3 + DynamoDB (locking), state list/show
- **T03 - Data sources**: consultar infra existente (aws_ami, aws_vpc, aws_caller_identity)
- **T04 - Import**: terraform import (traer recursos existentes), state mv, state rm, refactoring

### Sección 3: Módulos y Organización
- **T01 - Módulos**: source (local, registry, git), inputs (variables), outputs, versioning
- **T02 - Estructura de proyecto**: environments/ (dev, staging, prod), modules/, variables.tf, outputs.tf
- **T03 - Variables**: variable blocks, types, validation rules, tfvars files, locals, sensitive
- **T04 - Workspaces**: terraform workspace new/select, state por workspace, vs directories separados

### Sección 4: Patrones Avanzados
- **T01 - for_each y count**: crear múltiples recursos, dynamic blocks, toset, tomap
- **T02 - Provisioners**: local-exec, remote-exec, por qué evitarlos (usar Ansible o cloud-init)
- **T03 - Backend remoto**: configurar S3 + DynamoDB paso a paso, encryption, IAM policy mínima
- **T04 - Terraform + Ansible**: TF provisiona infra, Ansible configura, local-exec trigger, workflow completo

## Capítulo 4: Prácticas Cloud [M]

### Sección 1: Despliegues
- **T01 - VPC completa**: subnets públicas/privadas, NAT gateway, route tables, todo en Terraform
- **T02 - EC2 + RDS**: app server + database, security groups cruzados, connection string
- **T03 - S3 static website**: bucket policy, CloudFront distribution, HTTPS con ACM
- **T04 - Auto Scaling + ALB**: launch template, ASG, target group, health checks, stress test

### Sección 2: Seguridad y Costos
- **T01 - Security audit**: IAM Access Analyzer, unused permissions, MFA enforcement, CloudTrail
- **T02 - Cost management**: Cost Explorer, budgets con alertas, tagging strategy, right-sizing
- **T03 - Well-Architected overview**: 6 pilares, trade-offs, review tool, decisiones informadas
