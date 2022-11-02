#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "conescan_db.h"
#include "sqlite3.h"

void conescan_db_load_history(struct ConeScanDB* db, const char* type, int max, char** data, int* count)
{
  sqlite3_stmt* query;
  int rc;
  rc = sqlite3_prepare_v2(db->db, "SELECT path FROM history WHERE type = ? LIMIT ?", -1, &query, 0);
  assert(rc == SQLITE_OK);
  rc = sqlite3_bind_text(query, 1, type, -1, NULL);
  assert(rc == SQLITE_OK);
  rc = sqlite3_bind_int(query, 2, max);
  assert(rc == SQLITE_OK);
  *count = 0;
  for(int i = 0; i < max; i++)  {
    data[i][0] = '\0';
  }
  do {
    rc = sqlite3_step(query);
    if(rc != SQLITE_ROW) break;
    assert(rc == SQLITE_ROW);
    const char* tmp = (const char*)sqlite3_column_text(query, 0);

    int len = strlen(tmp);
    strncpy(data[*count], tmp, len);
    *count = *count + 1;
    if(*count == max) break;
  } while(rc == SQLITE_ROW);
  sqlite3_finalize(query);
}

void conescan_db_add_history(struct ConeScanDB* db, const char* type, char* path)
{
  sqlite3_stmt* query;
  int rc;
  rc = sqlite3_prepare_v2(db->db, "INSERT into history(type, path) values(?, ?);", -1, &query, 0);
  assert(rc == SQLITE_OK);
  rc = sqlite3_bind_text(query, 1, type, -1, NULL);
  assert(rc == SQLITE_OK);
  rc = sqlite3_bind_text(query, 2, path, -1, NULL);
  assert(rc == SQLITE_OK);
  rc = sqlite3_step(query);
  assert(rc == SQLITE_DONE);
  sqlite3_finalize(query);
}