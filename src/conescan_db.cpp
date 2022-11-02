#include "conescan_db.h"
#include "sqlite3.h"

void conescan_db_open(struct ConeScanDB* db, const char* path)
{
  sqlite3_open(path, &db->db);
}
