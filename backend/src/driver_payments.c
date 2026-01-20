#include "driver_payments.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sqlite3.h>
#include "cJSON.h"
#include "db.h"
#include "http.h"

static int pay_list(struct mg_connection *conn) {
    sqlite3 *db = db_handle();
    const char *sql =
        "SELECT id, driver_id, load_id, amount, currency, method, notes, paid_at "
        "FROM driver_payments ORDER BY id DESC;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}"); return 500; }

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddNumberToObject(o, "driver_id", sqlite3_column_int(stmt, 1));
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) cJSON_AddNumberToObject(o, "load_id", sqlite3_column_int(stmt, 2));
        cJSON_AddNumberToObject(o, "amount", sqlite3_column_double(stmt, 3));
        const unsigned char *cur = sqlite3_column_text(stmt, 4); if (cur) cJSON_AddStringToObject(o, "currency", (const char*)cur);
        const unsigned char *method = sqlite3_column_text(stmt, 5); if (method) cJSON_AddStringToObject(o, "method", (const char*)method);
        const unsigned char *notes = sqlite3_column_text(stmt, 6); if (notes) cJSON_AddStringToObject(o, "notes", (const char*)notes);
        const unsigned char *dt = sqlite3_column_text(stmt, 7); if (dt) cJSON_AddStringToObject(o, "paid_at", (const char*)dt);
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

static int pay_create(struct mg_connection *conn) {
    sqlite3 *db = db_handle();
    long long bl = 0;
    char *body = read_request_body(conn, &bl);
    if (!body || bl == 0) { free(body); send_json(conn, 400, "{\"error\":\"empty_body\"}"); return 400; }

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root || !cJSON_IsObject(root)) { cJSON_Delete(root); send_json(conn, 400, "{\"error\":\"invalid_json\"}"); return 400; }

    const cJSON *driver_id = cJSON_GetObjectItemCaseSensitive(root, "driver_id");
    const cJSON *amount = cJSON_GetObjectItemCaseSensitive(root, "amount");
    if (!cJSON_IsNumber(driver_id) || !cJSON_IsNumber(amount) || amount->valuedouble <= 0) {
        cJSON_Delete(root); send_json(conn, 400, "{\"error\":\"driver_id_and_positive_amount_required\"}"); return 400;
    }

    const cJSON *load_id = cJSON_GetObjectItemCaseSensitive(root, "load_id");
    const cJSON *currency = cJSON_GetObjectItemCaseSensitive(root, "currency");
    const cJSON *method = cJSON_GetObjectItemCaseSensitive(root, "method");
    const cJSON *notes = cJSON_GetObjectItemCaseSensitive(root, "notes");
    const cJSON *paid_at = cJSON_GetObjectItemCaseSensitive(root, "paid_at");

    const char *sql =
        "INSERT INTO driver_payments(driver_id, load_id, amount, currency, method, notes, paid_at) "
        "VALUES(?, ?, ?, ?, ?, ?, COALESCE(?, datetime('now')));";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { cJSON_Delete(root); send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}"); return 500; }

    sqlite3_bind_int(stmt, 1, driver_id->valueint);
    if (cJSON_IsNumber(load_id)) sqlite3_bind_int(stmt, 2, load_id->valueint); else sqlite3_bind_null(stmt, 2);
    sqlite3_bind_double(stmt, 3, amount->valuedouble);
    if (cJSON_IsString(currency) && currency->valuestring[0]) sqlite3_bind_text(stmt, 4, currency->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_text(stmt, 4, "USD", -1, SQLITE_TRANSIENT);
    if (cJSON_IsString(method)) sqlite3_bind_text(stmt, 5, method->valuestring, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 5);
    if (cJSON_IsString(notes)) sqlite3_bind_text(stmt, 6, notes->valuestring, -1, SQLITE_TRANSIENT); else sqlite3_bind_null(stmt, 6);
    if (cJSON_IsString(paid_at) && paid_at->valuestring[0]) sqlite3_bind_text(stmt, 7, paid_at->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 7);

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

int handle_api_driver_payments(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "GET") == 0) return pay_list(conn);
    if (strcmp(ri->request_method, "POST") == 0) return pay_create(conn);
    send_json(conn, 404, "{\"error\":\"not_found\"}");
    return 404;
}
