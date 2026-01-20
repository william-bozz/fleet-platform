#include "km_logs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include "cJSON.h"
#include "db.h"
#include "http.h"

static int km_list(struct mg_connection *conn) {
    sqlite3 *db = db_handle();
    const char *sql =
        "SELECT id, truck_id, load_id, km, odometer_start, odometer_end, logged_at "
        "FROM km_logs ORDER BY id DESC;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}"); return 500; }

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddNumberToObject(o, "truck_id", sqlite3_column_int(stmt, 1));
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) cJSON_AddNumberToObject(o, "load_id", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(o, "km", sqlite3_column_double(stmt, 3));
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) cJSON_AddNumberToObject(o, "odometer_start", sqlite3_column_double(stmt, 4));
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) cJSON_AddNumberToObject(o, "odometer_end", sqlite3_column_double(stmt, 5));
        const unsigned char *dt = sqlite3_column_text(stmt, 6); if (dt) cJSON_AddStringToObject(o, "logged_at", (const char*)dt);
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

static int km_create(struct mg_connection *conn) {
    sqlite3 *db = db_handle();
    long long bl = 0;
    char *body = read_request_body(conn, &bl);
    if (!body || bl == 0) { free(body); send_json(conn, 400, "{\"error\":\"empty_body\"}"); return 400; }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root || !cJSON_IsObject(root)) { cJSON_Delete(root); send_json(conn, 400, "{\"error\":\"invalid_json\"}"); return 400; }

    const cJSON *truck_id = cJSON_GetObjectItemCaseSensitive(root, "truck_id");
    const cJSON *km = cJSON_GetObjectItemCaseSensitive(root, "km");
    if (!cJSON_IsNumber(truck_id) || !cJSON_IsNumber(km) || km->valuedouble <= 0) {
        cJSON_Delete(root); send_json(conn, 400, "{\"error\":\"truck_id_and_positive_km_required\"}"); return 400;
    }

    const cJSON *load_id = cJSON_GetObjectItemCaseSensitive(root, "load_id");
    const cJSON *odometer_start = cJSON_GetObjectItemCaseSensitive(root, "odometer_start");
    const cJSON *odometer_end = cJSON_GetObjectItemCaseSensitive(root, "odometer_end");
    const cJSON *logged_at = cJSON_GetObjectItemCaseSensitive(root, "logged_at");

    const char *sql =
        "INSERT INTO km_logs(truck_id, load_id, km, odometer_start, odometer_end, logged_at) "
        "VALUES(?, ?, ?, ?, ?, COALESCE(?, datetime('now')));";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { cJSON_Delete(root); send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}"); return 500; }

    sqlite3_bind_int(stmt, 1, truck_id->valueint);
    if (cJSON_IsNumber(load_id)) sqlite3_bind_int(stmt, 2, load_id->valueint); else sqlite3_bind_null(stmt, 2);
    sqlite3_bind_double(stmt, 3, km->valuedouble);
    if (cJSON_IsNumber(odometer_start)) sqlite3_bind_double(stmt, 4, odometer_start->valuedouble); else sqlite3_bind_null(stmt, 4);
    if (cJSON_IsNumber(odometer_end)) sqlite3_bind_double(stmt, 5, odometer_end->valuedouble); else sqlite3_bind_null(stmt, 5);
    if (cJSON_IsString(logged_at) && logged_at->valuestring[0]) sqlite3_bind_text(stmt, 6, logged_at->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 6);

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

int handle_api_km_logs(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "GET") == 0) return km_list(conn);
    if (strcmp(ri->request_method, "POST") == 0) return km_create(conn);
    send_json(conn, 404, "{\"error\":\"not_found\"}");
    return 404;
}
