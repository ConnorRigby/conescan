#pragma once

struct Definition {
    char* xmlid;
    unsigned long internalidaddress;
    char* internalidstring;
    char* ecuid;
    char* market;
    char* make;
    char* model;
    char* submodel;
    char* transmission;
    char* year;
    char* flashmethod;
    char* memmodel;
    char* checksummodule;
};

void definition_add_value(struct Definition* definition, 
                          const char* fieldName, 
                          const char* value);