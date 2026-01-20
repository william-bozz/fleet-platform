#include "trailers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include "cJSON.h"
#include "db.h"
#include "http.h"

static int is_valid_type(const char *t) {
    return (strcmp(t, "reefer") == 0) || (strcmp(t, "dry_van") == 0) || (strcmp(t, "flatbed") == 0);
}

static int trailers_list(struct mg_connection *conn) {
    sqlite3 *db = db_handle();
    const char *sql = "SELECT id, unit_number, vin, type, status, current_km FROM trailers ORDER BY id DESC;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}"); return 500; }

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(o, "unit_number", (const char *)sqlite3_column_text(stmt, 1));
        const unsigned char *vin = sqlite3_column_text(stmt, 2); if (vin) cJSON_AddStringToObject(o, "vin", (const char *)vin);
        cJSON_AddStringToObject(o, "type", (const char *)sqlite3_column_text(stmt, 3));
        const unsigned char *status = sqlite3_column_text(stmt, 4); if (status) cJSON_AddStringToObject(o, "status", (const char *)status);
        cJSON_AddNumberToObject(o, "current_km", sqlite3_column_double(stmt, 5));
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

static int trailers_create(struct mg_connection *conn) {
    sqlite3 *db = db_handle();
    long long body_len = 0;
    char *body = read_request_body(conn, &body_len);
    if (!body || body_len == 0) { free(body); send_json(conn, 400, "{\"error\":\"empty_body\"}"); return 400; }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root || !cJSON_IsObject(root)) { cJSON_Delete(root); send_json(conn, 400, "{\"error\":\"invalid_json\"}"); return 400; }

    const cJSON *unit_number = cJSON_GetObjectItemCaseSensitive(root, "unit_number");
    if (!cJSON_IsString(unit_number) || unit_number->valuestring[0] == '\0') { cJSON_Delete(root); send_json(conn, 400, "{\"error\":\"unit_number_required\"}"); return 400; }

    const cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type) || !is_valid_type(type->valuestring)) { cJSON_Delete(root); send_json(conn, 400, "{\"error\":\"type_required_reefer_dry_van_flatbed\"}"); return 400; }

    const cJSON *vin = cJSON_GetObjectItemCaseSensitive(root, "vin");
    const cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "status");
    const cJSON *current_km = cJSON_GetObjectItemCaseSensitive(root, "current_km");

    const char *sql = "INSERT INTO trailers(unit_number, vin, type, status, current_km) VALUES(?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { cJSON_Delete(root); send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}"); return 500; }

    sqlite3_bind_text(stmt, 1, unit_number->valuestring, -1, SQLITE_TRANSIENT);
    if (cJSON_IsString(vin)) sqlite3_bind_text(stmt, 2, vin->valuestring, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 2);
    sqlite3_bind_text(stmt, 3, type->valuestring, -1, SQLITE_TRANSIENT);
    if (cJSON_IsString(status)) sqlite3_bind_text(stmt, 4, status->valuestring, -1, SQLITE_TRANSIENT); else sqlite3_bind_text(stmt, 4, "active", -1, SQLITE_TRANSIENT);
    if (cJSON_IsNumber(current_km)) sqlite3_bind_double(stmt, 5, current_km->valuedouble); else sqlite3_bind_double(stmt, 5, 0.0);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    cJSON_Delete(root);
    if (rc != SQLITE_DONE) { send_json(conn, 500, "{\"error\":\"db_insert_failed\"}"); return 500; }

    long long new_id = sqlite3_last_insert_rowid(db);
    char resp[128];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"id\":%lld}", new_id);
    send_json(conn, 201, resp);
    return 201;
}

int handle_api_trailers(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "GET") == 0) return trailers_list(conn);
    if (strcmp(ri->request_method, "POST") == 0) return trailers_create(conn);
    send_json(conn, 404, "{\"error\":\"not_found\"}");
    return 404;
}
