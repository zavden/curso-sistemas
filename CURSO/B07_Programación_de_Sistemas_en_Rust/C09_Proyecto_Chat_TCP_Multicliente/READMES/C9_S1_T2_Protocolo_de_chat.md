# Protocolo de chat: framing, nicknames y comandos

## Índice

1. [Diseñar un protocolo de aplicación](#diseñar-un-protocolo-de-aplicación)
2. [Framing de mensajes](#framing-de-mensajes)
3. [Tipos de mensaje](#tipos-de-mensaje)
4. [Sistema de nicknames](#sistema-de-nicknames)
5. [Comandos del chat](#comandos-del-chat)
6. [Parseo de mensajes entrantes](#parseo-de-mensajes-entrantes)
7. [Formateo de mensajes salientes](#formateo-de-mensajes-salientes)
8. [Integración con el servidor](#integración-con-el-servidor)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Diseñar un protocolo de aplicación

En T01 construimos un servidor que retransmite líneas de texto. Pero un chat
real necesita más estructura: nicknames, comandos, mensajes del sistema.
Necesitamos un **protocolo**: reglas que definen cómo cliente y servidor
se comunican.

```
┌──────────────────────────────────────────────────────────────┐
│  Sin protocolo (T01):                                        │
│  Cliente envía: "Hello everyone"                             │
│  Servidor broadcast: "127.0.0.1:54321: Hello everyone"       │
│  → Feo, no informativo, sin funcionalidad extra              │
│                                                              │
│  Con protocolo (T02):                                        │
│  Cliente envía: "Hello everyone"                             │
│  Servidor broadcast: "[Alice] Hello everyone"                │
│  Cliente envía: "/nick Bob"                                  │
│  Servidor responde: "*** You are now known as Bob ***"       │
│  Cliente envía: "/list"                                      │
│  Servidor responde: "*** Online: Alice, Bob, Charlie ***"    │
│  Cliente envía: "/quit"                                      │
│  Servidor broadcast: "*** Bob has left the chat ***"         │
└──────────────────────────────────────────────────────────────┘
```

### Principios de diseño del protocolo

```
┌──────────────────────────────────────────────────────────────┐
│  1. Simple: texto plano, una línea = un mensaje              │
│     (fácil de debuggear con nc y leer en logs)               │
│                                                              │
│  2. Extensible: los comandos empiezan con /                  │
│     (agregar nuevos comandos no rompe los existentes)        │
│                                                              │
│  3. Explícito: el servidor distingue mensajes de chat        │
│     de mensajes del sistema (anuncios, errores)              │
│                                                              │
│  4. Robusto: input inválido produce un error descriptivo,    │
│     no un crash                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Framing de mensajes

### Protocolo basado en líneas

Nuestro protocolo usa **una línea por mensaje**, terminada en `\n`:

```
┌──────────────────────────────────────────────────────────────┐
│  Cliente → Servidor (input del usuario):                     │
│                                                              │
│  Texto normal     → mensaje de chat                          │
│  /comando [args]  → comando del sistema                      │
│                                                              │
│  Servidor → Cliente (output):                                │
│                                                              │
│  [nick] texto     → mensaje de chat de otro usuario          │
│  *** texto ***    → mensaje del sistema (join, leave, etc.)  │
│  !ERR texto       → mensaje de error                         │
└──────────────────────────────────────────────────────────────┘
```

### Implementar el framing con tokio-util (opcional)

Para protocolos más complejos, `tokio-util` ofrece `LinesCodec`:

```toml
# Cargo.toml
[dependencies]
tokio-util = { version = "0.7", features = ["codec"] }
```

```rust
use tokio_util::codec::{Framed, LinesCodec};
use tokio::net::TcpStream;
use futures::{SinkExt, StreamExt};

async fn handle_with_codec(socket: TcpStream) {
    // LinesCodec: automáticamente lee/escribe líneas completas
    // con límite de longitud máxima (protección)
    let mut framed = Framed::new(socket, LinesCodec::new_with_max_length(1024));

    while let Some(result) = framed.next().await {
        match result {
            Ok(line) => {
                // line ya es un String completo, sin \n
                let response = format!("Echo: {}", line);
                framed.send(response).await.unwrap();
            }
            Err(e) => {
                eprintln!("Codec error: {}", e);
                break;
            }
        }
    }
}
```

Para nuestro chat, usaremos `read_line` directamente (más simple, sin
dependencia extra). Pero `LinesCodec` es la opción preferida para protocolos
de producción porque maneja automáticamente:
- Líneas parciales (TCP puede entregar fragmentos)
- Límite de longitud (protección contra DoS)
- Encoding/decoding simétrico

### Límite de longitud de mensaje

```rust
const MAX_LINE_LENGTH: usize = 1024;

/// Read a line with length limit
async fn read_line_limited(
    reader: &mut tokio::io::BufReader<tokio::net::tcp::OwnedReadHalf>,
    buf: &mut String,
) -> Result<usize, ChatError> {
    buf.clear();
    let bytes = reader.read_line(buf).await
        .map_err(|e| ChatError::Io(e))?;

    if bytes == 0 {
        return Ok(0);  // EOF
    }

    if buf.len() > MAX_LINE_LENGTH {
        buf.clear();
        return Err(ChatError::MessageTooLong);
    }

    Ok(bytes)
}
```

---

## Tipos de mensaje

### Enum para mensajes internos

```rust
/// Messages that flow through the broadcast channel
#[derive(Debug, Clone)]
pub enum ChatMessage {
    /// A regular chat message from a user
    UserMessage {
        sender: String,   // nickname
        content: String,
    },

    /// System announcement visible to all
    SystemAnnounce(String),

    /// A user joined the chat
    UserJoined {
        nickname: String,
    },

    /// A user left the chat
    UserLeft {
        nickname: String,
    },

    /// A user changed their nickname
    NickChange {
        old_nick: String,
        new_nick: String,
    },
}

impl ChatMessage {
    /// Format the message for display to a client
    pub fn format_for_display(&self) -> String {
        match self {
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
        }
    }

    /// Should this message be sent to the given user?
    pub fn should_send_to(&self, nickname: &str) -> bool {
        match self {
            // Don't echo back to sender
            ChatMessage::UserMessage { sender, .. } => sender != nickname,
            // System messages go to everyone
            _ => true,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_user_message_format() {
        let msg = ChatMessage::UserMessage {
            sender: "Alice".to_string(),
            content: "Hello!".to_string(),
        };
        assert_eq!(msg.format_for_display(), "[Alice] Hello!");
    }

    #[test]
    fn test_system_announce_format() {
        let msg = ChatMessage::SystemAnnounce("Server restart in 5 min".to_string());
        assert_eq!(msg.format_for_display(), "*** Server restart in 5 min ***");
    }

    #[test]
    fn test_should_send_to_filters_sender() {
        let msg = ChatMessage::UserMessage {
            sender: "Alice".to_string(),
            content: "Hi".to_string(),
        };
        assert!(!msg.should_send_to("Alice"));  // No eco
        assert!(msg.should_send_to("Bob"));     // Sí a otros
    }

    #[test]
    fn test_system_messages_go_to_all() {
        let msg = ChatMessage::UserJoined { nickname: "Alice".to_string() };
        assert!(msg.should_send_to("Alice"));
        assert!(msg.should_send_to("Bob"));
    }
}
```

### Mensajes de error (servidor → cliente)

```rust
/// Error messages sent to a specific client (not broadcast)
#[derive(Debug)]
pub enum ServerResponse {
    /// Success acknowledgment
    Ok(String),

    /// Error message
    Error(String),

    /// List of online users
    UserList(Vec<String>),
}

impl ServerResponse {
    pub fn format_for_display(&self) -> String {
        match self {
            ServerResponse::Ok(msg) => {
                format!("*** {} ***", msg)
            }
            ServerResponse::Error(msg) => {
                format!("!ERR {}", msg)
            }
            ServerResponse::UserList(users) => {
                format!("*** Online: {} ***", users.join(", "))
            }
        }
    }
}
```

---

## Sistema de nicknames

### Asignar nickname inicial

```rust
use std::net::SocketAddr;
use std::sync::atomic::{AtomicU64, Ordering};

static NEXT_ID: AtomicU64 = AtomicU64::new(1);

/// Generate a default nickname for a new connection
fn default_nickname() -> String {
    let id = NEXT_ID.fetch_add(1, Ordering::Relaxed);
    format!("User{}", id)
}
```

O pedir el nickname al conectar:

```rust
async fn ask_nickname(
    reader: &mut tokio::io::BufReader<tokio::net::tcp::OwnedReadHalf>,
    writer: &mut tokio::net::tcp::OwnedWriteHalf,
) -> Result<String, ChatError> {
    use tokio::io::{AsyncBufReadExt, AsyncWriteExt};

    writer.write_all(b"Enter your nickname: ").await?;

    let mut nick = String::new();
    let bytes = reader.read_line(&mut nick).await?;

    if bytes == 0 {
        return Err(ChatError::Disconnected);
    }

    let nick = nick.trim().to_string();

    if nick.is_empty() {
        return Ok(default_nickname());
    }

    validate_nickname(&nick)?;
    Ok(nick)
}
```

### Validar nicknames

```rust
/// Rules for valid nicknames
fn validate_nickname(nick: &str) -> Result<(), ChatError> {
    if nick.is_empty() {
        return Err(ChatError::InvalidNickname("Nickname cannot be empty".into()));
    }

    if nick.len() > 20 {
        return Err(ChatError::InvalidNickname("Nickname too long (max 20 chars)".into()));
    }

    if nick.starts_with('/') {
        return Err(ChatError::InvalidNickname(
            "Nickname cannot start with /".into()
        ));
    }

    if nick.contains(char::is_whitespace) {
        return Err(ChatError::InvalidNickname(
            "Nickname cannot contain spaces".into()
        ));
    }

    // Only alphanumeric, underscore, and hyphen
    if !nick.chars().all(|c| c.is_alphanumeric() || c == '_' || c == '-') {
        return Err(ChatError::InvalidNickname(
            "Nickname can only contain letters, numbers, _ and -".into()
        ));
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_valid_nicknames() {
        assert!(validate_nickname("Alice").is_ok());
        assert!(validate_nickname("Bob_123").is_ok());
        assert!(validate_nickname("my-nick").is_ok());
    }

    #[test]
    fn test_invalid_nicknames() {
        assert!(validate_nickname("").is_err());
        assert!(validate_nickname("/admin").is_err());
        assert!(validate_nickname("has space").is_err());
        assert!(validate_nickname("too_long_nickname_that_exceeds_limit").is_err());
        assert!(validate_nickname("special!char").is_err());
    }
}
```

### Verificar unicidad

La verificación de que un nickname no está en uso requiere acceso al estado
compartido (que cubriremos en T03). Por ahora, definimos la interfaz:

```rust
/// Check if a nickname is available (interface — implementation in T03)
pub trait NicknameRegistry {
    fn is_available(&self, nick: &str) -> bool;
    fn register(&mut self, nick: &str) -> bool;
    fn unregister(&mut self, nick: &str);
    fn rename(&mut self, old: &str, new: &str) -> bool;
    fn list_all(&self) -> Vec<String>;
}
```

---

## Comandos del chat

### Definir los comandos

```rust
/// Parsed command from user input
#[derive(Debug, PartialEq)]
pub enum Command {
    /// /nick <new_nickname>
    Nick(String),

    /// /quit [message]
    Quit(Option<String>),

    /// /list
    List,

    /// /help
    Help,

    /// /msg <user> <message> (private message)
    Msg {
        target: String,
        content: String,
    },

    /// /me <action> (emote)
    Me(String),
}

/// Result of parsing user input
#[derive(Debug)]
pub enum UserInput {
    /// A regular chat message
    Message(String),

    /// A command
    Command(Command),
}
```

### Parsear input del usuario

```rust
/// Parse a line of user input into a message or command
pub fn parse_input(input: &str) -> Result<UserInput, ChatError> {
    let input = input.trim();

    if input.is_empty() {
        return Err(ChatError::EmptyMessage);
    }

    if !input.starts_with('/') {
        return Ok(UserInput::Message(input.to_string()));
    }

    // Split command and arguments
    let mut parts = input[1..].splitn(2, char::is_whitespace);
    let command = parts.next().unwrap_or("");
    let args = parts.next().unwrap_or("").trim();

    match command.to_lowercase().as_str() {
        "nick" => {
            if args.is_empty() {
                Err(ChatError::MissingArgument("Usage: /nick <nickname>".into()))
            } else {
                let nick = args.split_whitespace().next().unwrap();
                validate_nickname(nick)?;
                Ok(UserInput::Command(Command::Nick(nick.to_string())))
            }
        }

        "quit" | "exit" => {
            let msg = if args.is_empty() { None } else { Some(args.to_string()) };
            Ok(UserInput::Command(Command::Quit(msg)))
        }

        "list" | "who" | "users" => {
            Ok(UserInput::Command(Command::List))
        }

        "help" | "?" => {
            Ok(UserInput::Command(Command::Help))
        }

        "msg" | "pm" | "whisper" => {
            let mut parts = args.splitn(2, char::is_whitespace);
            let target = parts.next()
                .filter(|s| !s.is_empty())
                .ok_or(ChatError::MissingArgument("Usage: /msg <user> <message>".into()))?;
            let content = parts.next()
                .filter(|s| !s.is_empty())
                .ok_or(ChatError::MissingArgument("Usage: /msg <user> <message>".into()))?;
            Ok(UserInput::Command(Command::Msg {
                target: target.to_string(),
                content: content.to_string(),
            }))
        }

        "me" => {
            if args.is_empty() {
                Err(ChatError::MissingArgument("Usage: /me <action>".into()))
            } else {
                Ok(UserInput::Command(Command::Me(args.to_string())))
            }
        }

        unknown => {
            Err(ChatError::UnknownCommand(unknown.to_string()))
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_regular_message() {
        let input = parse_input("Hello everyone!").unwrap();
        assert!(matches!(input, UserInput::Message(m) if m == "Hello everyone!"));
    }

    #[test]
    fn test_parse_nick_command() {
        let input = parse_input("/nick Bob").unwrap();
        assert!(matches!(input, UserInput::Command(Command::Nick(n)) if n == "Bob"));
    }

    #[test]
    fn test_parse_nick_missing_arg() {
        let result = parse_input("/nick");
        assert!(result.is_err());
    }

    #[test]
    fn test_parse_quit_no_message() {
        let input = parse_input("/quit").unwrap();
        assert!(matches!(input, UserInput::Command(Command::Quit(None))));
    }

    #[test]
    fn test_parse_quit_with_message() {
        let input = parse_input("/quit See you later!").unwrap();
        assert!(matches!(
            input,
            UserInput::Command(Command::Quit(Some(m))) if m == "See you later!"
        ));
    }

    #[test]
    fn test_parse_list() {
        let input = parse_input("/list").unwrap();
        assert!(matches!(input, UserInput::Command(Command::List)));
    }

    #[test]
    fn test_parse_msg() {
        let input = parse_input("/msg Alice Hey, how are you?").unwrap();
        assert!(matches!(
            input,
            UserInput::Command(Command::Msg { target, content })
            if target == "Alice" && content == "Hey, how are you?"
        ));
    }

    #[test]
    fn test_parse_me() {
        let input = parse_input("/me waves hello").unwrap();
        assert!(matches!(
            input,
            UserInput::Command(Command::Me(a)) if a == "waves hello"
        ));
    }

    #[test]
    fn test_parse_unknown_command() {
        let result = parse_input("/unknown");
        assert!(result.is_err());
    }

    #[test]
    fn test_parse_empty() {
        let result = parse_input("");
        assert!(result.is_err());
    }

    #[test]
    fn test_parse_aliases() {
        // /who es alias de /list
        let input = parse_input("/who").unwrap();
        assert!(matches!(input, UserInput::Command(Command::List)));

        // /exit es alias de /quit
        let input = parse_input("/exit").unwrap();
        assert!(matches!(input, UserInput::Command(Command::Quit(None))));
    }
}
```

### Mensaje de ayuda

```rust
pub fn help_text() -> &'static str {
    "\
*** Available commands: ***
  /nick <name>          Change your nickname
  /list                 Show online users
  /msg <user> <text>    Send a private message
  /me <action>          Send an emote (e.g., /me waves)
  /help                 Show this help
  /quit [message]       Leave the chat
***"
}
```

---

## Parseo de mensajes entrantes

### Tipo de error del chat

```rust
use std::fmt;
use std::io;

#[derive(Debug)]
pub enum ChatError {
    Io(io::Error),
    Disconnected,
    MessageTooLong,
    EmptyMessage,
    InvalidNickname(String),
    NicknameTaken(String),
    MissingArgument(String),
    UnknownCommand(String),
    UserNotFound(String),
}

impl fmt::Display for ChatError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ChatError::Io(e) => write!(f, "I/O error: {}", e),
            ChatError::Disconnected => write!(f, "Client disconnected"),
            ChatError::MessageTooLong => write!(f, "Message too long"),
            ChatError::EmptyMessage => write!(f, "Empty message"),
            ChatError::InvalidNickname(reason) => write!(f, "Invalid nickname: {}", reason),
            ChatError::NicknameTaken(nick) => write!(f, "Nickname '{}' is already taken", nick),
            ChatError::MissingArgument(usage) => write!(f, "{}", usage),
            ChatError::UnknownCommand(cmd) => write!(f, "Unknown command: /{}", cmd),
            ChatError::UserNotFound(nick) => write!(f, "User '{}' not found", nick),
        }
    }
}

impl std::error::Error for ChatError {}

impl From<io::Error> for ChatError {
    fn from(e: io::Error) -> Self {
        ChatError::Io(e)
    }
}

impl ChatError {
    /// Format as a message to send to the client
    pub fn to_client_message(&self) -> String {
        format!("!ERR {}\n", self)
    }
}
```

---

## Formateo de mensajes salientes

### Formatear para el wire (envío por TCP)

```rust
impl ChatMessage {
    /// Format for sending over TCP (includes newline)
    pub fn to_wire(&self) -> String {
        format!("{}\n", self.format_for_display())
    }
}

impl ServerResponse {
    /// Format for sending over TCP (includes newline)
    pub fn to_wire(&self) -> String {
        format!("{}\n", self.format_for_display())
    }
}
```

### Emotes (/me)

```rust
// /me waves hello → "* Alice waves hello"
fn format_emote(nickname: &str, action: &str) -> ChatMessage {
    ChatMessage::SystemAnnounce(format!("* {} {}", nickname, action))
}
```

---

## Integración con el servidor

### Handler de cliente con protocolo

```rust
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::TcpStream;
use tokio::sync::broadcast;
use std::net::SocketAddr;

pub async fn handle_client(
    socket: TcpStream,
    addr: SocketAddr,
    tx: broadcast::Sender<ChatMessage>,
    mut rx: broadcast::Receiver<ChatMessage>,
    // state: SharedState,  ← T03
) -> Result<(), ChatError> {
    let (reader, mut writer) = socket.into_split();
    let mut reader = BufReader::new(reader);
    let mut line = String::new();

    // Ask for nickname
    writer.write_all(b"Enter your nickname: ").await?;
    line.clear();
    let bytes = reader.read_line(&mut line).await?;
    if bytes == 0 {
        return Err(ChatError::Disconnected);
    }

    let mut nickname = line.trim().to_string();
    if nickname.is_empty() {
        nickname = default_nickname();
    } else if let Err(e) = validate_nickname(&nickname) {
        writer.write_all(e.to_client_message().as_bytes()).await?;
        nickname = default_nickname();
        let msg = format!("*** Using default nickname: {} ***\n", nickname);
        writer.write_all(msg.as_bytes()).await?;
    }

    // Announce join
    let join_msg = ChatMessage::UserJoined { nickname: nickname.clone() };
    let _ = tx.send(join_msg);

    // Send welcome
    let welcome = format!("*** Welcome, {}! Type /help for commands. ***\n", nickname);
    writer.write_all(welcome.as_bytes()).await?;

    // Main loop
    loop {
        tokio::select! {
            // Read from client
            result = read_line_limited(&mut reader, &mut line) => {
                match result {
                    Ok(0) => break,  // Disconnected
                    Ok(_) => {
                        match process_input(&line, &nickname, &tx, &mut writer).await {
                            Ok(ClientAction::Continue) => {}
                            Ok(ClientAction::Quit) => break,
                            Ok(ClientAction::ChangeNick(new_nick)) => {
                                let change = ChatMessage::NickChange {
                                    old_nick: nickname.clone(),
                                    new_nick: new_nick.clone(),
                                };
                                let _ = tx.send(change);
                                nickname = new_nick;
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

            // Receive broadcast
            result = rx.recv() => {
                match result {
                    Ok(msg) => {
                        if msg.should_send_to(&nickname) {
                            let _ = writer.write_all(
                                msg.to_wire().as_bytes()
                            ).await;
                        }
                    }
                    Err(broadcast::error::RecvError::Lagged(n)) => {
                        let warn = format!("*** Missed {} messages ***\n", n);
                        let _ = writer.write_all(warn.as_bytes()).await;
                    }
                    Err(broadcast::error::RecvError::Closed) => break,
                }
            }
        }
    }

    // Announce departure
    let leave_msg = ChatMessage::UserLeft { nickname: nickname.clone() };
    let _ = tx.send(leave_msg);

    Ok(())
}

/// What the client handler should do after processing input
enum ClientAction {
    Continue,
    Quit,
    ChangeNick(String),
}

/// Process a line of input from the client
async fn process_input(
    input: &str,
    nickname: &str,
    tx: &broadcast::Sender<ChatMessage>,
    writer: &mut tokio::net::tcp::OwnedWriteHalf,
) -> Result<ClientAction, ChatError> {
    use tokio::io::AsyncWriteExt;

    match parse_input(input)? {
        UserInput::Message(content) => {
            let msg = ChatMessage::UserMessage {
                sender: nickname.to_string(),
                content,
            };
            let _ = tx.send(msg);
            Ok(ClientAction::Continue)
        }

        UserInput::Command(Command::Nick(new_nick)) => {
            // TODO: check uniqueness via state (T03)
            let response = ServerResponse::Ok(
                format!("You are now known as {}", new_nick)
            );
            writer.write_all(response.to_wire().as_bytes()).await?;
            Ok(ClientAction::ChangeNick(new_nick))
        }

        UserInput::Command(Command::Quit(msg)) => {
            if let Some(msg) = msg {
                let farewell = ChatMessage::UserMessage {
                    sender: nickname.to_string(),
                    content: format!("(leaving: {})", msg),
                };
                let _ = tx.send(farewell);
            }
            Ok(ClientAction::Quit)
        }

        UserInput::Command(Command::List) => {
            // TODO: get list from shared state (T03)
            let response = ServerResponse::Ok("User list requires shared state (T03)".into());
            writer.write_all(response.to_wire().as_bytes()).await?;
            Ok(ClientAction::Continue)
        }

        UserInput::Command(Command::Help) => {
            writer.write_all(help_text().as_bytes()).await?;
            writer.write_all(b"\n").await?;
            Ok(ClientAction::Continue)
        }

        UserInput::Command(Command::Msg { target, content }) => {
            // TODO: private messages require shared state (T03)
            let _ = (target, content);
            let response = ServerResponse::Error(
                "Private messages require shared state (T03)".into()
            );
            writer.write_all(response.to_wire().as_bytes()).await?;
            Ok(ClientAction::Continue)
        }

        UserInput::Command(Command::Me(action)) => {
            let emote = format_emote(nickname, &action);
            let _ = tx.send(emote);
            Ok(ClientAction::Continue)
        }
    }
}
```

### Flujo de una sesión completa

```
┌──────────────────────────────────────────────────────────────┐
│  Client                         Server                       │
│  ──────                         ──────                       │
│                      ◀── "Enter your nickname: "             │
│  "Alice\n" ──────────▶                                       │
│                      ◀── "*** Welcome, Alice! ... ***"       │
│                      ◀── "*** Alice has joined ***" (via bc) │
│                                                              │
│  "Hello!\n" ─────────▶                                       │
│                          broadcast: [Alice] Hello!           │
│                      ◀── (other clients see the msg)         │
│                                                              │
│  "/nick Ally\n" ─────▶                                       │
│                      ◀── "*** You are now known as Ally ***" │
│                          broadcast: *** Alice → Ally ***     │
│                                                              │
│  "/list\n" ──────────▶                                       │
│                      ◀── "*** Online: Ally, Bob, Carol ***"  │
│                                                              │
│  "/me waves\n" ──────▶                                       │
│                          broadcast: *** * Ally waves ***     │
│                                                              │
│  "/quit Bye!\n" ─────▶                                       │
│                          broadcast: [Ally] (leaving: Bye!)   │
│                          broadcast: *** Ally has left ***    │
│                          close connection                    │
└──────────────────────────────────────────────────────────────┘
```

---

## Errores comunes

### 1. No validar input del usuario

```rust
// ❌ Confiar en el input: un nickname con \n rompe el protocolo
let nickname = line.to_string();  // Puede contener newlines, control chars

// ✅ Validar y sanitizar
let nickname = line.trim().to_string();
validate_nickname(&nickname)?;
```

### 2. Parsear comandos con split en vez de splitn

```rust
// ❌ split sin límite: "/msg Alice Hello dear friend" → 5 partes
let parts: Vec<&str> = input.split_whitespace().collect();
// parts = ["/msg", "Alice", "Hello", "dear", "friend"]
// ¿Cómo reconstruir el mensaje?

// ✅ splitn(2) o splitn(3) para preservar el resto
let mut parts = input[1..].splitn(2, char::is_whitespace);
let command = parts.next().unwrap();  // "msg"
let args = parts.next().unwrap_or(""); // "Alice Hello dear friend"

// Luego dentro del comando /msg:
let mut parts = args.splitn(2, char::is_whitespace);
let target = parts.next().unwrap();   // "Alice"
let content = parts.next().unwrap();  // "Hello dear friend"
```

### 3. No distinguir errores locales de broadcast

```rust
// ❌ Enviar errores por el broadcast (todos los clientes lo ven)
let _ = tx.send(ChatMessage::SystemAnnounce(
    format!("Error: invalid nickname")
));

// ✅ Los errores son locales al cliente que los causó
writer.write_all(
    ChatError::InvalidNickname("...".into()).to_client_message().as_bytes()
).await?;
// Solo este cliente ve el error
```

### 4. Permitir nicknames que rompen el parseo

```rust
// ❌ Si el nickname es "[Admin]", el mensaje se ve como:
// [[Admin]] Hello  → confuso, parece un mensaje del sistema

// ✅ La validación de nicknames solo permite [a-zA-Z0-9_-]
// Esto evita que nicknames contengan [], ***, /, etc.
```

### 5. No manejar el caso de comando vacío

```rust
// ❌ El usuario escribe solo "/" → crash o comportamiento extraño
let command = &input[1..];  // String vacía
match command { ... }       // No coincide con nada → confuso

// ✅ Manejar explícitamente
if input == "/" {
    return Err(ChatError::UnknownCommand("".to_string()));
}
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│           PROTOCOLO DE CHAT CHEATSHEET                       │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FRAMING                                                     │
│  Una línea = un mensaje (delimitado por \n)                  │
│  LinesCodec (tokio-util) para framing automático             │
│  MAX_LINE_LENGTH = 1024 (protección contra DoS)              │
│                                                              │
│  FORMATO DE MENSAJES                                         │
│  [nick] texto         Chat message                           │
│  *** texto ***        System announcement                    │
│  !ERR texto           Error (solo al cliente afectado)       │
│                                                              │
│  COMANDOS                                                    │
│  /nick <name>         Cambiar nickname                       │
│  /list                Listar usuarios online                 │
│  /msg <user> <text>   Mensaje privado                        │
│  /me <action>         Emote (* nick action)                  │
│  /help                Mostrar ayuda                          │
│  /quit [message]      Desconectarse                          │
│                                                              │
│  TIPOS INTERNOS                                              │
│  ChatMessage          Enum: UserMessage, UserJoined,         │
│                       UserLeft, NickChange, SystemAnnounce   │
│  Command              Enum: Nick, Quit, List, Help, Msg, Me  │
│  UserInput            Message(String) | Command(Command)     │
│  ServerResponse       Ok, Error, UserList                    │
│  ChatError            Io, Disconnected, InvalidNickname...   │
│                                                              │
│  PARSEO                                                      │
│  parse_input(line)    → Result<UserInput, ChatError>         │
│  validate_nickname()  → Result<(), ChatError>                │
│  splitn(2, ' ')       Preservar args con espacios            │
│                                                              │
│  VALIDACIÓN DE NICKNAME                                      │
│  No vacío, máx 20 chars, no empieza con /                   │
│  Solo [a-zA-Z0-9_-], sin espacios                           │
│                                                              │
│  FLUJO                                                       │
│  1. Conectar → pedir nickname                                │
│  2. Validar → broadcast UserJoined                           │
│  3. Loop: leer línea → parse_input → procesar                │
│  4. Desconectar → broadcast UserLeft                         │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Parser de comandos

Implementa el módulo `protocol.rs` con el parser de comandos:

**Tareas:**
1. Define los enums `Command`, `UserInput`, y `ChatError`
2. Implementa `parse_input` que distingue mensajes de comandos
3. Implementa `validate_nickname` con las reglas definidas
4. Escribe tests para cada comando, incluyendo:
   - Comandos válidos con argumentos
   - Comandos sin argumentos obligatorios (debe dar error)
   - Aliases (/who → /list, /exit → /quit)
   - Comandos desconocidos
   - Mensajes normales (sin /)
   - Casos edge: "/", "/ ", "/nick ", input vacío
5. Verifica que `/msg Alice Hello dear friend` parsea correctamente
   (el mensaje completo es "Hello dear friend", no solo "Hello")

**Pregunta de reflexión:** ¿Cómo añadirías un comando `/ignore <user>` que
oculte mensajes de un usuario específico? ¿Dónde se implementaría el filtrado:
en el parser, en el handler del cliente, o en el broadcast?

---

### Ejercicio 2: Integrar protocolo con servidor

Extiende el servidor de T01 con el protocolo:

**Tareas:**
1. Reemplaza el broadcast de `String` por `ChatMessage`
2. Agrega el flujo de nickname al inicio de la conexión
3. Implementa el handler con `process_input` que soporta:
   - Mensajes normales → broadcast con `[nickname]`
   - `/nick` → cambiar nickname + broadcast del cambio
   - `/me` → broadcast de emote
   - `/help` → enviar texto de ayuda (solo al cliente)
   - `/quit` → despedida + desconexión
4. Prueba con `nc`:
   ```
   nc 127.0.0.1 8080
   Enter your nickname: Alice
   *** Welcome, Alice! Type /help for commands. ***
   Hello!
   /me dances
   /nick Ally
   /quit Bye!
   ```
5. Verifica con dos clientes que los mensajes muestran nicknames correctos

**Pregunta de reflexión:** Actualmente, si dos clientes eligen el mismo
nickname, ambos lo obtienen. ¿Qué problemas causa esto? (Pista: ¿cómo sabe
un usuario quién envió un mensaje si hay dos "Alice"?)

---

### Ejercicio 3: Protocolo binario (diseño)

Nuestro protocolo es de texto. Diseña (sin implementar) un protocolo binario
equivalente:

```
┌──────────────────────────────────────────────────────────────┐
│  Wire format propuesto:                                      │
│                                                              │
│  ┌──────┬──────┬────────────────┐                           │
│  │ Type │ Len  │ Payload        │                           │
│  │ 1B   │ 2B   │ Len bytes      │                           │
│  └──────┴──────┴────────────────┘                           │
│                                                              │
│  Type: 0x01=ChatMsg, 0x02=Command, 0x03=System, 0x04=Error  │
│  Len: u16 big-endian, longitud del payload                   │
│  Payload: UTF-8 text                                         │
└──────────────────────────────────────────────────────────────┘
```

**Tareas:**
1. Define las constantes de tipo de mensaje
2. Escribe funciones `encode(msg: &ChatMessage) -> Vec<u8>` y
   `decode(bytes: &[u8]) -> Result<ChatMessage, Error>` (sin TCP, solo la
   serialización)
3. Escribe tests que verifiquen roundtrip: encode → decode → mismo mensaje
4. ¿Cuál es la ventaja de un protocolo binario sobre texto?
5. ¿Cuál es la desventaja para debugging? (no puedes usar `nc` directamente)

**Pregunta de reflexión:** IRC (Internet Relay Chat) usa un protocolo de texto.
Protobuf, MessagePack, y bincode usan formatos binarios. Discord usa WebSockets
con JSON (texto sobre binario). ¿Qué factores determinan la elección entre
texto y binario para un protocolo de aplicación?