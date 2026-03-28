# Bloque 10: Servicios de Red

**Objetivo**: Instalar, configurar y administrar servicios de red esenciales
para LPIC-2 y RHCSA. Cada servicio se practica en ambas familias de distribuciones.

## Capítulo 1: SSH [A]

### Sección 1: Configuración Avanzada
- **T01 - sshd_config**: PermitRootLogin, PasswordAuthentication, AllowUsers, Port
- **T02 - Autenticación por clave**: ssh-keygen (RSA, Ed25519), authorized_keys, ssh-agent
- **T03 - SSH tunneling**: local (-L), remote (-R), dynamic (-D) port forwarding
- **T04 - SSH hardening**: fail2ban, rate limiting, configuración de cifrados
- **T05 - ~/.ssh/config**: Host aliases, ProxyJump, IdentityFile

## Capítulo 2: Servidor DNS (BIND) [A]

### Sección 1: Instalación y Configuración
- **T01 - Instalación**: paquetes en Debian vs RHEL, named.conf
- **T02 - Zonas**: zona forward, zona reversa, archivos de zona, registros SOA
- **T03 - Tipos de servidor**: caching-only, forwarder, authoritative
- **T04 - DNSSEC**: conceptos básicos, validación, firma de zonas

### Sección 2: Diagnóstico
- **T01 - named-checkconf y named-checkzone**: validar configuración
- **T02 - rndc**: control remoto del servidor DNS
- **T03 - Logs de BIND**: canales de logging, debugging

## Capítulo 3: Servidor Web [A]

### Sección 1: Apache HTTP Server
- **T01 - Instalación y estructura**: httpd.conf vs sites-available (Debian), conf.d (RHEL)
- **T02 - Virtual hosts**: name-based, configuración, DocumentRoot
- **T03 - Módulos**: mod_rewrite, mod_proxy, mod_ssl, habilitación/deshabilitación
- **T04 - .htaccess**: override, performance implications, alternativas

### Sección 2: Nginx
- **T01 - Instalación y estructura**: nginx.conf, server blocks, location
- **T02 - Proxy inverso**: proxy_pass, headers, WebSocket proxying
- **T03 - Servir archivos estáticos**: root vs alias, try_files, caching
- **T04 - Apache vs Nginx**: diferencias de arquitectura (prefork/worker vs event-driven)

### Sección 3: HTTPS y TLS
- **T01 - Certificados**: generación de CSR, self-signed para lab, Let's Encrypt concepto
- **T02 - Configuración TLS en Apache**: mod_ssl, SSLCertificateFile, cipher suites
- **T03 - Configuración TLS en Nginx**: ssl_certificate, ssl_protocols, HSTS
- **T04 - Protocolos web**: HTTP/1.1, HTTP/2, WebSocket — conceptos clave para WebAssembly

## Capítulo 4: Compartición de Archivos [A]

### Sección 1: NFS
- **T01 - Servidor NFS**: /etc/exports, exportfs, opciones (rw, sync, no_root_squash)
- **T02 - Cliente NFS**: montaje manual, fstab, autofs
- **T03 - NFSv4 vs NFSv3**: diferencias, Kerberos, id mapping

### Sección 2: Samba
- **T01 - Configuración de Samba**: smb.conf, shares, usuarios
- **T02 - Cliente SMB/CIFS**: smbclient, montaje con mount.cifs
- **T03 - Integración con usuarios Linux**: smbpasswd, mapping de permisos

## Capítulo 5: Otros Servicios [A]

### Sección 1: DHCP Server
- **T01 - ISC DHCP server**: dhcpd.conf, rangos, reservaciones, opciones
- **T02 - Diagnóstico**: dhcpd logs, tcpdump para ver DORA

### Sección 2: Email (Fundamentos)
- **T01 - Conceptos**: MTA, MDA, MUA, SMTP, POP3, IMAP — flujo completo
- **T02 - Postfix básico**: main.cf, configuración mínima, relay
- **T03 - mailq y logs**: diagnóstico de problemas de entrega

### Sección 3: PAM
- **T01 - Arquitectura PAM**: módulos, stacks (auth, account, password, session)
- **T02 - Archivos de configuración**: /etc/pam.d/, sintaxis, orden de módulos
- **T03 - Módulos comunes**: pam_unix, pam_deny, pam_limits, pam_wheel, pam_faillock

### Sección 4: LDAP Client
- **T01 - Conceptos de LDAP**: directorio, DN, base DN, LDIF, esquema, operaciones (bind, search, add, modify)
- **T02 - sssd**: instalación, configuración de cliente LDAP/AD, sssd.conf, dominios
- **T03 - Integración con NSS y PAM**: nsswitch.conf, pam_sss, autenticación centralizada
- **T04 - Herramientas de diagnóstico**: ldapsearch, sssctl, logs de sssd

### Sección 5: Proxy Forward
- **T01 - Concepto de proxy forward vs reverse**: diferencias, casos de uso corporativos
- **T02 - Squid básico**: instalación, squid.conf, ACLs, caché, autenticación
- **T03 - Configuración de clientes**: variables http_proxy/https_proxy, proxy en navegador, proxy transparente
