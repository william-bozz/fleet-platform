#define _POSIX_C_SOURCE 200809L
// Le dice al compilador que habilite ciertas cosas de POSIX (Linux),
// útil para algunas funciones y constantes.

#include <stdio.h>      // printf, fprintf
#include <stdlib.h>     // malloc, free
#include <string.h>     // strlen, strcmp, memset
#include <errno.h>      // errno
#include <sys/stat.h>   // stat, mkdir
#include <unistd.h>     // sleep (Linux/POSIX)

#include <sqlite3.h>    // API de SQLite para C

#include "civetweb.h"   // Servidor HTTP (CivetWeb)
#include "cJSON.h"      // Manejo de JSON (cJSON)

// g_db es una variable global con el "handle" (puntero) a la DB abierta.
// Se usa en los handlers para ejecutar queries.
static sqlite3 *g_db = NULL;

// Crea la carpeta "data" si no existe.
static int ensure_dir(const char *path) {
    struct stat st;
    // stat revisa si el path existe y qué tipo es.
    if (stat(path, &st) == 0) {
        // Si existe, validamos que sea un directorio.
        if (S_ISDIR(st.st_mode)) return 0;  // ok
        return -1; // existe pero no es directorio
    }
    // Si no existe, lo intentamos crear con permisos 0755.
    if (mkdir(path, 0755) != 0) return -1;
    return 0;
}

// Ejecuta SQL directo con sqlite3_exec (sin resultados).
// Se usa para PRAGMAs y CREATE TABLE.
static int db_exec(sqlite3 *db, const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite error: %s\nSQL: %s\n", err ? err : "unknown", sql);
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

// Inicializa la base de datos:
// - crea carpeta data/
// - abre/crea data/app.db
// - activa opciones útiles
// - crea tablas si no existen
static int db_init(const char *db_path) {
    if (ensure_dir("data") != 0) {
        fprintf(stderr, "No pude crear/usar carpeta data/: %s\n", strerror(errno));
        return -1;
    }

    // Abre el archivo SQLite. Si no existe, lo crea.
    int rc = sqlite3_open(db_path, &g_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "No pude abrir DB: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    // foreign_keys = ON hace que SQLite respete llaves foráneas
    // (cuando luego tengamos relaciones entre tablas).
    db_exec(g_db, "PRAGMA foreign_keys = ON;");

    // WAL (Write-Ahead Logging) mejora concurrencia y estabilidad en dev.
    db_exec(g_db, "PRAGMA journal_mode = WAL;");

    // Creamos el esquema mínimo (tabla trucks).
    // IF NOT EXISTS evita errores si ya existe.
    const char *schema =
        "CREATE TABLE IF NOT EXISTS trucks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  unit_number TEXT NOT NULL,"
        "  vin TEXT,"
        "  year INTEGER,"
        "  make TEXT,"
        "  model TEXT,"
        "  engine TEXT,"
        "  status TEXT DEFAULT 'active',"
        "  current_km REAL DEFAULT 0,"
        "  created_at TEXT DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_trucks_unit ON trucks(unit_number);";

    if (db_exec(g_db, schema) != 0) return -1;

    return 0;
}

// Lee el body de una petición HTTP (POST/PUT) desde CivetWeb.
// Devuelve un buffer malloc con el contenido y out_len con el tamaño.
// Si no hay body, devuelve NULL.
static char *read_request_body(struct mg_connection *conn, long long *out_len) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    long long len = ri->content_length;

    // Si no hay body o es demasiado grande, rechazamos.
    if (len <= 0 || len > (10LL * 1024 * 1024)) {
        *out_len = 0;
        return NULL;
    }

    // Reservamos memoria para leer el body completo.
    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) return NULL;

    long long read_total = 0;
    // mg_read lee bytes del request.
    while (read_total < len) {
        int r = mg_read(conn, buf + read_total, (size_t)(len - read_total));
        if (r <= 0) break;
        read_total += r;
    }

    // Terminamos el buffer con '\0' para tratarlo como string.
    buf[read_total] = '\0';
    *out_len = read_total;
    return buf;
}

// Envía una respuesta HTTP con JSON.
static void send_json(struct mg_connection *conn, int status, const char *json) {
    const char *status_text =
        (status == 200) ? "OK" :
        (status == 201) ? "Created" :
        (status == 400) ? "Bad Request" :
        (status == 404) ? "Not Found" :
        (status == 500) ? "Internal Server Error" : "OK";

    // mg_printf escribe la respuesta completa (headers + body).
    mg_printf(conn,
              "HTTP/1.1 %d %s\r\n"
              "Content-Type: application/json\r\n"
              "Cache-Control: no-store\r\n"
              "Content-Length: %zu\r\n"
              "\r\n"
              "%s",
              status, status_text, strlen(json), json);
}

// Handler de /health (GET). Sirve para confirmar que el servidor está vivo.
static int handle_health(struct mg_connection *conn, void *cbdata) {
    (void)cbdata; // no lo usamos
    send_json(conn, 200, "{\"ok\":true,\"service\":\"fleet-platform\",\"version\":\"0.1\"}");
    return 200;
}

// GET /api/trucks
// Lee todos los camiones desde SQLite y regresa un arreglo JSON.
static int handle_trucks_list(struct mg_connection *conn) {
    const char *sql =
        "SELECT id, unit_number, vin, year, make, model, engine, status, current_km "
        "FROM trucks ORDER BY id DESC;";

    sqlite3_stmt *stmt = NULL;

    // Preparamos el statement (query).
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}");
        return 500;
    }

    // Creamos un arreglo JSON.
    cJSON *arr = cJSON_CreateArray();

    // sqlite3_step avanza fila por fila.
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *o = cJSON_CreateObject();

        // Columna 0: id
        cJSON_AddNumberToObject(o, "id", sqlite3_column_int(stmt, 0));

        // Columna 1: unit_number
        cJSON_AddStringToObject(o, "unit_number", (const char *)sqlite3_column_text(stmt, 1));

        // Columna 2: vin (puede ser NULL)
        const unsigned char *vin = sqlite3_column_text(stmt, 2);
        if (vin) cJSON_AddStringToObject(o, "vin", (const char *)vin);

        // Columna 3: year (si es 0, lo omitimos)
        int year = sqlite3_column_int(stmt, 3);
        if (year) cJSON_AddNumberToObject(o, "year", year);

        // Columna 4: make
        const unsigned char *make = sqlite3_column_text(stmt, 4);
        if (make) cJSON_AddStringToObject(o, "make", (const char *)make);

        // Columna 5: model
        const unsigned char *model = sqlite3_column_text(stmt, 5);
        if (model) cJSON_AddStringToObject(o, "model", (const char *)model);

        // Columna 6: engine
        const unsigned char *engine = sqlite3_column_text(stmt, 6);
        if (engine) cJSON_AddStringToObject(o, "engine", (const char *)engine);

        // Columna 7: status
        const unsigned char *status = sqlite3_column_text(stmt, 7);
        if (status) cJSON_AddStringToObject(o, "status", (const char *)status);

        // Columna 8: current_km
        cJSON_AddNumberToObject(o, "current_km", sqlite3_column_double(stmt, 8));

        // Agregamos el objeto del camión al arreglo.
        cJSON_AddItemToArray(arr, o);
    }

    // Liberamos el statement.
    sqlite3_finalize(stmt);

    // Convertimos el JSON a string.
    char *out = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    if (!out) {
        send_json(conn, 500, "{\"error\":\"json_encode_failed\"}");
        return 500;
    }

    send_json(conn, 200, out);
    free(out);
    return 200;
}

// POST /api/trucks
// Recibe JSON, valida, inserta en SQLite, devuelve id creado.
static int handle_trucks_create(struct mg_connection *conn) {
    long long body_len = 0;
    char *body = read_request_body(conn, &body_len);

    if (!body || body_len == 0) {
        free(body);
        send_json(conn, 400, "{\"error\":\"empty_body\"}");
        return 400;
    }

    // Parseamos JSON del request.
    cJSON *root = cJSON_Parse(body);
    free(body);

    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        send_json(conn, 400, "{\"error\":\"invalid_json\"}");
        return 400;
    }

    // unit_number es obligatorio.
    const cJSON *unit_number = cJSON_GetObjectItemCaseSensitive(root, "unit_number");
    if (!cJSON_IsString(unit_number) || unit_number->valuestring[0] == '\0') {
        cJSON_Delete(root);
        send_json(conn, 400, "{\"error\":\"unit_number_required\"}");
        return 400;
    }

    // Campos opcionales
    const cJSON *vin = cJSON_GetObjectItemCaseSensitive(root, "vin");
    const cJSON *year = cJSON_GetObjectItemCaseSensitive(root, "year");
    const cJSON *make = cJSON_GetObjectItemCaseSensitive(root, "make");
    const cJSON *model = cJSON_GetObjectItemCaseSensitive(root, "model");
    const cJSON *engine = cJSON_GetObjectItemCaseSensitive(root, "engine");
    const cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "status");
    const cJSON *current_km = cJSON_GetObjectItemCaseSensitive(root, "current_km");

    // Insert parametrizado: evita SQL injection y maneja NULLs bien.
    const char *sql =
        "INSERT INTO trucks(unit_number, vin, year, make, model, engine, status, current_km) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        cJSON_Delete(root);
        send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}");
        return 500;
    }

    // Bind de parámetros (1..8) en el mismo orden que VALUES.
    sqlite3_bind_text(stmt, 1, unit_number->valuestring, -1, SQLITE_TRANSIENT);

    if (cJSON_IsString(vin)) sqlite3_bind_text(stmt, 2, vin->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 2);

    if (cJSON_IsNumber(year)) sqlite3_bind_int(stmt, 3, year->valueint);
    else sqlite3_bind_null(stmt, 3);

    if (cJSON_IsString(make)) sqlite3_bind_text(stmt, 4, make->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 4);

    if (cJSON_IsString(model)) sqlite3_bind_text(stmt, 5, model->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 5);

    if (cJSON_IsString(engine)) sqlite3_bind_text(stmt, 6, engine->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 6);

    if (cJSON_IsString(status)) sqlite3_bind_text(stmt, 7, status->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_text(stmt, 7, "active", -1, SQLITE_TRANSIENT);

    if (cJSON_IsNumber(current_km)) sqlite3_bind_double(stmt, 8, current_km->valuedouble);
    else sqlite3_bind_double(stmt, 8, 0.0);

    // Ejecutamos el insert.
    int rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);
    cJSON_Delete(root);

    if (rc != SQLITE_DONE) {
        send_json(conn, 500, "{\"error\":\"db_insert_failed\"}");
        return 500;
    }

    // Recuperamos el id autogenerado.
    long long new_id = sqlite3_last_insert_rowid(g_db);

    // Respondemos JSON con el id.
    char resp[128];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"id\":%lld}", new_id);
    send_json(conn, 201, resp);
    return 201;
}

// Handler principal para /api/trucks.
// Decide qué función llamar según el método HTTP (GET o POST).
static int handle_api_trucks(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);

    if (strcmp(ri->request_method, "GET") == 0) {
        return handle_trucks_list(conn);
    }
    if (strcmp(ri->request_method, "POST") == 0) {
        return handle_trucks_create(conn);
    }

    send_json(conn, 404, "{\"error\":\"not_found\"}");
    return 404;
}

// main: punto de entrada del programa.
// Aquí arrancamos DB, servidor, rutas y dejamos el proceso vivo.
int main(void) {
    // Inicializa SQLite y tablas.
    if (db_init("data/app.db") != 0) {
        return 1;
    }

    // Opciones para CivetWeb:
    // listening_ports: puerto 8080
    // num_threads: threads internos para atender requests
    // document_root: carpeta de archivos estáticos (frontend)
    const char *options[] = {
        "listening_ports", "8080",
        "num_threads", "8",
        "document_root", "../frontend/static",
        0
    };

    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));

    // Inicia el servidor CivetWeb.
    struct mg_context *ctx = mg_start(&callbacks, 0, options);
    if (!ctx) {
        fprintf(stderr, "No se pudo iniciar el servidor HTTP.\n");
        return 1;
    }

    // Registramos rutas (endpoints).
    mg_set_request_handler(ctx, "/health", handle_health, 0);
    mg_set_request_handler(ctx, "/api/trucks", handle_api_trucks, 0);

    // Mensajes informativos.
    printf("Servidor local: http://127.0.0.1:8080\n");
    printf("health:         http://127.0.0.1:8080/health\n");
    printf("frontend:       http://127.0.0.1:8080/\n");
    printf("api trucks:     http://127.0.0.1:8080/api/trucks\n");

    // Loop infinito para que el servidor no termine.
    // sleep(1) evita usar 100% CPU.
    for (;;) sleep(1);

    // (No se llega aquí en la práctica, pero es “cierre correcto”.)
    mg_stop(ctx);
    sqlite3_close(g_db);
    return 0;
}
