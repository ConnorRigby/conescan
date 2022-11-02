#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <cstdlib>

#include "conescan_db.h"
#include "layout.h"
#include "sqlite3.h"

size_t conescan_db_load_layout(struct ConeScanDB* db, int id, const char** ini_data)
{
  if(*ini_data) {
    printf("ini data not empty!\n");
    free(*(char**)ini_data);
    *ini_data = NULL;
  }

  sqlite3_stmt* query;
  int rc;
  int length = 0;
  rc = sqlite3_prepare_v2(db->db, "SELECT data FROM layout WHERE id = ? LIMIT 1", -1, &query, 0);
  assert(rc == SQLITE_OK); 
  rc = sqlite3_bind_int(query, 1, id);
  assert(rc == SQLITE_OK); 
  rc = sqlite3_step(query);
  if(rc == SQLITE_ROW) {
    const char* tmp = (const char*)sqlite3_column_text(query, 0);
    length = strlen(tmp) + 1;
    if(length) {
      *ini_data = (char*)malloc(length);
      memset(*(char**)ini_data, 0, length);
      strncpy(*(char**)ini_data, tmp, length -1);
    }
  } else if (rc == SQLITE_DONE) { // no results 
    length = 0;
    *ini_data = NULL;
  } else { // error
    printf("unexpected SQLITE status(%d): %s\n", rc, sqlite3_errmsg(db->db));
    length = 0;
    *ini_data = NULL;
  }

  sqlite3_finalize(query);
  return length;
}

void conescan_db_save_layout(struct ConeScanDB* db, int id, const char* ini_data)
{
  sqlite3_stmt* statement;
  int rc;
  if(id == 0) {
    rc = sqlite3_prepare_v2(db->db, "INSERT into layout(data) values(?)", -1, &statement, 0);
    rc = sqlite3_bind_text(statement, 1, ini_data, -1, NULL);
    assert(rc == SQLITE_OK);
  } else {
    rc = sqlite3_prepare_v2(db->db, "UPDATE layout SET data=? WHERE ID=?", -1, &statement, 0);
    rc = sqlite3_bind_text(statement, 1, ini_data, -1, NULL);
    assert(rc == SQLITE_OK);
    rc = sqlite3_bind_int(statement, 2, id);
    assert(rc == SQLITE_OK);
  }
  rc = sqlite3_step(statement);
  assert(rc == SQLITE_DONE);
  sqlite3_finalize(statement);
}