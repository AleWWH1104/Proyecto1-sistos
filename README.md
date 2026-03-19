# Chat - Proyecto 1 Sistemas Operativos

Aplicación de chat cliente-servidor en C++ que utiliza **sockets TCP**, **multithreading** y **Protocol Buffers** para la serialización de mensajes.

## Autores

- **Jonathan Diaz**
- **Anggie Quezada**
- **Iris Ayala**

> Universidad del Valle de Guatemala — Sistemas Operativos, Semestre 7

## Requisitos

- **Plataformas soportadas**: Linux, macOS, Windows (MinGW/MSYS2)
- g++ con soporte C++17
- Protocol Buffers (protoc + libprotobuf)
- pkg-config
- pthread (incluido en Linux/macOS; provisto por MinGW en Windows)

> El proyecto utiliza un `Makefile` con detección automática de sistema operativo y un header de compatibilidad `platform.h` para abstraer las diferencias entre plataformas.

### Instalación de dependencias

**Linux (Ubuntu/Debian):**
```bash
sudo apt install protobuf-compiler libprotobuf-dev pkg-config g++
```

**macOS (Homebrew):**
```bash
brew install protobuf pkg-config
```

**Windows (MSYS2 + MinGW):**
```bash
pacman -S mingw-w64-x86_64-protobuf
```

## Compilación

El proyecto utiliza un `Makefile` multiplataforma con detección automática de OS y los siguientes targets:

```bash
# Compilar todo (protos + servidor + cliente)
make all

# Solo compilar archivos .proto → gen/*.pb.h + gen/*.pb.cc
make proto

# Compilar servidor (compila protos automáticamente si es necesario)
make server

# Compilar cliente (compila protos automáticamente si es necesario)
make client

# Limpiar archivos generados y binarios
make clean
```

Los binarios se generan en la raíz del proyecto: `./server` y `./client`.

## Ejecución

```bash
# Iniciar el servidor
./server <puerto>
# Ejemplo:
./server 12345

# Conectar un cliente (en otra terminal)
./client <usuario> <ip_servidor> <puerto>
# Ejemplo:
./client juan 127.0.0.1 12345
```

### Ejemplo de sesión completa

```bash
# Terminal 1 — Servidor
./server 8080

# Terminal 2 — Cliente 1
./client alice 127.0.0.1 8080

# Terminal 3 — Cliente 2
./client bob 127.0.0.1 8080
```

## Comandos del cliente

Al conectarse, el cliente muestra la lista de comandos disponibles automáticamente.

| Comando | Descripción |
|---------|-------------|
| `<mensaje>` | Enviar mensaje al chat general (broadcast) |
| `/dm <usuario> <mensaje>` | Enviar mensaje directo a un usuario |
| `/status <1\|2\|3>` | Cambiar estado: **1**=ACTIVO, **2**=OCUPADO, **3**=INACTIVO |
| `/list` | Listar usuarios conectados (excluye INACTIVOS) |
| `/info <usuario>` | Ver información de un usuario (IP, status) |
| `/help` | Mostrar ayuda de comandos |
| `/quit` | Desconectarse del servidor y salir |

> **Nota:** Ctrl+D (EOF) también termina el cliente de forma limpia.

## Arquitectura

### Servidor (`./server <puerto>`)

- **Multithreaded**: un thread detached por cada cliente conectado (`handle_session`)
- **UserRegistry**: registro thread-safe de usuarios protegido con `std::mutex`
  - Mapa principal `username → UserInfo` y mapa reverso `fd → username` para lookup O(1)
  - No permite dos usuarios con el mismo nombre
  - No permite dos conexiones desde la misma IP
- **Inactividad**: thread detached que escanea todos los usuarios cada 30 segundos
  - Tras **60 segundos** sin actividad, el status cambia automáticamente a INACTIVO (INVISIBLE internamente)
  - Al enviar cualquier mensaje después de inactividad, el status se restaura automáticamente a ACTIVO
- **Poll-based I/O**: cada sesión usa `poll()` con timeout de 10s para lectura no bloqueante
- **Shutdown graceful**: manejo de `SIGINT`/`SIGTERM` cierra el socket de escucha; `SIGPIPE` ignorado globalmente
- **SO_REUSEADDR**: permite reinicio rápido del servidor tras un crash

### Cliente (`./client <usuario> <ip> <puerto>`)

1. Crea socket TCP y se conecta al servidor
2. Envía mensaje de registro (`MSG_REGISTER`) y espera confirmación
3. **Thread receptor** (detached): recibe y muestra mensajes del servidor en segundo plano
4. **Input loop** (thread principal): lee entrada del usuario y despacha comandos

### Protocolo de comunicación

- **Serialización**: Protocol Buffers (proto3)
- **Framing TCP**: Header de 5 bytes + payload protobuf
  ```
  [1 byte: tipo de mensaje][4 bytes: longitud big-endian][N bytes: payload protobuf]
  ```
- **14 tipos de mensaje**: 7 cliente→servidor + 5 servidor→cliente + 2 reservados

#### Tipos de mensaje

| Código | Dirección | Nombre | Descripción |
|--------|-----------|--------|-------------|
| 1 | C→S | `MSG_REGISTER` | Registro de usuario |
| 2 | C→S | `MSG_GENERAL` | Mensaje al chat general (broadcast) |
| 3 | C→S | `MSG_DM` | Mensaje directo |
| 4 | C→S | `MSG_CHANGE_STATUS` | Cambio de estado |
| 5 | C→S | `MSG_LIST_USERS` | Solicitar lista de usuarios |
| 6 | C→S | `MSG_GET_USER_INFO` | Solicitar info de un usuario |
| 7 | C→S | `MSG_QUIT` | Desconectarse |
| 10 | S→C | `MSG_SERVER_RESPONSE` | Respuesta general (éxito/error + código) |
| 11 | S→C | `MSG_ALL_USERS` | Lista de usuarios conectados |
| 12 | S→C | `MSG_FOR_DM` | DM reenviado al destinatario |
| 13 | S→C | `MSG_BROADCAST_DELIVERY` | Mensaje broadcast reenviado |
| 14 | S→C | `MSG_GET_USER_INFO_RESP` | Información de un usuario |

#### Tipo compartido (StatusEnum)

| Valor | Nombre | Descripción |
|-------|--------|-------------|
| 0 | `ACTIVE` | Activo (default al registrarse) |
| 1 | `DO_NOT_DISTURB` | Ocupado |
| 2 | `INVISIBLE` | Inactivo / No visible en `/list` |

## Estructura del proyecto

```
Proyecto1-sistos/
├── Makefile                              # Sistema de build
├── .gitignore
├── README.md                             # Este archivo
├── protos/
│   ├── common.proto                      # StatusEnum compartido
│   ├── cliente-side/                     # 7 mensajes cliente→servidor
│   │   ├── register.proto
│   │   ├── message_general.proto
│   │   ├── message_dm.proto
│   │   ├── change_status.proto
│   │   ├── list_users.proto
│   │   ├── get_user_info.proto
│   │   └── quit.proto
│   └── server-side/                      # 5 mensajes servidor→cliente
│       ├── server_response.proto
│       ├── all_users.proto
│       ├── for_dm.proto
│       ├── broadcast_messages.proto
│       └── get_user_info_response.proto
├── src/
│   ├── common/
│   │   ├── net_utils.h                   # Framing TCP (header 5 bytes) + MessageType enum
│   │   └── net_utils.cpp                 # send_message() / recv_message()
│   ├── client/
│   │   ├── client.cpp                    # Main del cliente: conexión + registro
│   │   ├── input_handler.h/.cpp          # Parsing de comandos (/dm, /status, /list, etc.)
│   │   └── receiver.h/.cpp               # Thread receptor: despacho de mensajes entrantes
│   └── server/
│       ├── server.cpp                    # Main del servidor: accept loop + inactivity checker
│       ├── session.h/.cpp                # Lógica de sesión por cliente (poll + dispatch)
│       └── user_registry.h/.cpp          # Registro thread-safe de usuarios con mutex
├── gen/                                  # Archivos .pb.h/.pb.cc generados (gitignored)
└── docs/
    ├── instructions.md                   # Requisitos del proyecto
    ├── protocol_standard.md              # Especificación detallada del protocolo
    └── README.md                         # Resumen del protocolo y estructura de protos
```

## Reglas del servidor

1. **Nombre único**: no permite dos usuarios con el mismo nombre de usuario
2. **IP única**: no permite dos conexiones desde la misma dirección IP
3. **Visibilidad en `/list`**: los usuarios con status INACTIVO/INVISIBLE **no aparecen** en la lista, pero **sí son consultables** con `/info`
4. **Inactividad automática**: tras 60 segundos sin actividad, el status cambia automáticamente a INACTIVO
5. **Restauración automática**: al enviar un mensaje después de inactividad, el status se restaura a ACTIVO
6. **IP autoritativa**: el servidor usa la IP de `accept()`, no la que envía el cliente en el mensaje de registro
7. **Registro obligatorio**: el primer mensaje de un cliente **debe** ser `MSG_REGISTER`; cualquier otro tipo es rechazado

## Detalles técnicos

- **Plataformas**: Linux, macOS, Windows (MinGW/MSYS2) — con `platform.h` para compatibilidad multiplataforma
- **Estándar C++**: C++17 (`-std=c++17`)
- **Flags de compilación**: `-Wall -Wextra -pthread`
- **Linking**: `libprotobuf` via pkg-config + `-lpthread`
- **Build system**: Makefile con detección automática de OS (`uname -s`)
- **Backlog del servidor**: 10 conexiones pendientes (`BACKLOG = 10`)
- **Intervalo de chequeo de inactividad**: 30 segundos (`INACTIVITY_CHECK_INTERVAL`)
- **Timeout de inactividad**: 60 segundos (`INACTIVITY_TIMEOUT`)
- **Poll timeout por sesión**: 10 segundos (`POLL_TIMEOUT_MS`)
