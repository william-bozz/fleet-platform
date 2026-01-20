#define _POSIX_C_SOURCE 200809L
#include "charts.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

#include <sqlite3.h>

#include "db.h"
#include "http.h"

typedef struct {
    int id;
    double v;
} Pair;

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} StrBuf;

static void sb_init(StrBuf *sb) {
    sb->cap = 4096;
    sb->len = 0;
    sb->buf = (char*)malloc(sb->cap);
    if (sb->buf) sb->buf[0] = '\0';
}

static void sb_free(StrBuf *sb) {
    free(sb->buf);
    sb->buf = NULL;
    sb->len = sb->cap = 0;
}

static void sb_ensure(StrBuf *sb, size_t extra) {
    if (!sb->buf) return;
    size_t need = sb->len + extra + 1;
    if (need <= sb->cap) return;
    size_t newcap = sb->cap;
    while (newcap < need) newcap *= 2;
    char *p = (char*)realloc(sb->buf, newcap);
    if (!p) return;
    sb->buf = p;
    sb->cap = newcap;
}

static void sb_appendf(StrBuf *sb, const char *fmt, ...) {
    if (!sb->buf) return;

    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);

    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n <= 0) { va_end(ap2); return; }

    sb_ensure(sb, (size_t)n);
    vsnprintf(sb->buf + sb->len, sb->cap - sb->len, fmt, ap2);
    va_end(ap2);

    sb->len += (size_t)n;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return -1;
}

static void url_decode_inplace(char *s) {
    if (!s) return;
    char *src = s;
    char *dst = s;

    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            int hi = hexval(src[1]);
            int lo = hexval(src[2]);
            if (hi >= 0 && lo >= 0) {
                *dst++ = (char)((hi << 4) | lo);
                src += 3;
            } else {
                *dst++ = *src++;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static int looks_like_date_yyyy_mm_dd(const char *s) {
    if (!s) return 0;
    if (strlen(s) < 10) return 0;
    return isdigit((unsigned char)s[0]) &&
           isdigit((unsigned char)s[1]) &&
           isdigit((unsigned char)s[2]) &&
           isdigit((unsigned char)s[3]) &&
           s[4] == '-' &&
           isdigit((unsigned char)s[5]) &&
           isdigit((unsigned char)s[6]) &&
           s[7] == '-' &&
           isdigit((unsigned char)s[8]) &&
           isdigit((unsigned char)s[9]);
}

static void get_query_param(const char *qs, const char *key, char *out, size_t outsz) {
    if (!out || outsz == 0) return;
    out[0] = '\0';
    if (!qs || !key) return;

    size_t klen = strlen(key);
    const char *p = qs;

    while (*p) {
        const char *amp = strchr(p, '&');
        size_t chunk_len = amp ? (size_t)(amp - p) : strlen(p);

        const char *eq = memchr(p, '=', chunk_len);
        if (eq) {
            size_t name_len = (size_t)(eq - p);
            if (name_len == klen && strncmp(p, key, klen) == 0) {
                size_t val_len = chunk_len - name_len - 1;
                if (val_len >= outsz) val_len = outsz - 1;
                memcpy(out, eq + 1, val_len);
                out[val_len] = '\0';
                url_decode_inplace(out);
                return;
            }
        }

        if (!amp) break;
        p = amp + 1;
    }
}

static void make_range(const char *from10, const char *to10,
                       char *from_dt, size_t from_sz,
                       char *to_dt, size_t to_sz,
                       int *use_filter) {
    *use_filter = 0;
    if (from10 && looks_like_date_yyyy_mm_dd(from10) && to10 && looks_like_date_yyyy_mm_dd(to10)) {
        snprintf(from_dt, from_sz, "%.10s 00:00:00", from10);
        snprintf(to_dt, to_sz, "%.10s 23:59:59", to10);
        *use_filter = 1;
        return;
    }
    if (from10 && looks_like_date_yyyy_mm_dd(from10) && (!to10 || !to10[0])) {
        snprintf(from_dt, from_sz, "%.10s 00:00:00", from10);
        snprintf(to_dt, to_sz, "9999-12-31 23:59:59");
        *use_filter = 1;
        return;
    }
    if (to10 && looks_like_date_yyyy_mm_dd(to10) && (!from10 || !from10[0])) {
        snprintf(from_dt, from_sz, "0000-01-01 00:00:00");
        snprintf(to_dt, to_sz, "%.10s 23:59:59", to10);
        *use_filter = 1;
        return;
    }
}

static Pair *query_pairs(sqlite3 *db, const char *sql_no_filter, const char *sql_with_filter,
                         const char *from_dt, const char *to_dt, int use_filter,
                         int *out_n) {
    *out_n = 0;
    const char *sql = use_filter ? sql_with_filter : sql_no_filter;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;

    if (use_filter) {
        sqlite3_bind_text(stmt, 1, from_dt, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, to_dt, -1, SQLITE_TRANSIENT);
    }

    int cap = 16;
    int n = 0;
    Pair *arr = (Pair*)malloc((size_t)cap * sizeof(Pair));
    if (!arr) { sqlite3_finalize(stmt); return NULL; }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (n >= cap) {
            cap *= 2;
            Pair *p = (Pair*)realloc(arr, (size_t)cap * sizeof(Pair));
            if (!p) { free(arr); sqlite3_finalize(stmt); return NULL; }
            arr = p;
        }
        arr[n].id = sqlite3_column_int(stmt, 0);
        arr[n].v  = sqlite3_column_double(stmt, 1);
        n++;
    }

    sqlite3_finalize(stmt);
    *out_n = n;
    return arr;
}

static char *render_bar_svg(const char *title, const char *x_label_prefix,
                            const Pair *pairs, int n) {
    const int W = 900;
    const int H = 420;
    const int M = 50;
    const int CH = H - 2 * M;
    const int CW = W - 2 * M;

    double maxv = 0.0;
    for (int i = 0; i < n; i++) if (pairs[i].v > maxv) maxv = pairs[i].v;
    if (maxv <= 0.0) maxv = 1.0;

    int bars = (n > 0) ? n : 1;
    int gap = 10;
    int barw = (CW - gap * (bars - 1)) / bars;
    if (barw < 6) barw = 6;

    StrBuf sb;
    sb_init(&sb);

    sb_appendf(&sb,
        "<svg xmlns='http://www.w3.org/2000/svg' width='%d' height='%d' viewBox='0 0 %d %d'>",
        W, H, W, H);

    sb_appendf(&sb, "<rect x='0' y='0' width='%d' height='%d' fill='white'/>", W, H);

    sb_appendf(&sb, "<text x='%d' y='%d' font-family='sans-serif' font-size='18'>%s</text>",
               M, 28, title);

    sb_appendf(&sb, "<line x1='%d' y1='%d' x2='%d' y2='%d' stroke='#333'/>",
               M, H - M, W - M, H - M);
    sb_appendf(&sb, "<line x1='%d' y1='%d' x2='%d' y2='%d' stroke='#333'/>",
               M, M, M, H - M);

    for (int t = 0; t <= 4; t++) {
        double frac = (double)t / 4.0;
        int y = (int)(H - M - frac * CH);
        double val = frac * maxv;
        sb_appendf(&sb, "<line x1='%d' y1='%d' x2='%d' y2='%d' stroke='#eee'/>",
                   M, y, W - M, y);
        sb_appendf(&sb, "<text x='%d' y='%d' font-family='sans-serif' font-size='10' fill='#666'>%.0f</text>",
                   10, y + 4, val);
    }

    if (n == 0) {
        sb_appendf(&sb, "<text x='%d' y='%d' font-family='sans-serif' font-size='14' fill='#666'>Sin datos</text>",
                   M + 10, M + 40);
        sb_appendf(&sb, "</svg>");
        return sb.buf;
    }

    for (int i = 0; i < n; i++) {
        double v = pairs[i].v;
        int bh = (int)((v / maxv) * CH);
        int x = M + i * (barw + gap);
        int y = (H - M) - bh;

        sb_appendf(&sb, "<rect x='%d' y='%d' width='%d' height='%d' fill='#4C78A8'/>",
                   x, y, barw, bh);

        sb_appendf(&sb, "<text x='%d' y='%d' font-family='sans-serif' font-size='10' fill='#333' text-anchor='middle'>%.0f</text>",
                   x + barw / 2, y - 4, v);

        char lbl[64];
        snprintf(lbl, sizeof(lbl), "%s%d", x_label_prefix, pairs[i].id);
        sb_appendf(&sb, "<text x='%d' y='%d' font-family='sans-serif' font-size='10' fill='#333' text-anchor='middle'>%s</text>",
                   x + barw / 2, H - M + 16, lbl);
    }

    sb_appendf(&sb, "</svg>");
    return sb.buf;
}

static int handle_chart_common(struct mg_connection *conn,
                               const char *title,
                               const char *x_prefix,
                               const char *sql_no_filter,
                               const char *sql_with_filter,
                               const char *date_column,
                               int is_date_based) {
    (void)date_column;
    (void)is_date_based;

    const struct mg_request_info *ri = mg_get_request_info(conn);
    const char *qs = ri->query_string;

    char from10[64], to10[64];
    get_query_param(qs, "from", from10, sizeof(from10));
    get_query_param(qs, "to", to10, sizeof(to10));

    char from_dt[32] = {0}, to_dt[32] = {0};
    int use_filter = 0;
    make_range(from10, to10, from_dt, sizeof(from_dt), to_dt, sizeof(to_dt), &use_filter);

    sqlite3 *db = db_handle();

    int n = 0;
    Pair *pairs = query_pairs(db, sql_no_filter, sql_with_filter, from_dt, to_dt, use_filter, &n);
    if (!pairs && n == 0) {
        send_svg(conn, 500, "<svg xmlns='http://www.w3.org/2000/svg' width='600' height='120'><text x='10' y='40'>error db</text></svg>");
        return 500;
    }

    char *svg = render_bar_svg(title, x_prefix, pairs, n);
    free(pairs);

    if (!svg) {
        send_svg(conn, 500, "<svg xmlns='http://www.w3.org/2000/svg' width='600' height='120'><text x='10' y='40'>error svg</text></svg>");
        return 500;
    }

    send_svg(conn, 200, svg);
    free(svg);
    return 200;
}

int handle_chart_fuel_liters_by_truck(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "GET") != 0) { send_json(conn, 404, "{\"error\":\"not_found\"}"); return 404; }

    const char *sql_no =
        "SELECT truck_id, SUM(liters) AS v "
        "FROM fuel_entries "
        "GROUP BY truck_id ORDER BY truck_id;";

    const char *sql_f =
        "SELECT truck_id, SUM(liters) AS v "
        "FROM fuel_entries "
        "WHERE fueled_at BETWEEN ? AND ? "
        "GROUP BY truck_id ORDER BY truck_id;";

    return handle_chart_common(conn, "Fuel: litros por camión", "T", sql_no, sql_f, "fueled_at", 1);
}

int handle_chart_fuel_cost_by_truck(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "GET") != 0) { send_json(conn, 404, "{\"error\":\"not_found\"}"); return 404; }

    const char *sql_no =
        "SELECT truck_id, SUM(COALESCE(total_cost,0)) AS v "
        "FROM fuel_entries "
        "GROUP BY truck_id ORDER BY truck_id;";

    const char *sql_f =
        "SELECT truck_id, SUM(COALESCE(total_cost,0)) AS v "
        "FROM fuel_entries "
        "WHERE fueled_at BETWEEN ? AND ? "
        "GROUP BY truck_id ORDER BY truck_id;";

    return handle_chart_common(conn, "Fuel: costo total por camión", "T", sql_no, sql_f, "fueled_at", 1);
}

int handle_chart_km_by_truck(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "GET") != 0) { send_json(conn, 404, "{\"error\":\"not_found\"}"); return 404; }

    const char *sql_no =
        "SELECT truck_id, SUM(km) AS v "
        "FROM km_logs "
        "GROUP BY truck_id ORDER BY truck_id;";

    const char *sql_f =
        "SELECT truck_id, SUM(km) AS v "
        "FROM km_logs "
        "WHERE logged_at BETWEEN ? AND ? "
        "GROUP BY truck_id ORDER BY truck_id;";

    return handle_chart_common(conn, "Kilómetros por camión", "T", sql_no, sql_f, "logged_at", 1);
}

int handle_chart_pay_by_driver(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "GET") != 0) { send_json(conn, 404, "{\"error\":\"not_found\"}"); return 404; }

    const char *sql_no =
        "SELECT driver_id, SUM(amount) AS v "
        "FROM driver_payments "
        "GROUP BY driver_id ORDER BY driver_id;";

    const char *sql_f =
        "SELECT driver_id, SUM(amount) AS v "
        "FROM driver_payments "
        "WHERE paid_at BETWEEN ? AND ? "
        "GROUP BY driver_id ORDER BY driver_id;";

    return handle_chart_common(conn, "Pagos por conductor", "D", sql_no, sql_f, "paid_at", 1);
}

int handle_dashboard(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    if (strcmp(ri->request_method, "GET") != 0) { send_json(conn, 404, "{\"error\":\"not_found\"}"); return 404; }

    const char *qs = ri->query_string;

    char from10[64], to10[64];
    get_query_param(qs, "from", from10, sizeof(from10));
    get_query_param(qs, "to", to10, sizeof(to10));

    char qpart[256];
    qpart[0] = '\0';

    if ((from10[0] && looks_like_date_yyyy_mm_dd(from10)) || (to10[0] && looks_like_date_yyyy_mm_dd(to10))) {
        snprintf(qpart, sizeof(qpart), "from=%.10s&to=%.10s", from10[0] ? from10 : "", to10[0] ? to10 : "");
    }

    StrBuf sb;
    sb_init(&sb);

    sb_appendf(&sb,
        "<!doctype html><html lang='es'><head>"
        "<meta charset='utf-8'/>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
        "<title>Fleet Dashboard v0.6</title>"
        "</head><body>"
        "<h1>Fleet Dashboard v0.6</h1>"
        "<p>Filtro opcional por fechas (YYYY-MM-DD). Deja vacío si no quieres filtrar.</p>"
        "<form method='get' action='/dashboard'>"
        "From: <input name='from' value='%.10s' placeholder='YYYY-MM-DD'/> "
        "To: <input name='to' value='%.10s' placeholder='YYYY-MM-DD'/> "
        "<button type='submit'>Aplicar</button>"
        "</form>"
        "<p><a href='/'>Volver al frontend</a></p>"
        "<hr/>",
        from10[0] ? from10 : "",
        to10[0] ? to10 : ""
    );

    sb_appendf(&sb, "<h2>Fuel</h2>");
    sb_appendf(&sb, "<div><img alt='fuel liters' src='/charts/fuel_liters_by_truck.svg%s%s' /></div>",
               qpart[0] ? "?" : "", qpart[0] ? qpart : "");
    sb_appendf(&sb, "<div><img alt='fuel cost' src='/charts/fuel_cost_by_truck.svg%s%s' /></div>",
               qpart[0] ? "?" : "", qpart[0] ? qpart : "");

    sb_appendf(&sb, "<h2>Kilómetros</h2>");
    sb_appendf(&sb, "<div><img alt='km' src='/charts/km_by_truck.svg%s%s' /></div>",
               qpart[0] ? "?" : "", qpart[0] ? qpart : "");

    sb_appendf(&sb, "<h2>Pagos</h2>");
    sb_appendf(&sb, "<div><img alt='pay' src='/charts/pay_by_driver.svg%s%s' /></div>",
               qpart[0] ? "?" : "", qpart[0] ? qpart : "");

    sb_appendf(&sb, "</body></html>");

    send_html(conn, 200, sb.buf);
    sb_free(&sb);
    return 200;
}
