#include "stats.h"
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include "cJSON.h"
#include "db.h"
#include "http.h"

static cJSON *query_group(sqlite3 *db, const char *sql,
                          const char *key_name, const char *v1_name, const char *v2_name) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, key_name, sqlite3_column_int(stmt, 0));
        if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) cJSON_AddNumberToObject(o, v1_name, sqlite3_column_double(stmt, 1));
        if (v2_name && sqlite3_column_type(stmt, 2) != SQLITE_NULL) cJSON_AddNumberToObject(o, v2_name, sqlite3_column_double(stmt, 2));
        cJSON_AddItemToArray(arr, o);
    }
    sqlite3_finalize(stmt);
    return arr;
}

int handle_api_stats_summary(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "GET") != 0) { send_json(conn, 404, "{\"error\":\"not_found\"}"); return 404; }

    sqlite3 *db = db_handle();

    cJSON *root = cJSON_CreateObject();

    cJSON *fuel_by_truck = query_group(
        db,
        "SELECT truck_id, SUM(liters) AS liters, SUM(total_cost) AS cost "
        "FROM fuel_entries GROUP BY truck_id ORDER BY truck_id;",
        "truck_id", "liters_total", "cost_total"
    );
    if (!fuel_by_truck) fuel_by_truck = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "fuel_by_truck", fuel_by_truck);

    cJSON *km_by_truck = query_group(
        db,
        "SELECT truck_id, SUM(km) AS km_total, NULL AS dummy "
        "FROM km_logs GROUP BY truck_id ORDER BY truck_id;",
        "truck_id", "km_total", NULL
    );
    if (!km_by_truck) km_by_truck = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "km_by_truck", km_by_truck);

    cJSON *pay_by_driver = query_group(
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
