# Estado compartido: Arc, Mutex y gestión de clientes

## Índice

1. [El problema del estado compartido](#el-problema-del-estado-compartido)
2. [Arc y Mutex en contexto async](#arc-y-mutex-en-contexto-async)
3. [Diseño del estado del servidor](#diseño-del-estado-del-servidor)
4. [Registro de clientes](#registro-de-clientes)
5. [Mensajes privados](#mensajes-privados)
6. [tokio::sync::Mutex vs std::sync::Mutex](#tokiosyncmutex-vs-stdsyncmutex)
7. [Alternativa: canales en vez de Mutex](#alternativa-canales-en-vez-de-mutex)
8. [El servidor completo con estado](#el-servidor-completo-con-estado)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## El problema del estado compartido

En T01 y T02, cada task de cliente corre independientemente. La única
comunicación es el broadcast channel. Pero para funcionalidades como
`/list`, `/msg` privados, y verificación de nicknames únicos, las tasks
necesitan **acceso a un estado global**: la lista de clientes conectados.

```
┌──────────────────────────────────────────────────────────────┐
│  ¿Qué estado necesita compartirse?                           │
│                                                              │
│  Task A (Alice)     Estado compartido      Task B (Bob)      │
│  ─────────────      ──────────────────     ────────────      │
│                     ┌────────────────┐                       │
│  /list ────────────▶│ Clientes:      │◀──────── /list        │
│                     │  Alice → tx_a  │                       │
│  /nick Ally ───────▶│  Bob   → tx_b  │                       │
│  ¿"Ally" libre? ◀──│  Carol → tx_c  │                       │
│                     └────────────────┘                       │
│  /msg Bob Hi ──────▶    │                                    │
│                    tx_b.send("Hi") ────────────▶ Bob recibe  │
│                                                              │
│  Sin estado compartido, estas operaciones son imposibles     │
└──────────────────────────────────────────────────────────────┘
```

---

## Arc y Mutex en contexto async

### Recordatorio: por qué Arc + Mutex

```
┌──────────────────────────────────────────────────────────────┐
│  Arc<T>                                                      │
│  ─ Reference-counted pointer (thread-safe)                   │
│  ─ Permite que múltiples owners compartan el mismo dato      │
│  ─ Clone es barato (incrementa un contador atómico)          │
│                                                              │
│  Mutex<T>                                                    │
│  ─ Mutual exclusion: solo un thread/task a la vez            │
│  ─ .lock() bloquea hasta obtener acceso exclusivo            │
│  ─ El MutexGuard se libera al salir del scope                │
│                                                              │
│  Arc<Mutex<T>>                                               │
│  ─ Múltiples owners + acceso exclusivo                       │
│  ─ Patrón estándar para estado compartido en Rust            │
│                                                              │
│      Task A                Task B                            │
│        │                     │                               │
│        ▼                     ▼                               │
│   Arc::clone(&state)    Arc::clone(&state)                   │
│        │                     │                               │
│        ▼                     ▼                               │
│   state.lock()          state.lock()                         │
│        │                     │ (espera)                      │
│   ┌────▼─────┐               │                              │
│   │ MutexGuard│               │                              │
│   │ (acceso)  │               │                              │
│   └────┬─────┘               │                              │
│        │ drop                 │                              │
│        └─────────────────────▼                               │
│                          ┌────▼─────┐                       │
│                          │ MutexGuard│                       │
│                          └──────────┘                       │
└──────────────────────────────────────────────────────────────┘
```

### El patrón en async

```rust
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::Mutex;

// Tipo del estado compartido
type SharedState = Arc<Mutex<ServerState>>;

// Crear el estado
let state: SharedState = Arc::new(Mutex::new(ServerState::new()));

// Clonar para cada task (Arc::clone es barato)
let state_clone = state.clone();
tokio::spawn(async move {
    // Acceder al estado
    let mut guard = state_clone.lock().await;
    guard.add_client("Alice");
    // guard se libera aquí (drop)
});
```

---

## Diseño del estado del servidor

### Estructura del estado

```rust
use std::collections::HashMap;
use std::net::SocketAddr;
use tokio::sync::mpsc;

/// Information about a connected client
#[derive(Debug)]
pub struct ClientInfo {
    /// The client's current nickname
    pub nickname: String,

    /// Channel to send messages directly to this client
    pub direct_tx: mpsc::UnboundedSender<String>,

    /// When the client connected
    pub connected_at: std::time::Instant,
}

/// Shared server state
pub struct ServerState {
    /// Connected clients, indexed by socket address
    clients: HashMap<SocketAddr, ClientInfo>,

    /// Reverse lookup: nickname → socket address
    nick_to_addr: HashMap<String, SocketAddr>,
}

impl ServerState {
    pub fn new() -> Self {
        ServerState {
            clients: HashMap::new(),
            nick_to_addr: HashMap::new(),
        }
    }

    /// Number of connected clients
    pub fn client_count(&self) -> usize {
        self.clients.len()
    }
}
```

### Por qué un canal directo por cliente

Además del broadcast channel (para mensajes a todos), cada cliente tiene
un `mpsc::UnboundedSender` para mensajes dirigidos solo a él:

```
┌──────────────────────────────────────────────────────────────┐
│  Dos canales de comunicación                                 │
│                                                              │
│  broadcast::channel ──────── Mensajes para TODOS             │
│  │                           [Alice] Hello!                   │
│  │                           *** Bob joined ***              │
│  ▼                                                           │
│  Todos los rx reciben                                        │
│                                                              │
│  mpsc por cliente ─────────── Mensajes para UNO              │
│  direct_tx_alice ──▶ Alice   /list response                  │
│  direct_tx_bob   ──▶ Bob     /msg from Alice                 │
│                              Error messages                  │
│                              Server notices                  │
│                                                              │
│  ¿Por qué no solo broadcast?                                │
│  ─ /msg privado: no queremos que todos lo vean               │
│  ─ /list response: solo el que pidió necesita verlo          │
│  ─ Errores: solo el cliente que causó el error               │
└──────────────────────────────────────────────────────────────┘
```

---

## Registro de clientes

### Operaciones CRUD sobre el estado

```rust
use std::net::SocketAddr;
use tokio::sync::mpsc;

impl ServerState {
    /// Register a new client. Returns false if nickname is taken.
    pub fn add_client(
        &mut self,
        addr: SocketAddr,
        nickname: String,
        direct_tx: mpsc::UnboundedSender<String>,
    ) -> bool {
        // Check if nickname is already in use
        if self.nick_to_addr.contains_key(&nickname) {
            return false;
        }

        self.nick_to_addr.insert(nickname.clone(), addr);
        self.clients.insert(addr, ClientInfo {
            nickname,
            direct_tx,
            connected_at: std::time::Instant::now(),
        });

        true
    }

    /// Remove a client when they disconnect
    pub fn remove_client(&mut self, addr: &SocketAddr) -> Option<String> {
        if let Some(info) = self.clients.remove(addr) {
            self.nick_to_addr.remove(&info.nickname);
            Some(info.nickname)
        } else {
            None
        }
    }

    /// Change a client's nickname. Returns false if new nick is taken.
    pub fn change_nickname(
        &mut self,
        addr: &SocketAddr,
        new_nick: String,
    ) -> Result<String, ChatError> {
        // Check if new nickname is already in use
        if let Some(&existing_addr) = self.nick_to_addr.get(&new_nick) {
            if existing_addr != *addr {
                return Err(ChatError::NicknameTaken(new_nick));
            }
            // Same user, same nick — no-op
            return Ok(new_nick);
        }

        let client = self.clients.get_mut(addr)
            .ok_or(ChatError::Disconnected)?;

        let old_nick = client.nickname.clone();

        // Update both maps
        self.nick_to_addr.remove(&old_nick);
        self.nick_to_addr.insert(new_nick.clone(), *addr);
        client.nickname = new_nick.clone();

        Ok(old_nick)
    }

    /// Check if a nickname is available
    pub fn is_nickname_available(&self, nick: &str) -> bool {
        !self.nick_to_addr.contains_key(nick)
    }

    /// Get list of all connected nicknames
    pub fn list_nicknames(&self) -> Vec<String> {
        let mut nicks: Vec<String> = self.clients.values()
            .map(|c| c.nickname.clone())
            .collect();
        nicks.sort();
        nicks
    }

    /// Get the nickname for an address
    pub fn get_nickname(&self, addr: &SocketAddr) -> Option<&str> {
        self.clients.get(addr).map(|c| c.nickname.as_str())
    }

    /// Get the direct sender for a specific nickname
    pub fn get_direct_sender(&self, nickname: &str) -> Option<&mpsc::UnboundedSender<String>> {
        let addr = self.nick_to_addr.get(nickname)?;
        self.clients.get(addr).map(|c| &c.direct_tx)
    }
}
```

### Tests del estado

```rust
#[cfg(test)]
mod tests {
    use super::*;
    use tokio::sync::mpsc;

    fn make_tx() -> mpsc::UnboundedSender<String> {
        let (tx, _rx) = mpsc::unbounded_channel();
        tx
    }

    fn addr(port: u16) -> SocketAddr {
        use std::net::{IpAddr, Ipv4Addr};
        SocketAddr::new(IpAddr::V4(Ipv4Addr::LOCALHOST), port)
    }

    #[test]
    fn test_add_client() {
        let mut state = ServerState::new();
        assert!(state.add_client(addr(1), "Alice".into(), make_tx()));
        assert_eq!(state.client_count(), 1);
    }

    #[test]
    fn test_add_duplicate_nickname() {
        let mut state = ServerState::new();
        assert!(state.add_client(addr(1), "Alice".into(), make_tx()));
        assert!(!state.add_client(addr(2), "Alice".into(), make_tx()));
        assert_eq!(state.client_count(), 1);
    }

    #[test]
    fn test_remove_client() {
        let mut state = ServerState::new();
        state.add_client(addr(1), "Alice".into(), make_tx());
        let nick = state.remove_client(&addr(1));
        assert_eq!(nick, Some("Alice".to_string()));
        assert_eq!(state.client_count(), 0);
        // Nickname is available again
        assert!(state.is_nickname_available("Alice"));
    }

    #[test]
    fn test_change_nickname() {
        let mut state = ServerState::new();
        state.add_client(addr(1), "Alice".into(), make_tx());

        let old = state.change_nickname(&addr(1), "Ally".into()).unwrap();
        assert_eq!(old, "Alice");
        assert!(state.is_nickname_available("Alice"));
        assert!(!state.is_nickname_available("Ally"));
    }

    #[test]
    fn test_change_nickname_taken() {
        let mut state = ServerState::new();
        state.add_client(addr(1), "Alice".into(), make_tx());
        state.add_client(addr(2), "Bob".into(), make_tx());

        let result = state.change_nickname(&addr(1), "Bob".into());
        assert!(result.is_err());
    }

    #[test]
    fn test_list_nicknames() {
        let mut state = ServerState::new();
        state.add_client(addr(1), "Charlie".into(), make_tx());
        state.add_client(addr(2), "Alice".into(), make_tx());
        state.add_client(addr(3), "Bob".into(), make_tx());

        let list = state.list_nicknames();
        assert_eq!(list, vec!["Alice", "Bob", "Charlie"]);  // Sorted
    }

    #[test]
    fn test_get_direct_sender() {
        let mut state = ServerState::new();
        let (tx, mut rx) = mpsc::unbounded_channel();
        state.add_client(addr(1), "Alice".into(), tx);

        // Send via the stored sender
        let sender = state.get_direct_sender("Alice").unwrap();
        sender.send("Hello direct".into()).unwrap();

        // Verify the receiver gets it
        let msg = rx.try_recv().unwrap();
        assert_eq!(msg, "Hello direct");
    }
}
```

---

## Mensajes privados

### Implementar /msg con el estado compartido

```rust
use std::sync::Arc;
use tokio::sync::Mutex;

type SharedState = Arc<Mutex<ServerState>>;

/// Send a private message from one user to another
pub async fn send_private_message(
    state: &SharedState,
    from_nick: &str,
    to_nick: &str,
    content: &str,
) -> Result<(), ChatError> {
    let state = state.lock().await;

    let sender = state.get_direct_sender(to_nick)
        .ok_or_else(|| ChatError::UserNotFound(to_nick.to_string()))?;

    let msg = format!("[PM from {}] {}\n", from_nick, content);
    sender.send(msg).map_err(|_| ChatError::Disconnected)?;

    Ok(())
}
```

### Integrar el canal directo en el handler del cliente

```rust
async fn handle_client(
    socket: TcpStream,
    addr: SocketAddr,
    tx: broadcast::Sender<ChatMessage>,
    mut rx: broadcast::Receiver<ChatMessage>,
    state: SharedState,
) -> Result<(), ChatError> {
    let (reader, mut writer) = socket.into_split();
    let mut reader = BufReader::new(reader);
    let mut line = String::new();

    // Create direct message channel for this client
    let (direct_tx, mut direct_rx) = mpsc::unbounded_channel::<String>();

    // Ask for nickname and register
    let nickname = negotiate_nickname(&mut reader, &mut writer, &state).await?;

    // Register in shared state
    {
        let mut s = state.lock().await;
        if !s.add_client(addr, nickname.clone(), direct_tx) {
            writer.write_all(b"!ERR Nickname taken, disconnecting\n").await?;
            return Err(ChatError::NicknameTaken(nickname));
        }
    }
    // Lock released here — critical!

    let _ = tx.send(ChatMessage::UserJoined { nickname: nickname.clone() });
    let welcome = format!("*** Welcome, {}! Type /help for commands. ***\n", nickname);
    writer.write_all(welcome.as_bytes()).await?;

    let mut nickname = nickname;

    // Main loop: three sources of input
    loop {
        tokio::select! {
            // 1. Client sent us a message
            result = read_line_limited(&mut reader, &mut line) => {
                match result {
                    Ok(0) => break,
                    Ok(_) => {
                        match process_input(
                            &line, &nickname, &tx, &mut writer, &state, &addr
                        ).await {
                            Ok(ClientAction::Continue) => {}
                            Ok(ClientAction::Quit) => break,
                            Ok(ClientAction::ChangeNick(new)) => {
                                nickname = new;
                            }
                            Err(e) => {
                                let _ = writer.write_all(
                                    e.to_client_message().as_bytes()
                                ).await;
                            }
                        }
                    }
                    Err(e) => {
                        let _ = writer.write_all(
                            e.to_client_message().as_bytes()
                        ).await;
                    }
                }
            }

            // 2. Broadcast message from another client
            result = rx.recv() => {
                match result {
                    Ok(msg) if msg.should_send_to(&nickname) => {
                        let _ = writer.write_all(
                            msg.to_wire().as_bytes()
                        ).await;
                    }
                    Err(broadcast::error::RecvError::Lagged(n)) => {
                        let warn = format!("*** Missed {} messages ***\n", n);
                        let _ = writer.write_all(warn.as_bytes()).await;
                    }
                    Err(broadcast::error::RecvError::Closed) => break,
                    _ => {}
                }
            }

            // 3. Direct message (private msg, server notice)
            Some(msg) = direct_rx.recv() => {
                let _ = writer.write_all(msg.as_bytes()).await;
            }
        }
    }

    // Cleanup: remove from state and announce departure
    {
        let mut s = state.lock().await;
        s.remove_client(&addr);
    }
    let _ = tx.send(ChatMessage::UserLeft { nickname });

    Ok(())
}
```

### Negociar nickname con verificación de unicidad

```rust
async fn negotiate_nickname(
    reader: &mut BufReader<OwnedReadHalf>,
    writer: &mut OwnedWriteHalf,
    state: &SharedState,
) -> Result<String, ChatError> {
    use tokio::io::{AsyncBufReadExt, AsyncWriteExt};

    let max_attempts = 3;

    for attempt in 0..max_attempts {
        writer.write_all(b"Enter your nickname: ").await?;

        let mut nick = String::new();
        let bytes = reader.read_line(&mut nick).await?;
        if bytes == 0 {
            return Err(ChatError::Disconnected);
        }

        let nick = nick.trim().to_string();

        if nick.is_empty() {
            // Generate a unique default
            let default = generate_unique_default(state).await;
            return Ok(default);
        }

        // Validate format
        if let Err(e) = validate_nickname(&nick) {
            let msg = format!("!ERR {} (attempt {}/{})\n", e, attempt + 1, max_attempts);
            writer.write_all(msg.as_bytes()).await?;
            continue;
        }

        // Check availability
        {
            let s = state.lock().await;
            if !s.is_nickname_available(&nick) {
                let msg = format!(
                    "!ERR Nickname '{}' is taken (attempt {}/{})\n",
                    nick, attempt + 1, max_attempts
                );
                writer.write_all(msg.as_bytes()).await?;
                continue;
            }
        }

        return Ok(nick);
    }

    // After max attempts, assign a default
    let default = generate_unique_default(state).await;
    let msg = format!("*** Too many attempts. Your nickname is {} ***\n", default);
    writer.write_all(msg.as_bytes()).await?;
    Ok(default)
}

async fn generate_unique_default(state: &SharedState) -> String {
    let s = state.lock().await;
    let mut n = 1u64;
    loop {
        let nick = format!("User{}", n);
        if s.is_nickname_available(&nick) {
            return nick;
        }
        n += 1;
    }
}
```

---

## tokio::sync::Mutex vs std::sync::Mutex

Esta es una decisión de diseño importante en código async.

```
┌──────────────────────────────────────────────────────────────┐
│  std::sync::Mutex                tokio::sync::Mutex          │
├────────────────────────────────┬─────────────────────────────┤
│  .lock() → MutexGuard (sync)  │ .lock().await → Guard       │
│  Bloquea el thread del OS      │ Suspende la task, no el     │
│                                │ thread                      │
├────────────────────────────────┼─────────────────────────────┤
│  ✅ Más rápido (sin overhead   │ ✅ No bloquea el runtime    │
│     de polling)                │    de Tokio                 │
├────────────────────────────────┼─────────────────────────────┤
│  ❌ Bloquea el thread de Tokio │ ❌ Más overhead             │
│     si se mantiene mucho       │                             │
├────────────────────────────────┼─────────────────────────────┤
│  Usar cuando:                  │ Usar cuando:                │
│  - Lock se mantiene brevemente │ - Lock se mantiene mientras │
│  - Solo operaciones sync       │   se hace .await            │
│    dentro del lock             │ - Operaciones I/O dentro    │
│  - Acceso a HashMap, contadores│   del lock                  │
└────────────────────────────────┴─────────────────────────────┘
```

### Regla práctica

```rust
// ✅ std::sync::Mutex: operaciones rápidas, sin .await dentro
{
    let mut state = state.lock().unwrap();  // sync lock
    state.add_client(addr, nickname, tx);   // Operación rápida
}   // Lock liberado inmediatamente

// ✅ tokio::sync::Mutex: si necesitas .await dentro del lock
{
    let state = state.lock().await;  // async lock
    // ... hacer algo que requiere .await ...
}

// Para nuestro chat: las operaciones sobre el estado son rápidas
// (insert/remove/lookup en HashMap). std::sync::Mutex sería suficiente.
// Pero usamos tokio::sync::Mutex por simplicidad (no necesitas unwrap).
```

### Usar std::sync::Mutex en contexto async (perfectamente válido)

```rust
use std::sync::{Arc, Mutex};

type SharedState = Arc<Mutex<ServerState>>;

// No hay .await dentro del bloque del lock
fn process_list(state: &SharedState) -> Vec<String> {
    let state = state.lock().unwrap();  // No .await, solo lock sync
    state.list_nicknames()
}   // Lock liberado, nunca cruza un .await

// ⚠️ Lo que NUNCA debes hacer:
async fn bad_pattern(state: &Arc<Mutex<ServerState>>) {
    let guard = state.lock().unwrap();
    // ❌ .await mientras tienes el lock → bloquea thread del runtime
    tokio::time::sleep(Duration::from_secs(1)).await;
    drop(guard);
}
```

### Minimizar el scope del lock

```rust
// ❌ Lock mantenido durante toda la operación
async fn process_command(state: &SharedState) {
    let mut s = state.lock().await;
    let users = s.list_nicknames();
    let formatted = format!("*** Online: {} ***\n", users.join(", "));
    writer.write_all(formatted.as_bytes()).await?;  // .await con lock!
    // ← El lock se mantiene durante el write, bloqueando a todos
}

// ✅ Tomar solo lo necesario, liberar antes del I/O
async fn process_command(state: &SharedState) {
    let users = {
        let s = state.lock().await;
        s.list_nicknames()
    };  // Lock liberado aquí

    let formatted = format!("*** Online: {} ***\n", users.join(", "));
    writer.write_all(formatted.as_bytes()).await?;  // Sin lock
}
```

---

## Alternativa: canales en vez de Mutex

Un diseño alternativo usa un **actor**: una task dedicada que posee el estado
y recibe comandos por un canal.

```
┌──────────────────────────────────────────────────────────────┐
│  Patrón Actor (alternativa a Arc<Mutex<T>>)                  │
│                                                              │
│  Task A ──── StateCommand::AddClient ─────┐                 │
│                                            ▼                 │
│  Task B ──── StateCommand::ListUsers ──▶ State Actor         │
│                                            │  (posee el      │
│  Task C ──── StateCommand::Rename ────────┘   ServerState)   │
│                                                              │
│  El actor recibe comandos por un mpsc channel                │
│  y responde por un oneshot channel                           │
│                                                              │
│  Ventajas:                                                   │
│  + No hay Mutex (no hay deadlocks posibles)                  │
│  + El estado nunca se comparte (un solo owner)               │
│  + Fácil agregar lógica compleja                             │
│                                                              │
│  Desventajas:                                                │
│  - Más código boilerplate                                    │
│  - Overhead de los canales                                   │
│  - Latencia de ida y vuelta para cada operación              │
└──────────────────────────────────────────────────────────────┘
```

```rust
use tokio::sync::{mpsc, oneshot};

/// Commands that can be sent to the state actor
enum StateCommand {
    AddClient {
        addr: SocketAddr,
        nickname: String,
        direct_tx: mpsc::UnboundedSender<String>,
        respond_to: oneshot::Sender<bool>,
    },
    RemoveClient {
        addr: SocketAddr,
        respond_to: oneshot::Sender<Option<String>>,
    },
    ListNicknames {
        respond_to: oneshot::Sender<Vec<String>>,
    },
    ChangeNick {
        addr: SocketAddr,
        new_nick: String,
        respond_to: oneshot::Sender<Result<String, ChatError>>,
    },
    SendDirect {
        to_nick: String,
        message: String,
        respond_to: oneshot::Sender<Result<(), ChatError>>,
    },
}

/// The state actor: owns ServerState, processes commands sequentially
async fn state_actor(mut rx: mpsc::Receiver<StateCommand>) {
    let mut state = ServerState::new();

    while let Some(cmd) = rx.recv().await {
        match cmd {
            StateCommand::AddClient { addr, nickname, direct_tx, respond_to } => {
                let result = state.add_client(addr, nickname, direct_tx);
                let _ = respond_to.send(result);
            }
            StateCommand::RemoveClient { addr, respond_to } => {
                let result = state.remove_client(&addr);
                let _ = respond_to.send(result);
            }
            StateCommand::ListNicknames { respond_to } => {
                let _ = respond_to.send(state.list_nicknames());
            }
            StateCommand::ChangeNick { addr, new_nick, respond_to } => {
                let _ = respond_to.send(state.change_nickname(&addr, new_nick));
            }
            StateCommand::SendDirect { to_nick, message, respond_to } => {
                let result = match state.get_direct_sender(&to_nick) {
                    Some(tx) => tx.send(message)
                        .map_err(|_| ChatError::Disconnected),
                    None => Err(ChatError::UserNotFound(to_nick)),
                };
                let _ = respond_to.send(result);
            }
        }
    }
}

/// Handle for interacting with the state actor
#[derive(Clone)]
struct StateHandle {
    tx: mpsc::Sender<StateCommand>,
}

impl StateHandle {
    fn new() -> (Self, mpsc::Receiver<StateCommand>) {
        let (tx, rx) = mpsc::channel(64);
        (StateHandle { tx }, rx)
    }

    async fn list_nicknames(&self) -> Vec<String> {
        let (respond_to, response) = oneshot::channel();
        let _ = self.tx.send(StateCommand::ListNicknames { respond_to }).await;
        response.await.unwrap_or_default()
    }

    async fn add_client(
        &self,
        addr: SocketAddr,
        nickname: String,
        direct_tx: mpsc::UnboundedSender<String>,
    ) -> bool {
        let (respond_to, response) = oneshot::channel();
        let _ = self.tx.send(StateCommand::AddClient {
            addr, nickname, direct_tx, respond_to
        }).await;
        response.await.unwrap_or(false)
    }

    // ... otros métodos similares
}
```

Para nuestro chat, `Arc<Mutex<ServerState>>` es más simple y suficiente.
El patrón actor es mejor para sistemas más complejos donde el estado
tiene invariantes complicadas o múltiples Mutex causarían deadlocks.

---

## El servidor completo con estado

### main.rs

```rust
use tokio::net::TcpListener;
use tokio::sync::broadcast;
use std::sync::Arc;
use tokio::sync::Mutex;

mod protocol;
mod state;
mod handler;

use protocol::ChatMessage;
use state::ServerState;

type SharedState = Arc<Mutex<ServerState>>;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let addr = "127.0.0.1:8080";
    let listener = TcpListener::bind(addr).await?;
    println!("Chat server running on {}", addr);

    let (tx, _rx) = broadcast::channel::<ChatMessage>(256);
    let state: SharedState = Arc::new(Mutex::new(ServerState::new()));

    loop {
        let (socket, addr) = listener.accept().await?;
        let tx = tx.clone();
        let rx = tx.subscribe();
        let state = state.clone();

        tokio::spawn(async move {
            if let Err(e) = handler::handle_client(
                socket, addr, tx, rx, state
            ).await {
                eprintln!("[{}] {}", addr, e);
            }
        });
    }
}
```

### Sesión de ejemplo

```
Terminal 1 (Server):
$ cargo run
Chat server running on 127.0.0.1:8080
[Client 127.0.0.1:54321 connected]
[Client 127.0.0.1:54322 connected]

Terminal 2 (Alice):
$ nc 127.0.0.1 8080
Enter your nickname: Alice
*** Welcome, Alice! Type /help for commands. ***
Hello everyone!
*** Bob has joined the chat ***
[Bob] Hi Alice!
/list
*** Online: Alice, Bob ***
/msg Bob Hey, private message!
/nick Ally
*** You are now known as Ally ***
/quit Bye!

Terminal 3 (Bob):
$ nc 127.0.0.1 8080
Enter your nickname: Bob
*** Welcome, Bob! Type /help for commands. ***
*** Alice has joined the chat ***
[Alice] Hello everyone!
Hi Alice!
[PM from Alice] Hey, private message!
*** Alice is now known as Ally ***
[Ally] (leaving: Bye!)
*** Ally has left the chat ***
```

---

## Errores comunes

### 1. Mantener el lock del Mutex mientras haces .await

```rust
// ❌ Deadlock potencial: lock mantenido durante I/O
async fn bad(state: &SharedState, writer: &mut OwnedWriteHalf) {
    let s = state.lock().await;
    let users = s.list_nicknames();
    // ← El lock sigue activo durante el write!
    writer.write_all(users.join(", ").as_bytes()).await.unwrap();
}

// ✅ Extraer datos, liberar lock, luego I/O
async fn good(state: &SharedState, writer: &mut OwnedWriteHalf) {
    let users = {
        let s = state.lock().await;
        s.list_nicknames()
    };  // ← Lock liberado

    writer.write_all(users.join(", ").as_bytes()).await.unwrap();
}
```

### 2. No limpiar el estado cuando un cliente se desconecta

```rust
// ❌ Si handle_client paniquea, el cliente queda registrado para siempre
tokio::spawn(async move {
    handle_client(socket, addr, tx, rx, state).await;
});

// ✅ Limpiar en un bloque que siempre se ejecuta
tokio::spawn(async move {
    let result = handle_client(socket, addr, tx, rx, state.clone()).await;

    // Cleanup siempre se ejecuta, incluso si handle_client falló
    {
        let mut s = state.lock().await;
        if let Some(nick) = s.remove_client(&addr) {
            let _ = tx.send(ChatMessage::UserLeft { nickname: nick });
        }
    }

    if let Err(e) = result {
        eprintln!("[{}] {}", addr, e);
    }
});
```

### 3. Race condition en check-then-act para nicknames

```rust
// ❌ Race condition entre check y register
let available = {
    let s = state.lock().await;
    s.is_nickname_available(&nick)
};  // Lock liberado

// Otro task podría registrar el mismo nick aquí

{
    let mut s = state.lock().await;
    s.add_client(addr, nick, tx);  // ¡Puede haber colisión!
}

// ✅ Check y register en la misma sección crítica
{
    let mut s = state.lock().await;
    if !s.add_client(addr, nick.clone(), tx) {
        return Err(ChatError::NicknameTaken(nick));
    }
}
```

### 4. Olvidar clonar el Arc antes del spawn

```rust
// ❌ Mover el Arc original al spawn (el loop no puede usarlo más)
tokio::spawn(async move {
    handle_client(socket, addr, tx, rx, state).await;
    //                                     ^^^^^ movido
});
// state ya no existe aquí → error de compilación en la siguiente iteración

// ✅ Clonar antes de mover
let state_clone = state.clone();
tokio::spawn(async move {
    handle_client(socket, addr, tx, rx, state_clone).await;
});
// state sigue vivo para el siguiente accept
```

### 5. Canales directos que se acumulan sin límite

```rust
// ❌ unbounded_channel sin control: si un cliente no lee,
// los mensajes se acumulan infinitamente en memoria
let (direct_tx, direct_rx) = mpsc::unbounded_channel();

// ✅ Opción A: canal con límite
let (direct_tx, direct_rx) = mpsc::channel(100);
// Si se llena, send().await espera → backpressure

// ✅ Opción B: unbounded pero con try_send y desconectar clientes lentos
if direct_tx.send(msg).is_err() {
    // El receiver fue dropeado → el cliente se desconectó
    // Limpiar del estado
}
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│          ESTADO COMPARTIDO CHEATSHEET                        │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  TIPO DEL ESTADO                                             │
│  type SharedState = Arc<Mutex<ServerState>>;                 │
│  Arc::new(Mutex::new(ServerState::new()))   Crear            │
│  state.clone()                              Clonar para task │
│  state.lock().await                         Acceder          │
│                                                              │
│  ESTRUCTURA DEL ESTADO                                       │
│  clients: HashMap<SocketAddr, ClientInfo>   Por dirección    │
│  nick_to_addr: HashMap<String, SocketAddr>  Lookup inverso   │
│  ClientInfo { nickname, direct_tx, ... }    Info del cliente │
│                                                              │
│  OPERACIONES                                                 │
│  add_client(addr, nick, tx) → bool          Registrar       │
│  remove_client(&addr) → Option<String>      Desregistrar    │
│  change_nickname(&addr, new) → Result       Renombrar       │
│  is_nickname_available(&nick) → bool        Verificar       │
│  list_nicknames() → Vec<String>             Listar          │
│  get_direct_sender(&nick) → Option<&Tx>     Canal directo   │
│                                                              │
│  CANALES                                                     │
│  broadcast::channel → todos los clientes                     │
│  mpsc::unbounded_channel → un cliente específico             │
│                                                              │
│  SELECT CON 3 FUENTES                                        │
│  tokio::select! {                                            │
│      read_line()     → cliente envió mensaje                 │
│      rx.recv()       → broadcast de otro                     │
│      direct_rx.recv()→ mensaje privado/servidor              │
│  }                                                           │
│                                                              │
│  MUTEX: std vs tokio                                         │
│  std::sync::Mutex    Rápido, sin .await dentro               │
│  tokio::sync::Mutex  Suspende task, .await dentro OK         │
│  Regla: si el lock es breve y sin I/O → std                  │
│                                                              │
│  REGLAS DE ORO                                               │
│  1. Minimizar scope del lock                                 │
│  2. Nunca .await dentro de un std::sync::Mutex               │
│  3. Check-and-act en la misma sección crítica                │
│  4. Siempre limpiar estado en desconexión                    │
│  5. Clone Arc antes de spawn, no mover                       │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Implementar el estado compartido

**Tareas:**
1. Crea `state.rs` con `ServerState`, `ClientInfo`, y todas las operaciones
2. Escribe tests unitarios (sin async) para:
   - Agregar 3 clientes con nicknames diferentes
   - Intentar agregar un nickname duplicado
   - Remover un cliente y verificar que su nickname queda libre
   - Cambiar nickname (caso exitoso y caso tomado)
   - Listar nicknames (verificar orden alfabético)
3. Integra con el servidor de T02: reemplaza el nickname por dirección IP
   con nicknames reales verificados contra el estado
4. Verifica con `nc` que `/nick` con un nombre ya tomado da error

**Pregunta de reflexión:** El `nick_to_addr` HashMap duplica información que
ya existe en `clients`. ¿Por qué es útil esta duplicación? ¿Cuál sería el
costo de buscar un nickname iterando todos los `clients.values()`?

---

### Ejercicio 2: Mensajes privados

**Tareas:**
1. Implementa el canal directo `mpsc::unbounded_channel` por cliente
2. Agrega el tercer branch en `tokio::select!` para `direct_rx.recv()`
3. Implementa el comando `/msg <nick> <message>`:
   - Buscar el sender directo del destinatario en el estado compartido
   - Enviar el mensaje solo a ese cliente
   - Confirmar al remitente que el mensaje fue enviado
   - Error si el destinatario no existe
4. Prueba con 3 clientes:
   ```
   Alice: /msg Bob Hola en privado
   Bob: [PM from Alice] Hola en privado   (solo Bob lo ve)
   Carol: (no recibe nada)
   ```
5. ¿Qué pasa si envías `/msg Alice Hola` a ti misma?

**Pregunta de reflexión:** Los canales `mpsc::unbounded_channel` pueden
consumir memoria ilimitada. ¿Cómo implementarías un límite de mensajes
pendientes por cliente? ¿Qué acción tomarías cuando un cliente alcanza
el límite: descartar mensajes, desconectar al cliente, o bloquear al
remitente?

---

### Ejercicio 3: Actor de estado vs Mutex

Implementa el patrón actor como alternativa:

**Tareas:**
1. Define el enum `StateCommand` con variantes para cada operación
2. Implementa la función `state_actor` que recibe comandos por un canal
3. Crea `StateHandle` con métodos async que envían comandos y esperan respuesta
4. Reescribe el handler del cliente para usar `StateHandle` en vez de
   `Arc<Mutex<ServerState>>`
5. Compara ambas implementaciones:
   - ¿Cuántas líneas de código más necesita el actor?
   - ¿Es más fácil o más difícil razonar sobre la correctitud?
   - ¿Hay algún escenario donde el actor sería claramente mejor?

**Pregunta de reflexión:** En el patrón actor, ¿qué pasa si la task del actor
paniquea? Todos los `StateHandle` recibirán errores en sus `oneshot::Receiver`.
¿Cómo manejarías este caso? Compara con el caso Mutex: si una task paniquea
mientras tiene el lock, ¿qué pasa con `std::sync::Mutex` vs `tokio::sync::Mutex`?
(Hint: "poisoned mutex")