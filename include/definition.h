#pragma once

typedef union ScaledValue {
    float    f32;
    uint8_t  u8;
    uint16_t u16;
} scaled_value_t;

struct Scaling {
    char* name;
    char* units;
    char* toexpr;
    char* frexpr;
    char* format;
    float min;
    float max;
    float inc;
    char* storagetype;
    char* endian;
};

struct Table {
    int level;
    char* type;
    bool swapxy;
    char* name;
    unsigned long address;
    int elements;
    char* category;
    int numTables;
    char* scaling;
    struct Scaling* Scaling;
    struct Table* tables;
};

struct Definition {
    // XML Fields
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
    
    int numScalings;
    int numTables;
    struct Scaling* scalings;
    struct Table* tables;
};

void definition_add_value(struct Definition* definition, 
                          const char* fieldName, 
                          const char* value);

void definition_new_scalings(struct Definition* definition);

void definition_scaling_add_string_value(char** field, const char* value);

void definition_new_tables(struct Table** tables, int count);

void definition_deinit(struct Definition* definition);

//bool loadScalingValue(struct Scaling* scaling, char* rom, scaled_value_t* value);

unsigned long definition_count_cells(struct Definition* definition);