#include <assert.h>
#include <stdio.h>
#include "conescan_db.h"
#include "sqlite3.h"

void conescan_db_open(struct ConeScanDB* db, const char* path)
{
  assert(path);
  printf("loading db %s\n", path);
  sqlite3_open(path, &db->db);
}

void conescan_db_close(struct ConeScanDB* db)
{
  sqlite3_close(db->db);
}