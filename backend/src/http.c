#include "http.h"

#include <stdlib.h>
#include <string.h>

static void send_with_type(struct mg_connection *conn, int status,
                           const char *content_type, const char *body) {
    const char *status_text =
        (status == 200) ? "OK" :
        (status == 201) ? "Created" :
        (status == 400) ? "Bad Request" :
        (status == 404) ? "Not Found" :
        (status == 500) ? "Internal Server Error" : "OK";

    size_t len = body ? strlen(body) : 0;

    mg_printf(conn,
              "HTTP/1.1 %d %s\r\n"
              "Content-Type: %s\r\n"
              "Cache-Control: no-store\r\n"
              "Content-Length: %zu\r\n"
              "\r\n"
              "%s",
              status, status_text, content_type, len, body ? body : "");
}

char *read_request_body(struct mg_connection *conn, long long *out_len) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    long long len = ri->content_length;

    if (len <= 0 || len > (10LL * 1024 * 1024)) {
        *out_len = 0;
        return NULL;
    }

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) return NULL;

    long long read_total = 0;
    while (read_total < len) {
        int r = mg_read(conn, buf + read_total, (size_t)(len - read_total));
        if (r <= 0) break;
        read_total += r;
    }

    buf[read_total] = '\0';
    *out_len = read_total;
    return buf;
}

void send_json(struct mg_connection *conn, int status, const char *json) {
    send_with_type(conn, status, "application/json", json);
}

void send_html(struct mg_connection *conn, int status, const char *html) {
    send_with_type(conn, status, "text/html; charset=utf-8", html);
}

void send_svg(struct mg_connection *conn, int status, const char *svg) {
    send_with_type(conn, status, "image/svg+xml; charset=utf-8", svg);
}
