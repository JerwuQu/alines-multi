# Protocol

UI - Server (`alines-server`) - Program - Menuer (`alines-menu`)

Upon UI connecting to server, it will first be required to send its handshake packet.
After handshake, Server spawns a new thread and process for Program which in turn may start many Menuers.

The Server communicates with Menuers over a domain socket.
The Server will only accept one Menuer at a time.

A Menuer will upon connect send a single Menuer containing its menu, and nothing else.
If a Menuer disconnects, the Server will in turn disconnect the UI.

UI disconnecting will make Server send Close to any potentially connected Menuers until Program terminates.

## Universal

### string
- len: u16
- bytes: byte[len]

## Flow

### Init, UI->Server

#### Connection Request
- password: string

### Server->UI

#### Disconnect
- id: **0** (u8)
- message: string

#### Menu
- id: **1** (u8)
- flags: u8 (0: none, 1: multiple choice allowed, 2: custom entry allowed)
- entryCount: u16
- title: string
- menu: string[entryCount]

### UI->Server

#### Close (aka. No Selection)
- id: **0** (u8)

#### Single Selection
- id: **1** (u8)
- selection: u16

#### Multi Selection
- id: **2** (u8)
- selectionCount: u16
- selections: u16[selectionCount]

#### Custom Entry
- id: **3** (u8)
- text: string

**Repeat until disconnect**

