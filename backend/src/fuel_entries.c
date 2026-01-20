#include "fuel_entries.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include "cJSON.h"
#include "db.h"
#include "http.h"

static int fuel_list(struct mg_connection *conn) {
    sqlite3 *db = db_handle();
    const char *sql =
        "SELECT id, truck_id, load_id, driver_id, liters, total_cost, currency, odometer_km, location, fueled_at "
        "FROM fuel_entries ORDER BY id DESC;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}"); return 500; }

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddNumberToObject(o, "truck_id", sqlite3_column_int(stmt, 1));
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) cJSON_AddNumberToObject(o, "load_id", sqlite3_column_int(stmt, 2));
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) cJSON_AddNumberToObject(o, "driver_id", sqlite3_column_int(stmt, 3));
        cJSON_AddNumberToObject(o, "liters", sqlite3_column_double(stmt, 4));
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) cJSON_AddNumberToObject(o, "total_cost", sqlite3_column_double(stmt, 5));
        const unsigned char *cur = sqlite3_column_text(stmt, 6); if (cur) cJSON_AddStringToObject(o, "currency", (const char*)cur);
        if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) cJSON_AddNumberToObject(o, "odometer_km", sqlite3_column_double(stmt, 7));
        const unsigned char *loc = sqlite3_column_text(stmt, 8); if (loc) cJSON_AddStringToObject(o, "location", (const char*)loc);
        const unsigned char *dt = sqlite3_column_text(stmt, 9); if (dt) cJSON_AddStringToObject(o, "fueled_at", (const char*)dt);
        cJSON_AddItemToArray(arr, o);
    }
    sqlite3_finalize(stmt);

    char *out = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (!out) { send_json(conn, 500, "{\"error\":\"json_encode_failed\"}"); return 500; }
    send_json(conn, 200, out);
    free(out);
    return 200;
}

static int fuel_create(struct mg_connection *conn) {
    sqlite3 *db = db_handle();
    long long bl = 0;
    char *body = read_request_body(conn, &bl);
    if (!body || bl == 0) { free(body); send_json(conn, 400, "{\"error\":\"empty_body\"}"); return 400; }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root || !cJSON_IsObject(root)) { cJSON_Delete(root); send_json(conn, 400, "{\"error\":\"invalid_json\"}"); return 400; }

    const cJSON *truck_id = cJSON_GetObjectItemCaseSensitive(root, "truck_id");
    const cJSON *liters = cJSON_GetObjectItemCaseSensitive(root, "liters");
    if (!cJSON_IsNumber(truck_id) || !cJSON_IsNumber(liters) || liters->valuedouble <= 0) {
        cJSON_Delete(root); send_json(conn, 400, "{\"error\":\"truck_id_and_positive_liters_required\"}"); return 400;
    }

    const cJSON *load_id = cJSON_GetObjectItemCaseSensitive(root, "load_id");
    const cJSON *driver_id = cJSON_GetObjectItemCaseSensitive(root, "driver_id");
    const cJSON *total_cost = cJSON_GetObjectItemCaseSensitive(root, "total_cost");
    const cJSON *currency = cJSON_GetObjectItemCaseSensitive(root, "currency");
    const cJSON *odometer_km = cJSON_GetObjectItemCaseSensitive(root, "odometer_km");
    const cJSON *location = cJSON_GetObjectItemCaseSensitive(root, "location");
    const cJSON *fueled_at = cJSON_GetObjectItemCaseSensitive(root, "fueled_at");

    const char *sql =
        "INSERT INTO fuel_entries(truck_id, load_id, driver_id, liters, total_cost, currency, odometer_km, location, fueled_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, COALESCE(?, datetime('now')));";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { cJSON_Delete(root); send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}"); return 500; }

    sqlite3_bind_int(stmt, 1, truck_id->valueint);
    if (cJSON_IsNumber(load_id)) sqlite3_bind_int(stmt, 2, load_id->valueint); else sqlite3_bind_null(stmt, 2);
    if (cJSON_IsNumber(driver_id)) sqlite3_bind_int(stmt, 3, driver_id->valueint); else sqlite3_bind_null(stmt, 3);
    sqlite3_bind_double(stmt, 4, liters->valuedouble);
    if (cJSON_IsNumber(total_cost)) sqlite3_bind_double(stmt, 5, total_cost->valuedouble); else sqlite3_bind_null(stmt, 5);
    if (cJSON_IsString(currency) && currency->valuestring[0]) sqlite3_bind_text(stmt, 6, currency->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_text(stmt, 6, "USD", -1, SQLITE_TRANSIENT);
    if (cJSON_IsNumber(odometer_km)) sqlite3_bind_double(stmt, 7, odometer_km->valuedouble); else sqlite3_bind_null(stmt, 7);
    if (cJSON_IsString(location)) sqlite3_bind_text(stmt, 8, location->valuestring, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 8);
    if (cJSON_IsString(fueled_at) && fueled_at->valuestring[0]) sqlite3_bind_text(stmt, 9, fueled_at->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 9);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    cJSON_Delete(root);
    if (rc != SQLITE_DONE) { send_json(conn, 500, "{\"error\":\"db_insert_failed_check_foreign_keys\"}"); return 500; }

    long long new_id = sqlite3_last_insert_rowid(db);
    char resp[128];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"id\":%lld}", new_id);
    send_json(conn, 201, resp);
    return 201;
}

int handle_api_fuel_entries(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "GET") == 0) return fuel_list(conn);
    if (strcmp(ri->request_method, "POST") == 0) return fuel_create(conn);
    send_json(conn, 404, "{\"error\":\"not_found\"}");
    return 404;
}
