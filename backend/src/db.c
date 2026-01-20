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
        "  type TEXT NOT NULL,"
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
        "  pay_type TEXT,"
        "  pay_rate REAL,"
        "  status TEXT DEFAULT 'active',"
        "  created_at TEXT DEFAULT (datetime('now'))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_drivers_name ON drivers(name);"
        "CREATE INDEX IF NOT EXISTS idx_drivers_license ON drivers(license);"

        "CREATE TABLE IF NOT EXISTS loads ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  reference TEXT NOT NULL,"
        "  shipper TEXT,"
        "  pickup_location TEXT,"
        "  delivery_location TEXT,"
        "  pickup_date TEXT,"
        "  delivery_date TEXT,"
        "  commodity TEXT,"
        "  weight_kg REAL,"
        "  rate REAL,"
        "  currency TEXT DEFAULT 'USD',"
        "  distance_km REAL,"
        "  status TEXT DEFAULT 'planned',"
        "  truck_id INTEGER NOT NULL,"
        "  trailer_id INTEGER NOT NULL,"
        "  driver_id INTEGER NOT NULL,"
        "  created_at TEXT DEFAULT (datetime('now')),"
        "  FOREIGN KEY(truck_id) REFERENCES trucks(id),"
        "  FOREIGN KEY(trailer_id) REFERENCES trailers(id),"
        "  FOREIGN KEY(driver_id) REFERENCES drivers(id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_loads_ref ON loads(reference);"
        "CREATE INDEX IF NOT EXISTS idx_loads_status ON loads(status);"
        "CREATE INDEX IF NOT EXISTS idx_loads_truck ON loads(truck_id);"
        "CREATE INDEX IF NOT EXISTS idx_loads_trailer ON loads(trailer_id);"
        "CREATE INDEX IF NOT EXISTS idx_loads_driver ON loads(driver_id);"

        "CREATE TABLE IF NOT EXISTS fuel_entries ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  truck_id INTEGER NOT NULL,"
        "  load_id INTEGER,"
        "  driver_id INTEGER,"
        "  liters REAL NOT NULL,"
        "  total_cost REAL,"
        "  currency TEXT DEFAULT 'USD',"
        "  odometer_km REAL,"
        "  location TEXT,"
        "  fueled_at TEXT DEFAULT (datetime('now')),"
        "  FOREIGN KEY(truck_id) REFERENCES trucks(id),"
        "  FOREIGN KEY(load_id) REFERENCES loads(id),"
        "  FOREIGN KEY(driver_id) REFERENCES drivers(id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_fuel_truck ON fuel_entries(truck_id);"
        "CREATE INDEX IF NOT EXISTS idx_fuel_load ON fuel_entries(load_id);"
        "CREATE INDEX IF NOT EXISTS idx_fuel_driver ON fuel_entries(driver_id);"
        "CREATE INDEX IF NOT EXISTS idx_fuel_date ON fuel_entries(fueled_at);"

        "CREATE TABLE IF NOT EXISTS km_logs ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  truck_id INTEGER NOT NULL,"
        "  load_id INTEGER,"
        "  km REAL NOT NULL,"
        "  odometer_start REAL,"
        "  odometer_end REAL,"
        "  logged_at TEXT DEFAULT (datetime('now')),"
        "  FOREIGN KEY(truck_id) REFERENCES trucks(id),"
        "  FOREIGN KEY(load_id) REFERENCES loads(id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_km_truck ON km_logs(truck_id);"
        "CREATE INDEX IF NOT EXISTS idx_km_load ON km_logs(load_id);"
        "CREATE INDEX IF NOT EXISTS idx_km_date ON km_logs(logged_at);"

        "CREATE TABLE IF NOT EXISTS driver_payments ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  driver_id INTEGER NOT NULL,"
        "  load_id INTEGER,"
        "  amount REAL NOT NULL,"
        "  currency TEXT DEFAULT 'USD',"
        "  method TEXT,"
        "  notes TEXT,"
        "  paid_at TEXT DEFAULT (datetime('now')),"
        "  FOREIGN KEY(driver_id) REFERENCES drivers(id),"
        "  FOREIGN KEY(load_id) REFERENCES loads(id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_pay_driver ON driver_payments(driver_id);"
        "CREATE INDEX IF NOT EXISTS idx_pay_load ON driver_payments(load_id);"
        "CREATE INDEX IF NOT EXISTS idx_pay_date ON driver_payments(paid_at);";

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
