#pragma once

#include "conescan_db.h"

void conescan_db_load_history(struct ConeScanDB* db, const char* type, int max, char** data, int* count);
void conescan_db_add_history(struct ConeScanDB* db, const char* type, char* path);
