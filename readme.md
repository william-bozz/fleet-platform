# Fleet Platform (local) - plataforma para transportistas

Este proyecto es una plataforma web 100% local para registrar y analizar información de una empresa transportista.
La aplicación corre en Linux y se usa desde el navegador.

La meta final es administrar:
- camiones (quinta rueda)
- trailers (reefer, dry van, flatbed)
- conductores
- mantenimientos
- cargas (loads) y asignaciones (camión + trailer + conductor)
- combustible, distancias (km) y pagos del conductor
- dashboard con gráficas basadas en los datos ingresados

Por ahora (v0.1) ya existe un módulo funcional de camiones.

## Qué construye este repo

La app tiene 3 partes:

1) Frontend (navegador)
- Archivos HTML/JS que muestran una interfaz mínima.
- Envían y reciben datos usando HTTP + JSON.

2) Backend (servidor en C)
- Un programa en C que se queda corriendo y escuchando peticiones HTTP en 127.0.0.1:8080.
- Expone endpoints tipo API (por ejemplo /api/trucks).
- Valida datos, ejecuta consultas SQL y responde JSON.

3) Base de datos (SQLite)
- Un archivo local (data/app.db) donde se guardan los datos de forma persistente.
- Aunque cierres el servidor, los registros quedan guardados.

Flujo de comunicación:
- Navegador -> HTTP -> Backend (C) -> SQLite
- SQLite -> Backend (C) -> JSON -> Navegador

## Tecnologías usadas y por qué

- C (backend)
  - Objetivo del proyecto: lógica y backend en C.
  - Permite controlar estructura, performance, y entender a fondo cómo funciona un servidor.

- CivetWeb (vendor/civetweb)
  - Librería HTTP embebible en C.
  - Evita reimplementar sockets + parsing HTTP + routing.

- cJSON (vendor/cJSON)
  - Librería para parsear y generar JSON en C.
  - La API se comunica con JSON (requests/responses).

- SQLite (libsqlite3-dev)
  - Base de datos embebida como archivo.
  - Ideal para proyectos locales: cero configuración y persistencia inmediata.

- CMake
  - Sistema de compilación.
  - Genera el build de forma consistente sin escribir comandos largos de gcc a mano.

Nota sobre OpenSSL:
En Pop!_OS/Ubuntu modernos, CivetWeb puede requerir OpenSSL para compilar (por cambios de API).
Por eso se utiliza libssl-dev y una definición OPENSSL_API_3_0 en el build.

## Estructura del proyecto

fleet-platform/
- backend/
  - src/
    - main.c
  - vendor/
    - civetweb/
    - cJSON/
  - CMakeLists.txt
  - build/ (autogenerado)
- frontend/
  - static/
    - index.html
- data/
  - app.db (autogenerado)

Qué contiene cada parte:
- backend/src/main.c
  - inicia la base de datos SQLite
  - crea tablas si no existen
  - levanta el servidor HTTP en el puerto 8080
  - registra endpoints
  - sirve archivos estáticos del frontend

- frontend/static/index.html
  - interfaz mínima para probar el módulo de camiones
  - usa fetch() para llamar la API:
    - POST /api/trucks
    - GET /api/trucks

- data/app.db
  - base de datos local SQLite generada automáticamente

## Requisitos (Pop!_OS / Ubuntu)

Instalar dependencias:
```bash
sudo apt update
sudo apt install -y build-essential cmake git pkg-config libsqlite3-dev libssl-dev
