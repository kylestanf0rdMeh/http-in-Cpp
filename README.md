# Server Application README

## Overview

This server application is designed to handle HTTP requests, supporting both GET and POST methods. It serves files from a specified directory, supports gzip compression, and can handle multiple client connections concurrently using threads.

## Features

- **GET Requests**:
  - Serve files from a specified directory.
  - Echo back content sent to `/echo/`.
  - Return the `User-Agent` header value.
  - Gzip compression support for responses.

- **POST Requests**:
  - Save files to a specified directory.

- **Concurrency**:
  - Handles multiple client connections using threads.

## Endpoints

### GET Requests

1. **Root Path (`/`)**:
   - Returns a simple "200 OK" message.
   - Supports gzip compression.

   **Example**:
   ```sh
   GET /
   ```

2. **Echo Path (`/echo/{content}`)**:
   - Echoes back the content provided in the URL.
   - Supports gzip compression.

   **Example**:
   ```sh
   GET /echo/HelloWorld
   ```

3. **User-Agent Path (`/user-agent`)**:
   - Returns the `User-Agent` header value from the request.
   - Supports gzip compression.

   **Example**:
   ```sh
   GET /user-agent
   ```

4. **Files Path (`/files/{filename}`)**:
   - Serves the file specified by `{filename}` from the server's directory.
   - Supports gzip compression.

   **Example**:
   ```sh
   GET /files/example.txt
   ```

### POST Requests

1. **Files Path (`/files/{filename}`)**:
   - Saves the content of the request body to the file specified by `{filename}` in the server's directory.

   **Example**:
   ```sh
   POST /files/example.txt
   ```

## Usage

### Starting the Server

To start the server, use the following command:

```
sh
./server --directory <path>
```
- `<path>`: The directory where files will be served from and saved to.

**Example**:
```
sh
./server --directory /path/to/directory
```


### Example Requests

#### GET Request to Root Path

```
sh
curl http://localhost:4221/
```

#### GET Request to Echo Path

```
sh
curl http://localhost:4221/echo/HelloWorld
```


#### GET Request to User-Agent Path
```
sh
curl http://localhost:4221/user-agent
```


#### GET Request to Files Path
```
sh
curl http://localhost:4221/files/example.txt
```


## Dependencies

- **zlib**: For gzip compression.
- **pthread**: For threading support (on non-Windows systems).

## Platform Support

- **Windows**: Uses Winsock2 for socket operations.
- **Linux/Unix**: Uses standard POSIX socket operations.

## Compilation

To compile the server, ensure you have a C++ compiler installed and run:
```
sh
g++ -o server src/server.cpp -lz -lpthread
```


- `-lz`: Links the zlib library.
- `-lpthread`: Links the pthread library (on non-Windows systems).

## Notes

- Ensure the specified directory exists and has the necessary read/write permissions.
- The server listens on port `4221` by default.
- The buffer size for receiving messages is set to `1024` bytes.