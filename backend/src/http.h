#pragma once
#include "civetweb.h"

char *read_request_body(struct mg_connection *conn, long long *out_len);

void send_json(struct mg_connection *conn, int status, const char *json);
void send_html(struct mg_connection *conn, int status, const char *html);
void send_svg(struct mg_connection *conn, int status, const char *svg);
