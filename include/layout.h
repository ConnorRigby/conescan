#pragma once
#include <stdint.h>

#include "conescan_db.h"

size_t conescan_db_load_layout(struct ConeScanDB* db, int id, const char** ini_data);
void conescan_db_save_layout(struct ConeScanDB* db, int id, const char* ini_data);
