#include "loads.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include "cJSON.h"
#include "db.h"
#include "http.h"

static int is_valid_status(const char *s) {
    return (strcmp(s, "planned") == 0) || (strcmp(s, "in_transit") == 0) || (strcmp(s, "delivered") == 0) || (strcmp(s, "cancelled") == 0);
}

static int loads_list(struct mg_connection *conn) {
    sqlite3 *db = db_handle();
    const char *sql =
        "SELECT id, reference, shipper, pickup_location, delivery_location, "
        "pickup_date, delivery_date, commodity, weight_kg, rate, currency, distance_km, status, "
        "truck_id, trailer_id, driver_id "
        "FROM loads ORDER BY id DESC;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}"); return 500; }

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(o, "reference", (const char *)sqlite3_column_text(stmt, 1));
        const unsigned char *shipper = sqlite3_column_text(stmt, 2); if (shipper) cJSON_AddStringToObject(o, "shipper", (const char *)shipper);
        const unsigned char *pick = sqlite3_column_text(stmt, 3); if (pick) cJSON_AddStringToObject(o, "pickup_location", (const char *)pick);
        const unsigned char *del = sqlite3_column_text(stmt, 4); if (del) cJSON_AddStringToObject(o, "delivery_location", (const char *)del);
        const unsigned char *pd = sqlite3_column_text(stmt, 5); if (pd) cJSON_AddStringToObject(o, "pickup_date", (const char *)pd);
        const unsigned char *dd = sqlite3_column_text(stmt, 6); if (dd) cJSON_AddStringToObject(o, "delivery_date", (const char *)dd);
        const unsigned char *comm = sqlite3_column_text(stmt, 7); if (comm) cJSON_AddStringToObject(o, "commodity", (const char *)comm);
        if (sqlite3_column_type(stmt, 8) != SQLITE_NULL) cJSON_AddNumberToObject(o, "weight_kg", sqlite3_column_double(stmt, 8));
        if (sqlite3_column_type(stmt, 9) != SQLITE_NULL) cJSON_AddNumberToObject(o, "rate", sqlite3_column_double(stmt, 9));
        const unsigned char *cur = sqlite3_column_text(stmt, 10); if (cur) cJSON_AddStringToObject(o, "currency", (const char *)cur);
        if (sqlite3_column_type(stmt, 11) != SQLITE_NULL) cJSON_AddNumberToObject(o, "distance_km", sqlite3_column_double(stmt, 11));
        const unsigned char *st = sqlite3_column_text(stmt, 12); if (st) cJSON_AddStringToObject(o, "status", (const char *)st);
        cJSON_AddNumberToObject(o, "truck_id", sqlite3_column_int(stmt, 13));
        cJSON_AddNumberToObject(o, "trailer_id", sqlite3_column_int(stmt, 14));
        cJSON_AddNumberToObject(o, "driver_id", sqlite3_column_int(stmt, 15));
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

static int loads_create(struct mg_connection *conn) {
    sqlite3 *db = db_handle();
    long long body_len = 0;
    char *body = read_request_body(conn, &body_len);
    if (!body || body_len == 0) { free(body); send_json(conn, 400, "{\"error\":\"empty_body\"}"); return 400; }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root || !cJSON_IsObject(root)) { cJSON_Delete(root); send_json(conn, 400, "{\"error\":\"invalid_json\"}"); return 400; }

    const cJSON *reference = cJSON_GetObjectItemCaseSensitive(root, "reference");
    if (!cJSON_IsString(reference) || reference->valuestring[0] == '\0') { cJSON_Delete(root); send_json(conn, 400, "{\"error\":\"reference_required\"}"); return 400; }

    const cJSON *truck_id = cJSON_GetObjectItemCaseSensitive(root, "truck_id");
    const cJSON *trailer_id = cJSON_GetObjectItemCaseSensitive(root, "trailer_id");
    const cJSON *driver_id = cJSON_GetObjectItemCaseSensitive(root, "driver_id");
    if (!cJSON_IsNumber(truck_id) || !cJSON_IsNumber(trailer_id) || !cJSON_IsNumber(driver_id)) { cJSON_Delete(root); send_json(conn, 400, "{\"error\":\"truck_id_trailer_id_driver_id_required\"}"); return 400; }

    const cJSON *shipper = cJSON_GetObjectItemCaseSensitive(root, "shipper");
    const cJSON *pickup_location = cJSON_GetObjectItemCaseSensitive(root, "pickup_location");
    const cJSON *delivery_location = cJSON_GetObjectItemCaseSensitive(root, "delivery_location");
    const cJSON *pickup_date = cJSON_GetObjectItemCaseSensitive(root, "pickup_date");
    const cJSON *delivery_date = cJSON_GetObjectItemCaseSensitive(root, "delivery_date");
    const cJSON *commodity = cJSON_GetObjectItemCaseSensitive(root, "commodity");
    const cJSON *weight_kg = cJSON_GetObjectItemCaseSensitive(root, "weight_kg");
    const cJSON *rate = cJSON_GetObjectItemCaseSensitive(root, "rate");
    const cJSON *currency = cJSON_GetObjectItemCaseSensitive(root, "currency");
    const cJSON *distance_km = cJSON_GetObjectItemCaseSensitive(root, "distance_km");
    const cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "status");

    if (cJSON_IsString(status) && !is_valid_status(status->valuestring)) { cJSON_Delete(root); send_json(conn, 400, "{\"error\":\"status_invalid_planned_in_transit_delivered_cancelled\"}"); return 400; }

    const char *sql =
        "INSERT INTO loads(reference, shipper, pickup_location, delivery_location, pickup_date, delivery_date, "
        "commodity, weight_kg, rate, currency, distance_km, status, truck_id, trailer_id, driver_id) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { cJSON_Delete(root); send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}"); return 500; }

    sqlite3_bind_text(stmt, 1, reference->valuestring, -1, SQLITE_TRANSIENT);
    if (cJSON_IsString(shipper)) sqlite3_bind_text(stmt, 2, shipper->valuestring, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 2);
    if (cJSON_IsString(pickup_location)) sqlite3_bind_text(stmt, 3, pickup_location->valuestring, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 3);
    if (cJSON_IsString(delivery_location)) sqlite3_bind_text(stmt, 4, delivery_location->valuestring, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 4);
    if (cJSON_IsString(pickup_date)) sqlite3_bind_text(stmt, 5, pickup_date->valuestring, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 5);
    if (cJSON_IsString(delivery_date)) sqlite3_bind_text(stmt, 6, delivery_date->valuestring, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 6);
    if (cJSON_IsString(commodity)) sqlite3_bind_text(stmt, 7, commodity->valuestring, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 7);
    if (cJSON_IsNumber(weight_kg)) sqlite3_bind_double(stmt, 8, weight_kg->valuedouble); else sqlite3_bind_null(stmt, 8);
    if (cJSON_IsNumber(rate)) sqlite3_bind_double(stmt, 9, rate->valuedouble); else sqlite3_bind_null(stmt, 9);
    if (cJSON_IsString(currency)) sqlite3_bind_text(stmt, 10, currency->valuestring, -1, SQLITE_TRANSIENT); else sqlite3_bind_text(stmt, 10, "USD", -1, SQLITE_TRANSIENT);
    if (cJSON_IsNumber(distance_km)) sqlite3_bind_double(stmt, 11, distance_km->valuedouble); else sqlite3_bind_null(stmt, 11);
    if (cJSON_IsString(status)) sqlite3_bind_text(stmt, 12, status->valuestring, -1, SQLITE_TRANSIENT); else sqlite3_bind_text(stmt, 12, "planned", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 13, truck_id->valueint);
    sqlite3_bind_int(stmt, 14, trailer_id->valueint);
    sqlite3_bind_int(stmt, 15, driver_id->valueint);

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

int handle_api_loads(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "GET") == 0) return loads_list(conn);
    if (strcmp(ri->request_method, "POST") == 0) return loads_create(conn);
    send_json(conn, 404, "{\"error\":\"not_found\"}");
    return 404;
}
