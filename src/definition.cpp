#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "definition.h"

// populates an address by strtll'ing the string param
signed int get_address(unsigned long* address, const char* param)
{
  uint8_t base = 10;
  char* extra = NULL;
  long long out = 0;
  const char* substr = strstr(param, "0x");
  if(substr) {
    param = substr;
    base=16;
  } else if(strcmp(param, "abcdef") == 1) { // sorry
    base = 16;
  }

  out = strtoll(param, &extra, base);

  if(strlen(extra) > 0) return strlen(extra);

  *address = out;
  return 0;
}

void definition_add_value(struct Definition* definition, 
                          const char* fieldName, 
                          const char* value)
{
  if(value == NULL) return;
  int length = strlen(value);

  if(length <= 0) {
    return;
  }

  char** key = NULL;
  if(strcmp(fieldName, "xmlid") == 0) {
    key = &definition->xmlid;
  } else if(strcmp(fieldName, "internalidaddress") == 0) {
    get_address(&definition->internalidaddress, value);
    return;
  } else if(strcmp(fieldName, "internalidstring") == 0) {
    key = &definition->internalidstring;
  } else if(strcmp(fieldName, "ecuid") == 0) {
    key = &definition->ecuid;
  } else if(strcmp(fieldName, "market") == 0) {
    key = &definition->market;
  } else if(strcmp(fieldName, "make") == 0) {
    key = &definition->make;
  } else if(strcmp(fieldName, "model") == 0) {
    key = &definition->model;
  } else if(strcmp(fieldName, "submodel") == 0) {
    key = &definition->submodel;
  } else if(strcmp(fieldName, "transmission") == 0) {
    key = &definition->transmission;
  } else if(strcmp(fieldName, "year") == 0) {
    key = &definition->year;
  } else if(strcmp(fieldName, "flashmethod") == 0) {
    key = &definition->flashmethod;
  } else if(strcmp(fieldName, "memmodel") == 0) {
    key = &definition->memmodel;
  } else if(strcmp(fieldName, "checksummodule") == 0) {
    key = &definition->checksummodule;
  } else {
    fprintf(stderr, "Unknown field name %s\n", fieldName);
    return;
  }
  *key=(char*)malloc(length+1);
  strncpy(*key, value, length);
  char* iSuck = *key;
  iSuck[length] = '\0';
}

void definition_new_scalings(struct Definition* definition)
{
  if(definition->numScalings) {
    definition->scalings = (struct Scaling*)malloc(sizeof(struct Scaling) * definition->numScalings);
    memset(definition->scalings, 0, sizeof(struct Scaling) * definition->numScalings);
  }
}

void definition_scaling_add_string_value(char** field, const char* value)
{
  if(value == NULL) return;
  int length = strlen(value);

  if(length > 0) {
    *field = (char*)malloc(length + 1);
    strncpy(*field, value, length);
    char* iSuck = *field;
    iSuck[length] = '\0';
  }
}

void definition_new_tables(struct Table** tables, int count)
{
  if(*tables) {
    printf("tables allready malloced? %p\n", *tables);
    return;
  }
  if(count <= 0) {
    // *tables = NULL;
    return;
  }
  *tables = (struct Table*)malloc(sizeof(struct Table) * count);
  memset(*tables, 0, sizeof(struct Table) * count);
}

void definition_deinit(struct Definition* definition)
{
  if(definition->xmlid) {
    free(definition->xmlid);
    definition->xmlid = NULL;
  }
  if(definition->internalidstring) {
    free(definition->internalidstring);
    definition->internalidstring = NULL;
  }
  if(definition->ecuid) {
    free(definition->ecuid);
    definition->ecuid = NULL;
  }
  if(definition->market) {
    free(definition->market);
    definition->market = NULL;
  }
  if(definition->make) {
    free(definition->make);
    definition->make = NULL;
  }
  if(definition->model) {
    free(definition->model);
    definition->model = NULL;
  }
  if(definition->submodel) {
    free(definition->submodel);
    definition->submodel = NULL;
  }
  if(definition->transmission) {
    free(definition->transmission);
    definition->transmission = NULL;
  }
  if(definition->year) {
    free(definition->year);
    definition->year = NULL;
  }
  if(definition->flashmethod) {
    free(definition->flashmethod);
    definition->flashmethod = NULL;
  }
  if(definition->memmodel) {
    free(definition->memmodel);
    definition->memmodel = NULL;
  }
  if(definition->checksummodule) {
    free(definition->checksummodule);
    definition->checksummodule = NULL;
  }

  if(definition->scalings) {
    for(int i = 0; i < definition->numScalings; i++) {
      if(definition->scalings[i].endian) {
        free(definition->scalings[i].endian);
        definition->scalings[i].endian = NULL;
      }
      if(definition->scalings[i].format) {
        free(definition->scalings[i].format);
        definition->scalings[i].format = NULL;
      }
      if(definition->scalings[i].frexpr) {
        free(definition->scalings[i].frexpr);
        definition->scalings[i].frexpr = NULL;
      }
      if(definition->scalings[i].name) {
        free(definition->scalings[i].name);
        definition->scalings[i].name = NULL;
      }
      if(definition->scalings[i].storagetype) {
        free(definition->scalings[i].storagetype);
        definition->scalings[i].storagetype = NULL;
      }
      if(definition->scalings[i].toexpr) {
        free(definition->scalings[i].toexpr);
        definition->scalings[i].toexpr = NULL;
      }
      if(definition->scalings[i].units) {
        free(definition->scalings[i].units);
        definition->scalings[i].units = NULL;
      }
    }
    free(definition->scalings);
    definition->scalings = NULL;
  }

  if(definition->tables) {
    for(int i = 0; i < definition->numTables; i++) {
      if(definition->tables[i].category) {
        free(definition->tables[i].category);
        definition->tables[i].category = NULL;
      }
      if(definition->tables[i].name) {
        free(definition->tables[i].name);
        definition->tables[i].name = NULL;
      }
      if(definition->tables[i].type) {
        free(definition->tables[i].type);
        definition->tables[i].type = NULL;
      }
      if(definition->tables[i].scaling) {
        free(definition->tables[i].scaling);
        definition->tables[i].scaling = NULL;
      }

      if(definition->tables[i].tables) {
        for(int j = 0; j < definition->tables[i].numTables; j++) {
          if(definition->tables[i].tables[j].category) {
            free(definition->tables[i].tables[j].category);
            definition->tables[i].tables[j].category = NULL;
          }
          if(definition->tables[i].tables[j].name) {
            free(definition->tables[i].tables[j].name);
            definition->tables[i].tables[j].name = NULL;
          }
          if(definition->tables[i].tables[j].type) {
            free(definition->tables[i].tables[j].type);
            definition->tables[i].tables[j].type = NULL;
          }
          if(definition->tables[i].tables[j].scaling) {
            free(definition->tables[i].tables[j].scaling);
            definition->tables[i].tables[j].scaling = NULL;
          }
        }
        free(definition->tables[i].tables);
        definition->tables[i].tables = NULL;
      }
    }
    free(definition->tables);
    definition->tables = NULL;
  }

  if(definition->scalings) {
    free(definition->scalings);
    definition->scalings = NULL;
  }
}

unsigned long definition_count_cells(struct Definition* definition)
{
    unsigned long numCells = 0;
    struct Table* table;
    for (int i = 0; i < definition->numTables; i++) {
        numCells += definition->tables[i].elements;
        if (definition->tables[i].tables) {
            for (int j = 0; j < definition->tables[i].numTables; j++) {
                numCells += definition->tables[i].tables[j].elements;
            }
        }
    }
    return numCells;
}
