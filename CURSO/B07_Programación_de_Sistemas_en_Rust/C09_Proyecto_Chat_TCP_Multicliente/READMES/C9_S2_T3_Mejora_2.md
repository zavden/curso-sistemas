# Mejora 2: salas y canales

## Índice

1. [De un chat global a salas](#de-un-chat-global-a-salas)
2. [Diseño del modelo de salas](#diseño-del-modelo-de-salas)
3. [Extender el protocolo](#extender-el-protocolo)
4. [Estado del servidor con salas](#estado-del-servidor-con-salas)
5. [Operaciones de sala](#operaciones-de-sala)
6. [Enrutamiento de mensajes](#enrutamiento-de-mensajes)
7. [Integración con el handler de conexión](#integración-con-el-handler-de-conexión)
8. [Mejoras al cliente](#mejoras-al-cliente)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## De un chat global a salas

Hasta ahora, todos los usuarios comparten un único espacio: todo mensaje llega
a todos. Esto funciona para grupos pequeños pero no escala. Las **salas**
(o canales) permiten crear espacios temáticos separados:

```
┌──────────────────────────────────────────────────────────────┐
│  Sin salas (estado actual):                                  │
│                                                              │
│  Alice ──┐                                                   │
│  Bob   ──┼──▶ broadcast ──▶ TODOS ven todo                  │
│  Carol ──┘                                                   │
│                                                              │
│  Con salas:                                                   │
│                                                              │
│  Alice ──┐              ┌──▶ Alice, Bob ven msgs de #general │
│  Bob   ──┼── #general ──┘                                    │
│  Bob   ──┼── #rust     ──┐                                   │
│  Carol ──┘              └──▶ Bob, Carol ven msgs de #rust    │
│                                                              │
│  - Un usuario puede estar en múltiples salas                 │
│  - Un mensaje en #general NO aparece en #rust                │
│  - /msg funciona entre cualquier usuario (sin importar sala) │
│  - Existe una sala por defecto (#lobby) al conectarse        │
└──────────────────────────────────────────────────────────────┘
```

### Convención de nombres

Seguimos la convención de IRC: los nombres de sala empiezan con `#`:

```
#lobby       ← sala por defecto
#rust        ← sala temática
#off-topic   ← sala informal
general      ← INVÁLIDO (falta el #)
```

---

## Diseño del modelo de salas

### Estructura Room

```rust
use std::collections::HashSet;
use std::net::SocketAddr;

/// A chat room/channel
#[derive(Debug)]
pub struct Room {
    /// Name of the room (including the # prefix)
    pub name: String,

    /// Optional topic/description
    pub topic: Option<String>,

    /// Addresses of members currently in this room
    members: HashSet<SocketAddr>,

    /// When the room was created
    pub created_at: std::time::Instant,
}

impl Room {
    pub fn new(name: String) -> Self {
        Self {
            name,
            topic: None,
            members: HashSet::new(),
            created_at: std::time::Instant::now(),
        }
    }

    pub fn add_member(&mut self, addr: SocketAddr) -> bool {
        self.members.insert(addr) // true si no estaba ya
    }

    pub fn remove_member(&mut self, addr: &SocketAddr) -> bool {
        self.members.remove(addr)
    }

    pub fn has_member(&self, addr: &SocketAddr) -> bool {
        self.members.contains(addr)
    }

    pub fn member_count(&self) -> usize {
        self.members.len()
    }

    pub fn members(&self) -> &HashSet<SocketAddr> {
        &self.members
    }

    pub fn is_empty(&self) -> bool {
        self.members.is_empty()
    }
}
```

### Relación usuario-sala

```
┌──────────────────────────────────────────────────────────────┐
│  Modelo de datos                                             │
│                                                              │
│  ServerState                                                 │
│  ├── clients: HashMap<SocketAddr, ClientInfo>                │
│  │     ClientInfo {                                          │
│  │       nickname, direct_tx, connected_at,                  │
│  │       rooms: HashSet<String>,     ← NUEVO                │
│  │       active_room: String,        ← NUEVO                │
│  │     }                                                     │
│  │                                                           │
│  ├── nick_to_addr: HashMap<String, SocketAddr>               │
│  │                                                           │
│  └── rooms: HashMap<String, Room>    ← NUEVO                │
│        Room {                                                │
│          name, topic, members: HashSet<SocketAddr>           │
│        }                                                     │
│                                                              │
│  Invariante: si addr ∈ room.members                         │
│              entonces room.name ∈ client.rooms               │
│  (los dos sets se mantienen sincronizados)                   │
└──────────────────────────────────────────────────────────────┘
```

### Validación de nombres de sala

```rust
pub fn validate_room_name(name: &str) -> Result<(), ChatError> {
    if !name.starts_with('#') {
        return Err(ChatError::InvalidRoomName(
            "Room name must start with #".into()
        ));
    }

    let body = &name[1..]; // Sin el #

    if body.is_empty() {
        return Err(ChatError::InvalidRoomName(
            "Room name cannot be just #".into()
        ));
    }

    if body.len() > 30 {
        return Err(ChatError::InvalidRoomName(
            "Room name too long (max 30 chars after #)".into()
        ));
    }

    if body.contains(char::is_whitespace) {
        return Err(ChatError::InvalidRoomName(
            "Room name cannot contain spaces".into()
        ));
    }

    // Solo alfanumérico, guiones y underscores
    if !body.chars().all(|c| c.is_alphanumeric() || c == '-' || c == '_') {
        return Err(ChatError::InvalidRoomName(
            "Room name can only contain letters, numbers, - and _".into()
        ));
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_valid_room_names() {
        assert!(validate_room_name("#general").is_ok());
        assert!(validate_room_name("#rust-lang").is_ok());
        assert!(validate_room_name("#off_topic").is_ok());
        assert!(validate_room_name("#Room123").is_ok());
    }

    #[test]
    fn test_invalid_room_names() {
        assert!(validate_room_name("general").is_err());    // Sin #
        assert!(validate_room_name("#").is_err());           // Solo #
        assert!(validate_room_name("#has space").is_err());  // Espacios
        assert!(validate_room_name("#bad!char").is_err());   // Carácter inválido
    }
}
```

---

## Extender el protocolo

### Nuevos comandos

Agregamos los siguientes comandos al enum `Command` de T02:

```rust
#[derive(Debug, PartialEq)]
pub enum Command {
    // Comandos existentes de T02
    Nick(String),
    Quit(Option<String>),
    List,
    Help,
    Msg { target: String, content: String },
    Me(String),

    // Nuevos comandos para salas
    /// /join #room — unirse a una sala (la crea si no existe)
    Join(String),

    /// /leave [#room] — salir de una sala (default: la sala activa)
    Leave(Option<String>),

    /// /rooms — listar todas las salas del servidor
    Rooms,

    /// /topic [text] — ver o cambiar el topic de la sala activa
    Topic(Option<String>),

    /// /switch #room — cambiar la sala activa (sin join/leave)
    Switch(String),

    /// /who #room — listar los miembros de una sala
    Who(String),
}
```

### Nuevos tipos de mensaje

```rust
#[derive(Debug, Clone)]
pub enum ChatMessage {
    // Existentes
    UserMessage { sender: String, content: String },
    SystemAnnounce(String),
    UserJoined { nickname: String },
    UserLeft { nickname: String },
    NickChange { old_nick: String, new_nick: String },

    // Nuevos para salas
    /// Mensaje de chat dentro de una sala
    RoomMessage {
        room: String,
        sender: String,
        content: String,
    },

    /// Usuario entró a una sala
    RoomJoined {
        room: String,
        nickname: String,
    },

    /// Usuario salió de una sala
    RoomLeft {
        room: String,
        nickname: String,
    },

    /// El topic de una sala cambió
    TopicChanged {
        room: String,
        nickname: String,
        topic: String,
    },
}

impl ChatMessage {
    pub fn format_for_display(&self) -> String {
        match self {
            // Existentes...
            ChatMessage::UserMessage { sender, content } => {
                format!("[{}] {}", sender, content)
            }
            ChatMessage::SystemAnnounce(msg) => {
                format!("*** {} ***", msg)
            }
            ChatMessage::UserJoined { nickname } => {
                format!("*** {} has joined the chat ***", nickname)
            }
            ChatMessage::UserLeft { nickname } => {
                format!("*** {} has left the chat ***", nickname)
            }
            ChatMessage::NickChange { old_nick, new_nick } => {
                format!("*** {} is now known as {} ***", old_nick, new_nick)
            }

            // Nuevos
            ChatMessage::RoomMessage { room, sender, content } => {
                format!("[{}][{}] {}", room, sender, content)
            }
            ChatMessage::RoomJoined { room, nickname } => {
                format!("*** {} has joined {} ***", nickname, room)
            }
            ChatMessage::RoomLeft { room, nickname } => {
                format!("*** {} has left {} ***", nickname, room)
            }
            ChatMessage::TopicChanged { room, nickname, topic } => {
                format!("*** {} set topic for {} to: {} ***", nickname, room, topic)
            }
        }
    }
}
```

### Parsear los nuevos comandos

Extendemos `parse_input` con los nuevos comandos:

```rust
pub fn parse_input(input: &str) -> Result<UserInput, ChatError> {
    let input = input.trim();

    if input.is_empty() {
        return Err(ChatError::EmptyMessage);
    }

    if !input.starts_with('/') {
        return Ok(UserInput::Message(input.to_string()));
    }

    let mut parts = input[1..].splitn(2, char::is_whitespace);
    let command = parts.next().unwrap_or("");
    let args = parts.next().unwrap_or("").trim();

    match command.to_lowercase().as_str() {
        // ... comandos existentes (nick, quit, list, help, msg, me) ...

        "join" | "j" => {
            if args.is_empty() {
                return Err(ChatError::MissingArgument(
                    "Usage: /join #room".into()
                ));
            }
            let room = args.split_whitespace().next().unwrap();
            // Agregar # si el usuario lo omitió
            let room = if room.starts_with('#') {
                room.to_string()
            } else {
                format!("#{}", room)
            };
            validate_room_name(&room)?;
            Ok(UserInput::Command(Command::Join(room)))
        }

        "leave" | "part" => {
            if args.is_empty() {
                Ok(UserInput::Command(Command::Leave(None)))
            } else {
                let room = args.split_whitespace().next().unwrap();
                let room = if room.starts_with('#') {
                    room.to_string()
                } else {
                    format!("#{}", room)
                };
                Ok(UserInput::Command(Command::Leave(Some(room))))
            }
        }

        "rooms" | "channels" => {
            Ok(UserInput::Command(Command::Rooms))
        }

        "topic" => {
            if args.is_empty() {
                Ok(UserInput::Command(Command::Topic(None)))
            } else {
                Ok(UserInput::Command(Command::Topic(Some(args.to_string()))))
            }
        }

        "switch" | "sw" => {
            if args.is_empty() {
                return Err(ChatError::MissingArgument(
                    "Usage: /switch #room".into()
                ));
            }
            let room = args.split_whitespace().next().unwrap();
            let room = if room.starts_with('#') {
                room.to_string()
            } else {
                format!("#{}", room)
            };
            Ok(UserInput::Command(Command::Switch(room)))
        }

        "who" => {
            if args.is_empty() {
                return Err(ChatError::MissingArgument(
                    "Usage: /who #room".into()
                ));
            }
            let room = args.split_whitespace().next().unwrap().to_string();
            Ok(UserInput::Command(Command::Who(room)))
        }

        unknown => Err(ChatError::UnknownCommand(unknown.to_string())),
    }
}
```

### Nuevos errores

```rust
#[derive(Debug)]
pub enum ChatError {
    // Existentes
    Io(std::io::Error),
    Disconnected,
    MessageTooLong,
    EmptyMessage,
    InvalidNickname(String),
    NicknameTaken(String),
    MissingArgument(String),
    UnknownCommand(String),
    UserNotFound(String),

    // Nuevos para salas
    InvalidRoomName(String),
    NotInRoom(String),
    AlreadyInRoom(String),
}

impl std::fmt::Display for ChatError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            // ... existentes ...
            ChatError::InvalidRoomName(reason) => write!(f, "Invalid room name: {}", reason),
            ChatError::NotInRoom(room) => write!(f, "You are not in {}", room),
            ChatError::AlreadyInRoom(room) => write!(f, "You are already in {}", room),
            // ... resto igual ...
            _ => write!(f, "{:?}", self), // Fallback para no repetir todo
        }
    }
}
```

### Mensaje de ayuda actualizado

```rust
pub fn help_text() -> &'static str {
    "\
*** Available commands: ***
  /nick <name>          Change your nickname
  /msg <user> <text>    Send a private message
  /me <action>          Send an emote
  /join #room           Join a room (creates it if needed)
  /leave [#room]        Leave a room (default: active room)
  /switch #room         Switch active room
  /rooms                List all rooms
  /who #room            List members of a room
  /topic [text]         View or set room topic
  /list                 Show online users
  /help                 Show this help
  /quit [message]       Leave the chat
***"
}
```

---

## Estado del servidor con salas

### ClientInfo actualizado

```rust
use std::collections::HashSet;

#[derive(Debug)]
pub struct ClientInfo {
    pub nickname: String,
    pub direct_tx: mpsc::UnboundedSender<String>,
    pub connected_at: std::time::Instant,

    /// Rooms this client is a member of
    pub rooms: HashSet<String>,

    /// The room the client is currently "focused" on
    /// (messages without room prefix go here)
    pub active_room: String,
}
```

### ServerState actualizado

```rust
use std::collections::HashMap;

pub const DEFAULT_ROOM: &str = "#lobby";

pub struct ServerState {
    clients: HashMap<SocketAddr, ClientInfo>,
    nick_to_addr: HashMap<String, SocketAddr>,
    rooms: HashMap<String, Room>,    // NUEVO
}

impl ServerState {
    pub fn new() -> Self {
        let mut rooms = HashMap::new();
        // Crear la sala por defecto
        rooms.insert(
            DEFAULT_ROOM.to_string(),
            Room::new(DEFAULT_ROOM.to_string()),
        );

        ServerState {
            clients: HashMap::new(),
            nick_to_addr: HashMap::new(),
            rooms,
        }
    }
}
```

### Registro con sala por defecto

Al conectarse, un cliente se une automáticamente a `#lobby`:

```rust
impl ServerState {
    pub fn add_client(
        &mut self,
        addr: SocketAddr,
        nickname: String,
        direct_tx: mpsc::UnboundedSender<String>,
    ) -> bool {
        if self.nick_to_addr.contains_key(&nickname) {
            return false;
        }

        let mut rooms = HashSet::new();
        rooms.insert(DEFAULT_ROOM.to_string());

        self.nick_to_addr.insert(nickname.clone(), addr);
        self.clients.insert(addr, ClientInfo {
            nickname,
            direct_tx,
            connected_at: std::time::Instant::now(),
            rooms,
            active_room: DEFAULT_ROOM.to_string(),
        });

        // Agregar a la sala por defecto
        if let Some(room) = self.rooms.get_mut(DEFAULT_ROOM) {
            room.add_member(addr);
        }

        true
    }
}
```

---

## Operaciones de sala

### Join: unirse a una sala

```rust
impl ServerState {
    /// Join a room (creates it if it doesn't exist).
    /// Returns the list of existing members for the join notification.
    pub fn join_room(
        &mut self,
        addr: &SocketAddr,
        room_name: &str,
    ) -> Result<Vec<SocketAddr>, ChatError> {
        // Obtener referencia al cliente
        let client = self.clients.get_mut(addr)
            .ok_or(ChatError::Disconnected)?;

        // ¿Ya está en la sala?
        if client.rooms.contains(room_name) {
            return Err(ChatError::AlreadyInRoom(room_name.to_string()));
        }

        // Crear la sala si no existe
        let room = self.rooms
            .entry(room_name.to_string())
            .or_insert_with(|| Room::new(room_name.to_string()));

        // Miembros actuales (para notificarles)
        let existing_members: Vec<SocketAddr> = room.members().iter().copied().collect();

        // Agregar el miembro
        room.add_member(*addr);
        client.rooms.insert(room_name.to_string());
        client.active_room = room_name.to_string();

        Ok(existing_members)
    }
}
```

### Leave: salir de una sala

```rust
impl ServerState {
    /// Leave a room. Returns the remaining members for notification.
    pub fn leave_room(
        &mut self,
        addr: &SocketAddr,
        room_name: &str,
    ) -> Result<Vec<SocketAddr>, ChatError> {
        let client = self.clients.get_mut(addr)
            .ok_or(ChatError::Disconnected)?;

        if !client.rooms.contains(room_name) {
            return Err(ChatError::NotInRoom(room_name.to_string()));
        }

        client.rooms.remove(room_name);

        // Si era la sala activa, cambiar a otra
        if client.active_room == room_name {
            client.active_room = client.rooms.iter()
                .next()
                .cloned()
                .unwrap_or_else(|| DEFAULT_ROOM.to_string());

            // Asegurar que el usuario esté en alguna sala
            if client.rooms.is_empty() {
                client.rooms.insert(DEFAULT_ROOM.to_string());
                client.active_room = DEFAULT_ROOM.to_string();
                if let Some(room) = self.rooms.get_mut(DEFAULT_ROOM) {
                    room.add_member(*addr);
                }
            }
        }

        // Quitar de la sala
        let remaining = if let Some(room) = self.rooms.get_mut(room_name) {
            room.remove_member(addr);
            let remaining: Vec<SocketAddr> = room.members().iter().copied().collect();

            // Limpiar salas vacías (excepto la por defecto)
            if room.is_empty() && room_name != DEFAULT_ROOM {
                self.rooms.remove(room_name);
            }

            remaining
        } else {
            vec![]
        };

        Ok(remaining)
    }
}
```

### Switch: cambiar la sala activa

```rust
impl ServerState {
    /// Switch the active room (must already be a member)
    pub fn switch_room(
        &mut self,
        addr: &SocketAddr,
        room_name: &str,
    ) -> Result<(), ChatError> {
        let client = self.clients.get_mut(addr)
            .ok_or(ChatError::Disconnected)?;

        if !client.rooms.contains(room_name) {
            return Err(ChatError::NotInRoom(room_name.to_string()));
        }

        client.active_room = room_name.to_string();
        Ok(())
    }

    /// Get the active room for a client
    pub fn get_active_room(&self, addr: &SocketAddr) -> Option<&str> {
        self.clients.get(addr).map(|c| c.active_room.as_str())
    }
}
```

### Listar salas y miembros

```rust
impl ServerState {
    /// List all rooms with their member counts
    pub fn list_rooms(&self) -> Vec<(String, usize, Option<String>)> {
        let mut rooms: Vec<_> = self.rooms.values()
            .map(|r| (r.name.clone(), r.member_count(), r.topic.clone()))
            .collect();
        rooms.sort_by(|a, b| a.0.cmp(&b.0));
        rooms
    }

    /// List members of a specific room (returns nicknames)
    pub fn room_members(&self, room_name: &str) -> Result<Vec<String>, ChatError> {
        let room = self.rooms.get(room_name)
            .ok_or(ChatError::NotInRoom(room_name.to_string()))?;

        let mut members: Vec<String> = room.members().iter()
            .filter_map(|addr| {
                self.clients.get(addr).map(|c| c.nickname.clone())
            })
            .collect();
        members.sort();
        Ok(members)
    }

    /// Set topic for a room
    pub fn set_topic(
        &mut self,
        addr: &SocketAddr,
        room_name: &str,
        topic: String,
    ) -> Result<Vec<SocketAddr>, ChatError> {
        // Verificar que el usuario está en la sala
        let client = self.clients.get(addr)
            .ok_or(ChatError::Disconnected)?;

        if !client.rooms.contains(room_name) {
            return Err(ChatError::NotInRoom(room_name.to_string()));
        }

        let room = self.rooms.get_mut(room_name)
            .ok_or(ChatError::NotInRoom(room_name.to_string()))?;

        room.topic = Some(topic);
        let members: Vec<SocketAddr> = room.members().iter().copied().collect();
        Ok(members)
    }

    /// Get the members of a room who should receive a message
    /// (everyone except the sender)
    pub fn room_recipients(
        &self,
        room_name: &str,
        exclude: &SocketAddr,
    ) -> Vec<SocketAddr> {
        self.rooms.get(room_name)
            .map(|room| {
                room.members().iter()
                    .filter(|addr| *addr != exclude)
                    .copied()
                    .collect()
            })
            .unwrap_or_default()
    }
}
```

### Limpieza al desconectar

```rust
impl ServerState {
    /// Remove a client from all rooms and cleanup
    pub fn remove_client(&mut self, addr: &SocketAddr) -> Option<ClientDisconnect> {
        let info = self.clients.remove(addr)?;
        self.nick_to_addr.remove(&info.nickname);

        // Quitar de todas las salas
        let mut affected_rooms = Vec::new();
        for room_name in &info.rooms {
            if let Some(room) = self.rooms.get_mut(room_name) {
                room.remove_member(addr);
                let members: Vec<SocketAddr> = room.members().iter().copied().collect();
                affected_rooms.push((room_name.clone(), members));

                // Limpiar salas vacías (excepto la default)
                if room.is_empty() && room_name != DEFAULT_ROOM {
                    // Marcar para eliminación (no podemos remove aquí por el borrow)
                }
            }
        }

        // Limpiar salas vacías
        self.rooms.retain(|name, room| {
            name == DEFAULT_ROOM || !room.is_empty()
        });

        Some(ClientDisconnect {
            nickname: info.nickname,
            affected_rooms,
        })
    }
}

/// Info returned when a client disconnects
pub struct ClientDisconnect {
    pub nickname: String,
    /// (room_name, remaining_members) for each room the user was in
    pub affected_rooms: Vec<(String, Vec<SocketAddr>)>,
}
```

### Tests del modelo

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
    fn test_new_client_joins_default_room() {
        let mut state = ServerState::new();
        state.add_client(addr(1), "Alice".into(), make_tx());

        let client = state.clients.get(&addr(1)).unwrap();
        assert!(client.rooms.contains(DEFAULT_ROOM));
        assert_eq!(client.active_room, DEFAULT_ROOM);

        let room = state.rooms.get(DEFAULT_ROOM).unwrap();
        assert!(room.has_member(&addr(1)));
    }

    #[test]
    fn test_join_creates_room() {
        let mut state = ServerState::new();
        state.add_client(addr(1), "Alice".into(), make_tx());

        let result = state.join_room(&addr(1), "#rust");
        assert!(result.is_ok());
        assert!(state.rooms.contains_key("#rust"));

        let client = state.clients.get(&addr(1)).unwrap();
        assert!(client.rooms.contains("#rust"));
        assert_eq!(client.active_room, "#rust"); // Se vuelve la activa
    }

    #[test]
    fn test_join_existing_room() {
        let mut state = ServerState::new();
        state.add_client(addr(1), "Alice".into(), make_tx());
        state.add_client(addr(2), "Bob".into(), make_tx());

        state.join_room(&addr(1), "#rust").unwrap();
        let existing = state.join_room(&addr(2), "#rust").unwrap();

        // Bob ve que Alice ya estaba
        assert_eq!(existing, vec![addr(1)]);
    }

    #[test]
    fn test_join_already_in_room() {
        let mut state = ServerState::new();
        state.add_client(addr(1), "Alice".into(), make_tx());
        state.join_room(&addr(1), "#rust").unwrap();

        let result = state.join_room(&addr(1), "#rust");
        assert!(result.is_err());
    }

    #[test]
    fn test_leave_room() {
        let mut state = ServerState::new();
        state.add_client(addr(1), "Alice".into(), make_tx());
        state.add_client(addr(2), "Bob".into(), make_tx());
        state.join_room(&addr(1), "#rust").unwrap();
        state.join_room(&addr(2), "#rust").unwrap();

        let remaining = state.leave_room(&addr(1), "#rust").unwrap();
        assert_eq!(remaining, vec![addr(2)]); // Bob sigue

        let client = state.clients.get(&addr(1)).unwrap();
        assert!(!client.rooms.contains("#rust"));
    }

    #[test]
    fn test_leave_last_member_removes_room() {
        let mut state = ServerState::new();
        state.add_client(addr(1), "Alice".into(), make_tx());
        state.join_room(&addr(1), "#temp").unwrap();

        state.leave_room(&addr(1), "#temp").unwrap();
        assert!(!state.rooms.contains_key("#temp"));
    }

    #[test]
    fn test_default_room_never_removed() {
        let mut state = ServerState::new();
        state.add_client(addr(1), "Alice".into(), make_tx());

        // Intentar salir de #lobby → vuelve a entrar
        // (el usuario siempre está en al menos una sala)
        state.join_room(&addr(1), "#other").unwrap();
        state.leave_room(&addr(1), DEFAULT_ROOM).unwrap();

        // #lobby sigue existiendo aunque esté vacía
        assert!(state.rooms.contains_key(DEFAULT_ROOM));
    }

    #[test]
    fn test_disconnect_cleanup() {
        let mut state = ServerState::new();
        state.add_client(addr(1), "Alice".into(), make_tx());
        state.join_room(&addr(1), "#rust").unwrap();
        state.join_room(&addr(1), "#help").unwrap();

        let disconnect = state.remove_client(&addr(1)).unwrap();
        assert_eq!(disconnect.nickname, "Alice");
        assert_eq!(disconnect.affected_rooms.len(), 3); // #lobby, #rust, #help

        // Salas vacías se eliminaron (excepto #lobby)
        assert!(!state.rooms.contains_key("#rust"));
        assert!(!state.rooms.contains_key("#help"));
        assert!(state.rooms.contains_key(DEFAULT_ROOM));
    }

    #[test]
    fn test_room_recipients() {
        let mut state = ServerState::new();
        state.add_client(addr(1), "Alice".into(), make_tx());
        state.add_client(addr(2), "Bob".into(), make_tx());
        state.add_client(addr(3), "Carol".into(), make_tx());
        state.join_room(&addr(1), "#rust").unwrap();
        state.join_room(&addr(2), "#rust").unwrap();
        // Carol NO está en #rust

        let recipients = state.room_recipients("#rust", &addr(1));
        assert_eq!(recipients, vec![addr(2)]); // Solo Bob (no Alice que envía)
    }
}
```

---

## Enrutamiento de mensajes

### De broadcast global a routing por sala

El cambio fundamental: en vez de enviar a todos vía `broadcast::channel`,
ahora debemos enviar solo a los miembros de la sala correspondiente.

```
┌──────────────────────────────────────────────────────────────┐
│  Antes (broadcast global):                                   │
│                                                              │
│  Alice envía "Hello"                                        │
│    → broadcast_tx.send(msg)                                  │
│    → TODOS los broadcast_rx reciben                          │
│    → should_send_to() filtra al sender                       │
│                                                              │
│  Después (routing por sala):                                │
│                                                              │
│  Alice envía "Hello" (active_room = #rust)                  │
│    → state.room_recipients("#rust", alice_addr)              │
│    → Para cada recipient: direct_tx.send(formatted_msg)      │
│    → Solo los miembros de #rust reciben                      │
│                                                              │
│  Opciones de implementación:                                 │
│                                                              │
│  A) broadcast por sala:                                      │
│     HashMap<String, broadcast::Sender>                       │
│     + Eficiente para muchos miembros                        │
│     - Crear/destruir channels al crear/destruir salas        │
│                                                              │
│  B) direct_tx por recipiente (elegimos esta):                │
│     Iterar room.members, enviar por direct_tx de cada uno    │
│     + Simple, sin channels adicionales                      │
│     + Ya tenemos direct_tx por T03                           │
│     - O(n) por mensaje donde n = miembros de la sala        │
└──────────────────────────────────────────────────────────────┘
```

### Función de envío a sala

```rust
impl ServerState {
    /// Send a message to all members of a room except the sender
    pub fn send_to_room(
        &self,
        room_name: &str,
        sender_addr: &SocketAddr,
        message: &str,
    ) {
        if let Some(room) = self.rooms.get(room_name) {
            for member_addr in room.members() {
                if member_addr != sender_addr {
                    if let Some(client) = self.clients.get(member_addr) {
                        // Ignorar errores de envío (el cliente puede estar cerrando)
                        let _ = client.direct_tx.send(format!("{}\n", message));
                    }
                }
            }
        }
    }

    /// Send a message to all members of a room (including sender)
    pub fn broadcast_to_room(
        &self,
        room_name: &str,
        message: &str,
    ) {
        if let Some(room) = self.rooms.get(room_name) {
            for member_addr in room.members() {
                if let Some(client) = self.clients.get(member_addr) {
                    let _ = client.direct_tx.send(format!("{}\n", message));
                }
            }
        }
    }
}
```

---

## Integración con el handler de conexión

### Procesar comandos de sala

```rust
async fn handle_command(
    command: Command,
    addr: &SocketAddr,
    state: &SharedState,
) -> ClientAction {
    let mut state = state.lock().await;

    match command {
        // ... comandos existentes (nick, quit, list, help, msg, me) ...

        Command::Join(room_name) => {
            let nickname = state.get_nickname(addr)
                .unwrap_or("???").to_string();

            match state.join_room(addr, &room_name) {
                Ok(existing_members) => {
                    // Notificar a los miembros existentes
                    let join_msg = format!(
                        "*** {} has joined {} ***",
                        nickname, room_name
                    );
                    for member in &existing_members {
                        if let Some(client) = state.clients.get(member) {
                            let _ = client.direct_tx.send(
                                format!("{}\n", join_msg)
                            );
                        }
                    }

                    // Confirmar al usuario
                    let room_info = if let Some(room) = state.rooms.get(&room_name) {
                        let topic = room.topic.as_deref().unwrap_or("(no topic)");
                        let count = room.member_count();
                        format!(
                            "*** Joined {} ({} members) — Topic: {} ***",
                            room_name, count, topic
                        )
                    } else {
                        format!("*** Joined {} ***", room_name)
                    };

                    ClientAction::SendDirect(room_info)
                }
                Err(e) => ClientAction::SendDirect(format!("!ERR {}", e)),
            }
        }

        Command::Leave(room_name) => {
            let room_name = room_name.unwrap_or_else(|| {
                state.get_active_room(addr)
                    .unwrap_or(DEFAULT_ROOM)
                    .to_string()
            });

            let nickname = state.get_nickname(addr)
                .unwrap_or("???").to_string();

            match state.leave_room(addr, &room_name) {
                Ok(remaining) => {
                    // Notificar a los que quedan
                    let leave_msg = format!(
                        "*** {} has left {} ***",
                        nickname, room_name
                    );
                    for member in &remaining {
                        if let Some(client) = state.clients.get(member) {
                            let _ = client.direct_tx.send(
                                format!("{}\n", leave_msg)
                            );
                        }
                    }

                    let active = state.get_active_room(addr)
                        .unwrap_or(DEFAULT_ROOM);
                    ClientAction::SendDirect(
                        format!("*** Left {}. Active room: {} ***", room_name, active)
                    )
                }
                Err(e) => ClientAction::SendDirect(format!("!ERR {}", e)),
            }
        }

        Command::Switch(room_name) => {
            match state.switch_room(addr, &room_name) {
                Ok(()) => {
                    ClientAction::SendDirect(
                        format!("*** Switched to {} ***", room_name)
                    )
                }
                Err(e) => ClientAction::SendDirect(format!("!ERR {}", e)),
            }
        }

        Command::Rooms => {
            let rooms = state.list_rooms();
            let mut response = String::from("*** Rooms: ***\n");
            for (name, count, topic) in &rooms {
                let topic_str = topic.as_deref().unwrap_or("");
                response.push_str(
                    &format!("  {} ({} members) {}\n", name, count, topic_str)
                );
            }
            response.push_str("***");
            ClientAction::SendDirect(response)
        }

        Command::Who(room_name) => {
            match state.room_members(&room_name) {
                Ok(members) => {
                    let msg = format!(
                        "*** Members of {}: {} ***",
                        room_name,
                        members.join(", ")
                    );
                    ClientAction::SendDirect(msg)
                }
                Err(e) => ClientAction::SendDirect(format!("!ERR {}", e)),
            }
        }

        Command::Topic(new_topic) => {
            let active_room = state.get_active_room(addr)
                .unwrap_or(DEFAULT_ROOM).to_string();

            match new_topic {
                None => {
                    // Mostrar el topic actual
                    let topic = state.rooms.get(&active_room)
                        .and_then(|r| r.topic.as_deref())
                        .unwrap_or("(no topic set)");
                    ClientAction::SendDirect(
                        format!("*** Topic for {}: {} ***", active_room, topic)
                    )
                }
                Some(topic) => {
                    let nickname = state.get_nickname(addr)
                        .unwrap_or("???").to_string();

                    match state.set_topic(addr, &active_room, topic.clone()) {
                        Ok(members) => {
                            let msg = format!(
                                "*** {} set topic for {} to: {} ***",
                                nickname, active_room, topic
                            );
                            for member in &members {
                                if let Some(client) = state.clients.get(member) {
                                    let _ = client.direct_tx.send(
                                        format!("{}\n", msg)
                                    );
                                }
                            }
                            ClientAction::None
                        }
                        Err(e) => ClientAction::SendDirect(format!("!ERR {}", e)),
                    }
                }
            }
        }

        // Otros comandos...
        _ => ClientAction::None,
    }
}

enum ClientAction {
    None,
    SendDirect(String),
    Disconnect,
}
```

### Envío de mensajes normales con sala

```rust
// Cuando el usuario envía un mensaje normal (no un comando):

async fn handle_message(
    content: String,
    addr: &SocketAddr,
    state: &SharedState,
) {
    let state = state.lock().await;
    let nickname = state.get_nickname(addr)
        .unwrap_or("???").to_string();
    let active_room = state.get_active_room(addr)
        .unwrap_or(DEFAULT_ROOM).to_string();

    let formatted = format!("[{}][{}] {}", active_room, nickname, content);
    state.send_to_room(&active_room, addr, &formatted);
}
```

### Diagrama de flujo completo

```
┌──────────────────────────────────────────────────────────────┐
│  Flujo de un mensaje con salas                               │
│                                                              │
│  Alice escribe: "Hello!"                                    │
│  (active_room = #rust)                                       │
│         │                                                    │
│         ▼                                                    │
│  parse_input("Hello!") → UserInput::Message("Hello!")        │
│         │                                                    │
│         ▼                                                    │
│  state.lock() → get active_room → "#rust"                    │
│         │                                                    │
│         ▼                                                    │
│  formatted = "[#rust][Alice] Hello!"                         │
│         │                                                    │
│         ▼                                                    │
│  state.send_to_room("#rust", alice_addr, formatted)          │
│         │                                                    │
│         ▼                                                    │
│  room.members() = {alice, bob}                               │
│  filter out alice (sender)                                   │
│  bob.direct_tx.send("[#rust][Alice] Hello!\n")               │
│         │                                                    │
│         ▼                                                    │
│  Bob ve: [#rust][Alice] Hello!                               │
│  Carol (no está en #rust): no ve nada                       │
└──────────────────────────────────────────────────────────────┘
```

---

## Mejoras al cliente

### Indicador de sala activa

En el cliente TUI de T02, mostramos la sala activa en la barra de título
y en el prompt:

```rust
// Barra de título dinámica
fn render_title_bar(
    out: &mut impl Write,
    active_room: &str,
    cols: u16,
) -> std::io::Result<()> {
    let title = format!(" === {} === ", active_room);
    queue!(
        out,
        cursor::MoveTo(0, 0),
        terminal::Clear(terminal::ClearType::CurrentLine),
        style::Print(&title),
    )?;
    Ok(())
}

// Prompt con sala activa
fn render_input(
    out: &mut impl Write,
    input: &InputLine,
    active_room: &str,
    cols: u16,
    rows: u16,
) -> std::io::Result<()> {
    let prompt = format!("{} > ", active_room);
    queue!(
        out,
        cursor::MoveTo(0, rows - 1),
        terminal::Clear(terminal::ClearType::CurrentLine),
        style::Print(&prompt),
    )?;
    // ... resto del render de input ...
    Ok(())
}
```

### Detectar cambio de sala activa desde el servidor

Cuando el servidor responde a `/join` o `/switch`, el cliente debe actualizar
el indicador de sala activa. Una forma simple: parsear los mensajes del sistema:

```rust
fn extract_active_room(server_message: &str) -> Option<String> {
    // "*** Joined #rust (3 members) — Topic: ... ***"
    if server_message.contains("Joined ") {
        let after = server_message.split("Joined ").nth(1)?;
        let room = after.split_whitespace().next()?;
        if room.starts_with('#') {
            return Some(room.to_string());
        }
    }

    // "*** Switched to #general ***"
    if server_message.contains("Switched to ") {
        let after = server_message.split("Switched to ").nth(1)?;
        let room = after.trim_end_matches(" ***").trim();
        if room.starts_with('#') {
            return Some(room.to_string());
        }
    }

    None
}
```

Una alternativa más robusta: definir un protocolo binario o con prefijos
estructurados (ej: `ROOM:#rust` como prefijo de las respuestas del servidor).
Pero para un chat de texto simple, el parsing del formato legible es suficiente.

### Historial por sala

Para una experiencia completa, el cliente puede mantener un historial separado
por sala:

```rust
use std::collections::HashMap;

struct ChatClient {
    histories: HashMap<String, MessageHistory>,
    active_room: String,
}

impl ChatClient {
    fn new() -> Self {
        let mut histories = HashMap::new();
        histories.insert("#lobby".to_string(), MessageHistory::new(10_000));

        Self {
            histories,
            active_room: "#lobby".to_string(),
        }
    }

    fn push_message(&mut self, room: &str, message: String) {
        self.histories
            .entry(room.to_string())
            .or_insert_with(|| MessageHistory::new(10_000))
            .push(message);
    }

    fn active_history(&mut self) -> &mut MessageHistory {
        self.histories
            .entry(self.active_room.clone())
            .or_insert_with(|| MessageHistory::new(10_000))
    }

    fn switch_room(&mut self, room: &str) {
        self.active_room = room.to_string();
        // Crear historial si no existe
        self.histories
            .entry(room.to_string())
            .or_insert_with(|| MessageHistory::new(10_000));
    }
}
```

---

## Errores comunes

### 1. Olvidar sincronizar los dos sets (Room.members y ClientInfo.rooms)

```rust
// ❌ Agregar a la sala pero no al cliente
fn join_room(&mut self, addr: &SocketAddr, room: &str) {
    if let Some(room) = self.rooms.get_mut(room) {
        room.add_member(*addr);
        // FALTA: client.rooms.insert(room_name)
    }
}
// El usuario no sabe que está en la sala, /leave falla

// ✅ Siempre actualizar ambos lados
fn join_room(&mut self, addr: &SocketAddr, room_name: &str) {
    let client = self.clients.get_mut(addr).unwrap();
    client.rooms.insert(room_name.to_string());  // Set del cliente
    client.active_room = room_name.to_string();

    let room = self.rooms.entry(room_name.to_string())
        .or_insert_with(|| Room::new(room_name.to_string()));
    room.add_member(*addr);  // Set de la sala
}
```

### 2. Sostener el lock de Mutex durante I/O async

```rust
// ❌ Lock sostenido durante await: bloquea todas las otras tasks
let state = state.lock().await;
let members = state.room_members("#rust")?;
for member in members {
    // Este await es con el lock aún sostenido
    writer.write_all(msg.as_bytes()).await?;
}
// Todas las demás tasks esperan el lock durante cada write

// ✅ Recolectar los senders dentro del lock, enviar fuera
let senders = {
    let state = state.lock().await;
    state.room_recipients("#rust", &addr)
        .iter()
        .filter_map(|a| state.clients.get(a))
        .map(|c| c.direct_tx.clone())
        .collect::<Vec<_>>()
}; // Lock liberado aquí

for tx in senders {
    let _ = tx.send(formatted_msg.clone());
}
```

### 3. No manejar el caso de que el usuario no esté en ninguna sala

```rust
// ❌ Si leave_room deja al usuario sin salas, active_room apunta a nada
fn leave_room(&mut self, addr: &SocketAddr, room: &str) {
    let client = self.clients.get_mut(addr).unwrap();
    client.rooms.remove(room);
    // Si rooms está vacío, active_room queda como string fantasma
    // Mensajes futuros van a una sala donde no está el usuario
}

// ✅ Siempre asegurar que el usuario está en al menos una sala
if client.rooms.is_empty() {
    client.rooms.insert(DEFAULT_ROOM.to_string());
    client.active_room = DEFAULT_ROOM.to_string();
    if let Some(room) = self.rooms.get_mut(DEFAULT_ROOM) {
        room.add_member(*addr);
    }
}
```

### 4. Race condition al crear salas

```rust
// ❌ Verificar y crear en dos pasos separados
let state = state.lock().await;
let exists = state.rooms.contains_key("#new");
drop(state); // Liberamos el lock

// Otra task puede crear #new aquí

let mut state = state.lock().await;
if !exists {
    state.rooms.insert("#new".into(), Room::new("#new".into()));
    // ¿Ya existía? Sobreescribimos y perdimos miembros
}

// ✅ Usar entry() para check-and-insert atómico (dentro del mismo lock)
let mut state = state.lock().await;
let room = state.rooms
    .entry("#new".to_string())
    .or_insert_with(|| Room::new("#new".to_string()));
room.add_member(*addr);
```

### 5. No notificar a las salas correctas al desconectar

```rust
// ❌ Solo notificar al broadcast global
fn handle_disconnect(addr: &SocketAddr, state: &SharedState) {
    let mut state = state.lock().await;
    if let Some(nick) = state.remove_client(addr) {
        broadcast_tx.send(format!("*** {} has left ***", nick));
        // Los usuarios de #rust ven que "Alice left" aunque Alice
        // nunca estuvo en #rust — confuso
    }
}

// ✅ Notificar a cada sala donde estaba el usuario
fn handle_disconnect(addr: &SocketAddr, state: &SharedState) {
    let mut state = state.lock().await;
    if let Some(disconnect) = state.remove_client(addr) {
        for (room_name, members) in &disconnect.affected_rooms {
            let msg = format!(
                "*** {} has left {} ***",
                disconnect.nickname, room_name
            );
            for member_addr in members {
                if let Some(client) = state.clients.get(member_addr) {
                    let _ = client.direct_tx.send(format!("{}\n", msg));
                }
            }
        }
    }
}
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│            SALAS Y CANALES CHEATSHEET                        │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  COMANDOS DEL USUARIO                                        │
│  /join #room          Unirse (crea si no existe)            │
│  /leave [#room]       Salir (default: sala activa)          │
│  /switch #room        Cambiar sala activa                   │
│  /rooms               Listar todas las salas                │
│  /who #room           Miembros de una sala                  │
│  /topic [text]        Ver/cambiar topic                      │
│  /msg user text       Mensaje privado (inter-sala)          │
│                                                              │
│  MODELO DE DATOS                                             │
│  Room { name, topic, members: HashSet<SocketAddr> }          │
│  ClientInfo {                                                │
│    nickname, direct_tx,                                      │
│    rooms: HashSet<String>,                                   │
│    active_room: String,                                      │
│  }                                                           │
│  ServerState {                                               │
│    clients: HashMap<Addr, ClientInfo>,                       │
│    nick_to_addr: HashMap<String, Addr>,                      │
│    rooms: HashMap<String, Room>,                             │
│  }                                                           │
│                                                              │
│  INVARIANTES                                                 │
│  - addr ∈ room.members ↔ room.name ∈ client.rooms           │
│  - Un usuario siempre está en ≥ 1 sala                      │
│  - DEFAULT_ROOM (#lobby) siempre existe                      │
│  - Salas vacías se eliminan (excepto #lobby)                │
│                                                              │
│  ENRUTAMIENTO                                                │
│  send_to_room(room, sender, msg)    A todos menos sender    │
│  broadcast_to_room(room, msg)       A todos incluido sender │
│  client.direct_tx.send(msg)         Mensaje directo          │
│                                                              │
│  VALIDACIÓN DE NOMBRE DE SALA                                │
│  Empieza con #                                               │
│  1-30 chars alfanumérico, - o _                             │
│  Sin espacios ni caracteres especiales                       │
│                                                              │
│  FLUJO DE /join                                              │
│  1. validate_room_name()                                     │
│  2. entry().or_insert() la sala                              │
│  3. Recolectar miembros existentes                          │
│  4. room.add_member(addr)                                    │
│  5. client.rooms.insert(name)                                │
│  6. client.active_room = name                                │
│  7. Notificar a miembros existentes                         │
│  8. Confirmar al usuario                                     │
│                                                              │
│  FLUJO DE DISCONNECT                                         │
│  1. remove_client() → ClientDisconnect                       │
│  2. Para cada (room, remaining_members):                     │
│       enviar "X has left room" a remaining_members           │
│  3. Limpiar salas vacías (retain)                           │
│                                                              │
│  ANTI-PATRONES                                               │
│  ✗ Sostener lock durante await                              │
│  ✗ Check-then-act sin lock (race condition)                 │
│  ✗ Olvidar sincronizar Room.members ↔ Client.rooms          │
│  ✗ Dejar al usuario sin sala activa                         │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Sistema de salas básico

Extiende el servidor de S01 con soporte para salas:

**Tareas:**
1. Agrega la struct `Room` y el campo `rooms` a `ServerState`
2. Modifica `add_client` para unir automáticamente a `#lobby`
3. Implementa `join_room` y `leave_room` con las notificaciones correctas
4. Implementa `send_to_room` para routing de mensajes por sala
5. Parsea los comandos `/join`, `/leave`, `/rooms`, `/who`
6. Prueba con dos clientes:
   ```
   Client A:                   Client B:
   /join #rust                 /join #rust
   Hello in rust!              ← ve "[#rust][Alice] Hello in rust!"
                               Hello back!
   ← ve "[#rust][Bob] ..."
   /leave #rust
                               ← ve "*** Alice has left #rust ***"
   Hello in lobby!             ← Bob NO ve esto (está en #rust)
   ```
7. Verifica que desconectar un cliente lo remueve de todas las salas
   y notifica a cada sala por separado

**Pregunta de reflexión:** ¿Por qué usamos `HashMap<String, Room>` en vez
de `HashMap<String, broadcast::Sender<String>>` (un broadcast channel por sala)?
¿En qué escenario el broadcast por sala sería más eficiente?

---

### Ejercicio 2: Switch y topic

Agrega funcionalidad de sala activa y topics:

**Tareas:**
1. Implementa `/switch` para cambiar la sala activa sin unirse/salir
2. Implementa `/topic` para ver y cambiar el topic de la sala activa
3. Cuando un usuario hace `/join`, muestra el topic y el número de miembros
4. Modifica `/rooms` para mostrar el topic de cada sala:
   ```
   *** Rooms: ***
     #lobby (3 members) Welcome to the chat!
     #rust (2 members) Rust programming
     #help (1 member) (no topic)
   ***
   ```
5. En el cliente TUI, muestra la sala activa en la barra de título:
   ```
   === #rust (2 members) — Rust programming ===
   ```
6. Bonus: permite que `/join #room` acepte el nombre sin `#`:
   `/join rust` equivale a `/join #rust`

**Pregunta de reflexión:** ¿Debería existir un rol de "operador de sala"
que sea el único con permiso para cambiar el topic? ¿Cómo implementarías
un sistema de permisos por sala sin complicar excesivamente el código?

---

### Ejercicio 3: Mensajes privados inter-sala

Verifica que `/msg` funciona entre usuarios de diferentes salas y agrega
notificaciones cruzadas:

**Tareas:**
1. Confirma que `/msg Alice Hello` funciona aunque Alice esté en `#rust`
   y Bob en `#lobby` (los mensajes privados no dependen de la sala)
2. Agrega `/me` con contexto de sala:
   ```
   /me waves
   → Los miembros de la sala activa ven: "* Alice waves"
   ```
3. Al hacer `/nick`, notifica a **todas las salas** donde el usuario está:
   ```
   # Si Alice está en #lobby y #rust:
   # Miembros de #lobby ven: "*** Alice is now known as Ally ***"
   # Miembros de #rust ven:  "*** Alice is now known as Ally ***"
   ```
4. Implementa un mensaje de bienvenida al unirse a una sala que muestre
   los últimos 5 miembros activos (o un resumen de la actividad reciente)
5. Bonus: agrega `/invite user #room` que envía un mensaje privado al
   usuario invitándolo a unirse

**Pregunta de reflexión:** En IRC, los mensajes van a una sala y los
mensajes privados van entre usuarios. Nuestro `/msg` es directo (usuario
a usuario). ¿Cuáles son las implicaciones de que un mensaje privado no
tenga contexto de sala? ¿Cómo manejaría un cliente TUI con historial
por sala la recepción de un mensaje privado?
