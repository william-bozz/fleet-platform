#define _POSIX_C_SOURCE 200809L
#include "db.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

static sqlite3 *g_db = NULL;

static int ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        return -1;
    }
    if (mkdir(path, 0755) != 0) return -1;
    return 0;
}

static int db_exec(sqlite3 *db, const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite error: %s\nSQL: %s\n", err ? err : "unknown", sql);
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

static int create_schema(sqlite3 *db) {
    const char *schema =
        "PRAGMA foreign_keys = ON;"
        "PRAGMA journal_mode = WAL;"

        "CREATE TABLE IF NOT EXISTS trucks ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  unit_number TEXT NOT NULL,"
        "  vin TEXT,"
        "  year INTEGER,"
        "  make TEXT,"
        "  model TEXT,"
        "  engine TEXT,"
        "  status TEXT DEFAULT 'active',"
        "  current_km REAL DEFAULT 0,"
        "  created_at TEXT DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_trucks_unit ON trucks(unit_number);"

        "CREATE TABLE IF NOT EXISTS trailers ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  unit_number TEXT NOT NULL,"
        "  vin TEXT,"
        "  type TEXT NOT NULL,"  /* reefer | dry_van | flatbed */
        "  status TEXT DEFAULT 'active',"
        "  current_km REAL DEFAULT 0,"
        "  created_at TEXT DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_trailers_unit ON trailers(unit_number);"
        "CREATE INDEX IF NOT EXISTS idx_trailers_type ON trailers(type);"

        "CREATE TABLE IF NOT EXISTS drivers ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  license TEXT,"
        "  phone TEXT,"
        "  pay_type TEXT,"       /* per_km | percent | salary */
        "  pay_rate REAL,"       /* número según pay_type */
        "  status TEXT DEFAULT 'active',"
        "  created_at TEXT DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_drivers_name ON drivers(name);"
        "CREATE INDEX IF NOT EXISTS idx_drivers_license ON drivers(license);";


    return db_exec(db, schema);
}

int db_init(const char *db_path) {
    if (ensure_dir("data") != 0) {
        fprintf(stderr, "No pude crear/usar carpeta data/: %s\n", strerror(errno));
        return -1;
    }

    int rc = sqlite3_open(db_path, &g_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "No pude abrir DB: %s\n", sqlite3_errmsg(g_db));
        return -1;
    }

    if (create_schema(g_db) != 0) {
        sqlite3_close(g_db);
        g_db = NULL;
        return -1;
    }

    return 0;
}

void db_close(void) {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

sqlite3 *db_handle(void) {
    return g_db;
}
