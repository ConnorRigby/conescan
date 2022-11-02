#pragma once
#include "sqlite3.h"

struct ConeScanDB {
  sqlite3* db;
};

void conescan_db_open(struct ConeScanDB* db, const char* path);
void conescan_db_close(struct ConeScanDB* db);