#include "stats.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sqlite3.h>
#include "cJSON.h"

#include "db.h"
#include "http.h"

static int get_query_int(const char *query_string, const char *key, int def_value) {
    if (!query_string || !key) return def_value;

    // query_string ejemplo: "days=30&x=1"
    // buscamos "key="
    size_t klen = strlen(key);
    const char *p = query_string;

    while (p && *p) {
        const char *eq = strchr(p, '=');
        if (!eq) break;

        size_t name_len = (size_t)(eq - p);
        if (name_len == klen && strncmp(p, key, klen) == 0) {
            const char *val = eq + 1;
            // termina en & o fin
            char tmp[32];
            size_t i = 0;
            while (val[i] && val[i] != '&' && i < sizeof(tmp) - 1) {
                tmp[i] = val[i];
                i++;
            }
            tmp[i] = '\0';
            int n = atoi(tmp);
            return (n > 0) ? n : def_value;
        }

        const char *amp = strchr(eq + 1, '&');
        if (!amp) break;
        p = amp + 1;
    }

    return def_value;
}

static cJSON *query_group_1key_2vals(sqlite3 *db, const char *sql,
                                    const char *kname, const char *v1name, const char *v2name) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, kname, sqlite3_column_int(stmt, 0));
        if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) cJSON_AddNumberToObject(o, v1name, sqlite3_column_double(stmt, 1));
        if (v2name && sqlite3_column_type(stmt, 2) != SQLITE_NULL) cJSON_AddNumberToObject(o, v2name, sqlite3_column_double(stmt, 2));
        cJSON_AddItemToArray(arr, o);
    }
    sqlite3_finalize(stmt);
    return arr;
}

static cJSON *query_daily(sqlite3 *db, const char *sql, int days, const char *dname, const char *v1name, const char *v2name) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;

    // bind 1 = days
    sqlite3_bind_int(stmt, 1, days);

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *o = cJSON_CreateObject();
        const unsigned char *day = sqlite3_column_text(stmt, 0);
        if (day) cJSON_AddStringToObject(o, dname, (const char *)day);

        if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) cJSON_AddNumberToObject(o, v1name, sqlite3_column_double(stmt, 1));
        if (v2name && sqlite3_column_type(stmt, 2) != SQLITE_NULL) cJSON_AddNumberToObject(o, v2name, sqlite3_column_double(stmt, 2));

        cJSON_AddItemToArray(arr, o);
    }

    sqlite3_finalize(stmt);
    return arr;
}

static int query_single_double(sqlite3 *db, const char *sql, double *out_value) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        if (sqlite3_column_type(stmt, 0) == SQLITE_NULL) *out_value = 0.0;
        else *out_value = sqlite3_column_double(stmt, 0);
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return -1;
}

static int ensure_get(struct mg_connection *conn) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "GET") != 0) {
        send_json(conn, 404, "{\"error\":\"not_found\"}");
        return 0;
    }
    return 1;
}

int handle_api_stats_summary(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    if (!ensure_get(conn)) return 404;

    sqlite3 *db = db_handle();
    cJSON *root = cJSON_CreateObject();

    cJSON *fuel_by_truck = query_group_1key_2vals(
        db,
        "SELECT truck_id, SUM(liters) AS liters, SUM(total_cost) AS cost "
        "FROM fuel_entries GROUP BY truck_id ORDER BY truck_id;",
        "truck_id", "liters_total", "cost_total"
    );
    if (!fuel_by_truck) fuel_by_truck = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "fuel_by_truck", fuel_by_truck);

    cJSON *km_by_truck = query_group_1key_2vals(
        db,
        "SELECT truck_id, SUM(km) AS km_total, NULL AS dummy "
        "FROM km_logs GROUP BY truck_id ORDER BY truck_id;",
        "truck_id", "km_total", NULL
    );
    if (!km_by_truck) km_by_truck = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "km_by_truck", km_by_truck);

    cJSON *pay_by_driver = query_group_1key_2vals(
        db,
        "SELECT driver_id, SUM(amount) AS pay_total, NULL AS dummy "
        "FROM driver_payments GROUP BY driver_id ORDER BY driver_id;",
        "driver_id", "pay_total", NULL
    );
    if (!pay_by_driver) pay_by_driver = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "pay_by_driver", pay_by_driver);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) { send_json(conn, 500, "{\"error\":\"json_encode_failed\"}"); return 500; }

    send_json(conn, 200, out);
    free(out);
    return 200;
}

int handle_api_stats_fuel_daily(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    if (!ensure_get(conn)) return 404;

    const struct mg_request_info *ri = mg_get_request_info(conn);
    int days = get_query_int(ri->query_string, "days", 30);
    if (days > 3650) days = 3650;

    sqlite3 *db = db_handle();

    // Nota: filtramos los últimos N días usando datetime('now', '-'||?||' day')
    // y agrupamos por fecha (YYYY-MM-DD)
    const char *sql =
        "SELECT substr(fueled_at, 1, 10) AS day, "
        "       SUM(liters) AS liters_total, "
        "       SUM(COALESCE(total_cost, 0)) AS cost_total "
        "FROM fuel_entries "
        "WHERE fueled_at >= datetime('now', '-' || ? || ' day') "
        "GROUP BY day "
        "ORDER BY day;";

    cJSON *arr = query_daily(db, sql, days, "day", "liters_total", "cost_total");
    if (!arr) {
        send_json(conn, 500, "{\"error\":\"db_query_failed\"}");
        return 500;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "days", days);
    cJSON_AddItemToObject(root, "rows", arr);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) { send_json(conn, 500, "{\"error\":\"json_encode_failed\"}"); return 500; }

    send_json(conn, 200, out);
    free(out);
    return 200;
}

int handle_api_stats_km_daily(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    if (!ensure_get(conn)) return 404;

    const struct mg_request_info *ri = mg_get_request_info(conn);
    int days = get_query_int(ri->query_string, "days", 30);
    if (days > 3650) days = 3650;

    sqlite3 *db = db_handle();

    const char *sql =
        "SELECT substr(logged_at, 1, 10) AS day, "
        "       SUM(km) AS km_total, "
        "       NULL AS dummy "
        "FROM km_logs "
        "WHERE logged_at >= datetime('now', '-' || ? || ' day') "
        "GROUP BY day "
        "ORDER BY day;";

    cJSON *arr = query_daily(db, sql, days, "day", "km_total", NULL);
    if (!arr) {
        send_json(conn, 500, "{\"error\":\"db_query_failed\"}");
        return 500;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "days", days);
    cJSON_AddItemToObject(root, "rows", arr);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) { send_json(conn, 500, "{\"error\":\"json_encode_failed\"}"); return 500; }

    send_json(conn, 200, out);
    free(out);
    return 200;
}

int handle_api_stats_pay_daily(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    if (!ensure_get(conn)) return 404;

    const struct mg_request_info *ri = mg_get_request_info(conn);
    int days = get_query_int(ri->query_string, "days", 30);
    if (days > 3650) days = 3650;

    sqlite3 *db = db_handle();

    const char *sql =
        "SELECT substr(paid_at, 1, 10) AS day, "
        "       SUM(amount) AS pay_total, "
        "       NULL AS dummy "
        "FROM driver_payments "
        "WHERE paid_at >= datetime('now', '-' || ? || ' day') "
        "GROUP BY day "
        "ORDER BY day;";

    cJSON *arr = query_daily(db, sql, days, "day", "pay_total", NULL);
    if (!arr) {
        send_json(conn, 500, "{\"error\":\"db_query_failed\"}");
        return 500;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "days", days);
    cJSON_AddItemToObject(root, "rows", arr);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) { send_json(conn, 500, "{\"error\":\"json_encode_failed\"}"); return 500; }

    send_json(conn, 200, out);
    free(out);
    return 200;
}

int handle_api_stats_profit_summary(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    if (!ensure_get(conn)) return 404;

    sqlite3 *db = db_handle();

    // Importante:
    // - Aquí hacemos un "profit" estimado en USD solamente.
    // - Si mezclas monedas, esto no es exacto (luego lo mejoramos con conversión y tasas).
    double revenue_usd = 0.0;
    double fuel_cost_usd = 0.0;
    double pay_usd = 0.0;

    // Loads: si currency es NULL, lo tratamos como USD.
    if (query_single_double(db,
        "SELECT SUM(COALESCE(rate, 0)) FROM loads WHERE COALESCE(currency, 'USD') = 'USD';",
        &revenue_usd) != 0) {
        send_json(conn, 500, "{\"error\":\"db_query_failed\"}");
        return 500;
    }

    // Fuel: si total_cost es NULL se cuenta como 0. Si currency NULL, USD.
    if (query_single_double(db,
        "SELECT SUM(COALESCE(total_cost, 0)) FROM fuel_entries WHERE COALESCE(currency, 'USD') = 'USD';",
        &fuel_cost_usd) != 0) {
        send_json(conn, 500, "{\"error\":\"db_query_failed\"}");
        return 500;
    }

    // Driver payments: currency NULL, USD.
    if (query_single_double(db,
        "SELECT SUM(COALESCE(amount, 0)) FROM driver_payments WHERE COALESCE(currency, 'USD') = 'USD';",
        &pay_usd) != 0) {
        send_json(conn, 500, "{\"error\":\"db_query_failed\"}");
        return 500;
    }

    double profit_usd = revenue_usd - fuel_cost_usd - pay_usd;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "currency", "USD");
    cJSON_AddNumberToObject(root, "revenue_total", revenue_usd);
    cJSON_AddNumberToObject(root, "fuel_cost_total", fuel_cost_usd);
    cJSON_AddNumberToObject(root, "driver_pay_total", pay_usd);
    cJSON_AddNumberToObject(root, "profit_estimated", profit_usd);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!out) { send_json(conn, 500, "{\"error\":\"json_encode_failed\"}"); return 500; }

    send_json(conn, 200, out);
    free(out);
    return 200;
}
