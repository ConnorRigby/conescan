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

  if(strlen(extra) > 0) return -(strlen(extra));

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
  *key=(char*)malloc(length);
  strcpy(*key, value);
}