#include "drivers.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sqlite3.h>
#include "cJSON.h"

#include "db.h"
#include "http.h"

static int is_valid_pay_type(const char *t) {
    return (strcmp(t, "per_km") == 0) ||
           (strcmp(t, "percent") == 0) ||
           (strcmp(t, "salary") == 0);
}

static int drivers_list(struct mg_connection *conn) {
    sqlite3 *db = db_handle();

    const char *sql =
        "SELECT id, name, license, phone, pay_type, pay_rate, status "
        "FROM drivers ORDER BY id DESC;";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}");
        return 500;
    }

    cJSON *arr = cJSON_CreateArray();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cJSON *o = cJSON_CreateObject();

        cJSON_AddNumberToObject(o, "id", sqlite3_column_int(stmt, 0));
        cJSON_AddStringToObject(o, "name", (const char *)sqlite3_column_text(stmt, 1));

        const unsigned char *license = sqlite3_column_text(stmt, 2);
        if (license) cJSON_AddStringToObject(o, "license", (const char *)license);

        const unsigned char *phone = sqlite3_column_text(stmt, 3);
        if (phone) cJSON_AddStringToObject(o, "phone", (const char *)phone);

        const unsigned char *pay_type = sqlite3_column_text(stmt, 4);
        if (pay_type) cJSON_AddStringToObject(o, "pay_type", (const char *)pay_type);

        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            cJSON_AddNumberToObject(o, "pay_rate", sqlite3_column_double(stmt, 5));
        }

        const unsigned char *status = sqlite3_column_text(stmt, 6);
        if (status) cJSON_AddStringToObject(o, "status", (const char *)status);

        cJSON_AddItemToArray(arr, o);
    }

    sqlite3_finalize(stmt);

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

static int drivers_create(struct mg_connection *conn) {
    sqlite3 *db = db_handle();

    long long body_len = 0;
    char *body = read_request_body(conn, &body_len);
    if (!body || body_len == 0) {
        free(body);
        send_json(conn, 400, "{\"error\":\"empty_body\"}");
        return 400;
    }

    cJSON *root = cJSON_Parse(body);
    free(body);

    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        send_json(conn, 400, "{\"error\":\"invalid_json\"}");
        return 400;
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    if (!cJSON_IsString(name) || name->valuestring[0] == '\0') {
        cJSON_Delete(root);
        send_json(conn, 400, "{\"error\":\"name_required\"}");
        return 400;
    }

    const cJSON *license = cJSON_GetObjectItemCaseSensitive(root, "license");
    const cJSON *phone = cJSON_GetObjectItemCaseSensitive(root, "phone");
    const cJSON *pay_type = cJSON_GetObjectItemCaseSensitive(root, "pay_type");
    const cJSON *pay_rate = cJSON_GetObjectItemCaseSensitive(root, "pay_rate");
    const cJSON *status = cJSON_GetObjectItemCaseSensitive(root, "status");

    if (cJSON_IsString(pay_type) && !is_valid_pay_type(pay_type->valuestring)) {
        cJSON_Delete(root);
        send_json(conn, 400, "{\"error\":\"pay_type_invalid_per_km_percent_salary\"}");
        return 400;
    }

    const char *sql =
        "INSERT INTO drivers(name, license, phone, pay_type, pay_rate, status) "
        "VALUES(?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        cJSON_Delete(root);
        send_json(conn, 500, "{\"error\":\"db_prepare_failed\"}");
        return 500;
    }

    sqlite3_bind_text(stmt, 1, name->valuestring, -1, SQLITE_TRANSIENT);

    if (cJSON_IsString(license)) sqlite3_bind_text(stmt, 2, license->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 2);

    if (cJSON_IsString(phone)) sqlite3_bind_text(stmt, 3, phone->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 3);

    if (cJSON_IsString(pay_type)) sqlite3_bind_text(stmt, 4, pay_type->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 4);

    if (cJSON_IsNumber(pay_rate)) sqlite3_bind_double(stmt, 5, pay_rate->valuedouble);
    else sqlite3_bind_null(stmt, 5);

    if (cJSON_IsString(status)) sqlite3_bind_text(stmt, 6, status->valuestring, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_text(stmt, 6, "active", -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    cJSON_Delete(root);

    if (rc != SQLITE_DONE) {
        send_json(conn, 500, "{\"error\":\"db_insert_failed\"}");
        return 500;
    }

    long long new_id = sqlite3_last_insert_rowid(db);

    char resp[128];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"id\":%lld}", new_id);
    send_json(conn, 201, resp);
    return 201;
}

int handle_api_drivers(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);

    if (strcmp(ri->request_method, "GET") == 0) return drivers_list(conn);
    if (strcmp(ri->request_method, "POST") == 0) return drivers_create(conn);

    send_json(conn, 404, "{\"error\":\"not_found\"}");
    return 404;
}
