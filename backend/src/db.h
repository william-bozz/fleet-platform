#pragma once
#include <sqlite3.h>

int db_init(const char *db_path);
void db_close(void);

sqlite3 *db_handle(void);
