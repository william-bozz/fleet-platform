#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "civetweb.h"

#include "db.h"
#include "http.h"
#include "trucks.h"
#include "trailers.h"
#include "drivers.h"

static int handle_health(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    send_json(conn, 200, "{\"ok\":true,\"service\":\"fleet-platform\",\"version\":\"0.2\"}");
    return 200;
}

int main(void) {
    if (db_init("data/app.db") != 0) {
        return 1;
    }

    const char *options[] = {
        "listening_ports", "8080",
        "num_threads", "8",
        "document_root", "frontend/static",
        0
    };

    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));

    struct mg_context *ctx = mg_start(&callbacks, 0, options);
    if (!ctx) {
        fprintf(stderr, "No se pudo iniciar el servidor HTTP.\n");
        return 1;
    }

    mg_set_request_handler(ctx, "/health", handle_health, 0);
    mg_set_request_handler(ctx, "/api/trucks", handle_api_trucks, 0);
    mg_set_request_handler(ctx, "/api/trailers", handle_api_trailers, 0);
    mg_set_request_handler(ctx, "/api/drivers", handle_api_drivers, 0);

    printf("Servidor local: http://127.0.0.1:8080\n");
    printf("health:         http://127.0.0.1:8080/health\n");
    printf("frontend:       http://127.0.0.1:8080/\n");
    printf("api trucks:     http://127.0.0.1:8080/api/trucks\n");
    printf("api trailers:   http://127.0.0.1:8080/api/trailers\n");

    for (;;) sleep(1);

    mg_stop(ctx);
    db_close();
    return 0;
}
