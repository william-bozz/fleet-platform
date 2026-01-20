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
#include "loads.h"

#include "fuel_entries.h"
#include "km_logs.h"
#include "driver_payments.h"
#include "stats.h"
#include "charts.h"

static int handle_health(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    send_json(conn, 200, "{\"ok\":true,\"service\":\"fleet-platform\",\"version\":\"0.6\"}");
    return 200;
}

int main(void) {
    if (db_init("data/app.db") != 0) return 1;

    const char *options[] = {
        "listening_ports", "8080",
        "num_threads", "8",
        "document_root", "frontend/static",
        0
    };

    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));

    struct mg_context *ctx = mg_start(&callbacks, 0, options);
    if (!ctx) { fprintf(stderr, "No se pudo iniciar el servidor HTTP.\n"); return 1; }

    mg_set_request_handler(ctx, "/health", handle_health, 0);

    mg_set_request_handler(ctx, "/api/trucks", handle_api_trucks, 0);
    mg_set_request_handler(ctx, "/api/trailers", handle_api_trailers, 0);
    mg_set_request_handler(ctx, "/api/drivers", handle_api_drivers, 0);
    mg_set_request_handler(ctx, "/api/loads", handle_api_loads, 0);

    mg_set_request_handler(ctx, "/api/fuel_entries", handle_api_fuel_entries, 0);
    mg_set_request_handler(ctx, "/api/km_logs", handle_api_km_logs, 0);
    mg_set_request_handler(ctx, "/api/driver_payments", handle_api_driver_payments, 0);

    mg_set_request_handler(ctx, "/api/stats/summary", handle_api_stats_summary, 0);

    mg_set_request_handler(ctx, "/dashboard", handle_dashboard, 0);

    mg_set_request_handler(ctx, "/charts/fuel_liters_by_truck.svg", handle_chart_fuel_liters_by_truck, 0);
    mg_set_request_handler(ctx, "/charts/fuel_cost_by_truck.svg", handle_chart_fuel_cost_by_truck, 0);
    mg_set_request_handler(ctx, "/charts/km_by_truck.svg", handle_chart_km_by_truck, 0);
    mg_set_request_handler(ctx, "/charts/pay_by_driver.svg", handle_chart_pay_by_driver, 0);

    printf("Servidor local:  http://127.0.0.1:8080\n");
    printf("Dashboard:       http://127.0.0.1:8080/dashboard\n");

    for (;;) sleep(1);

    mg_stop(ctx);
    db_close();
    return 0;
}
