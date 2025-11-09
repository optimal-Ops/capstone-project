# Network File Sharing System

A simple C++ client-server application to share files over a network.  
Clients can upload, download, and list files on the server.

---

## Features

- Upload and download files
- List files on server
- User authentication (username/password)
- Safe file handling with threads

---

## Setup

### Server

g++ server.cpp utils.cpp -o server -lpthread

./server

### Client

g++ client.cpp -o client

./client

_Enter username and password_

---

## Commands

- `LIST` – Show files on server
- `UPLOAD <filename>` – Upload a file
- `DOWNLOAD <filename>` – Download a file
- `EXIT` – Close client
