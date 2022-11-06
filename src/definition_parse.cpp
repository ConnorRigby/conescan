#include <assert.h>
#include <string.h>

#include "tinyxml2.h"

#include "conescan_db.h"
#include "history.h"
#include "definition_parse.h"
#include "definition.h"

void closeMetadataFile(struct DefinitionParse* parse,
                       struct Definition* definition)
{
    if (parse->metadataFile) {
        delete parse->metadataFile;
        parse->metadataFile = NULL;
    }
    if (parse->metadataFilePath) {
        free(parse->metadataFilePath);
        parse->metadataFilePath = NULL;
    }

    definition_deinit(definition);
    memset(definition, 0, sizeof(struct Definition));
}

void loadScaling(struct Scaling* scaling, tinyxml2::XMLElement* xml)
{
    assert(xml);
    assert(scaling);

    const char* name = xml->Attribute("name");

    definition_scaling_add_string_value(
        &scaling->name,
        name
    );

    definition_scaling_add_string_value(
        &scaling->units,
        xml->Attribute("units")
    );

    definition_scaling_add_string_value(
        &scaling->toexpr,
        xml->Attribute("toexpr")
    );

    definition_scaling_add_string_value(
        &scaling->frexpr,
        xml->Attribute("frexpr")
    );

    definition_scaling_add_string_value(
        &scaling->format,
        xml->Attribute("format")
    );

    definition_scaling_add_string_value(
        &scaling->storagetype,
        xml->Attribute("storagetype")
    );

    definition_scaling_add_string_value(
        &scaling->endian,
        xml->Attribute("endian")
    );

    scaling->min = xml->FloatAttribute("min");
    scaling->max = xml->FloatAttribute("max");
    scaling->inc = xml->FloatAttribute("inc");
}

void loadScalings(struct DefinitionParse* parse,
                  struct Definition* definition, 
                  tinyxml2::XMLNode* rom)
{
    tinyxml2::XMLElement* scaling;
    scaling = rom->FirstChildElement("scaling");
    while (scaling) {
        definition->numScalings += 1;
        scaling = scaling->NextSiblingElement("scaling");
    }
    parse->console->AddLog("Processing %d scalings", definition->numScalings);
    definition_new_scalings(definition);

    scaling = rom->FirstChildElement("scaling");
    int index = 0;
    while (scaling) {
        const char* name = scaling->Attribute("name");
        if (name == NULL) {
            parse->console->AddLog("[error] Invalid scaling: missing name");
        }
        loadScaling(&definition->scalings[index], scaling);

        scaling = scaling->NextSiblingElement("scaling");
        index += 1;
    }
}

void loadTable(struct DefinitionParse* parse,
               struct Definition* definition,
               struct Table* table, 
               tinyxml2::XMLElement* xml)
{
    assert(table);
    assert(xml);
    assert(definition->scalings);

    const char* address_string = xml->Attribute("address");
    table->address = strtol(address_string, NULL, 16);
    table->level = xml->IntAttribute("level");
    table->elements = xml->IntAttribute("elements");
    definition_scaling_add_string_value(&table->name, xml->Attribute("name"));
    definition_scaling_add_string_value(&table->type, xml->Attribute("type"));
    definition_scaling_add_string_value(&table->category, xml->Attribute("category"));
    definition_scaling_add_string_value(&table->scaling, xml->Attribute("scaling"));
    assert(table->scaling);
    for (int i = 0; i < definition->numScalings; i++) {
        if (strcmp(table->scaling, definition->scalings[i].name) == 0) {
            table->Scaling = &definition->scalings[i];
            break;
        }
    }
    if (table->Scaling == NULL) {
        parse->console->AddLog("Could not locate scaling for table %s", table->name);
    }
}

void loadTables(struct DefinitionParse* parse,
                struct Definition* definition,
                tinyxml2::XMLNode* rom)
{
    int index = 0;
    int jndex = 0;
    tinyxml2::XMLElement* table;
    definition->numTables = 0;
    table = rom->FirstChildElement("table");
    while (table) {
        definition->numTables += 1;
        table = table->NextSiblingElement("table");
    }
    parse->console->AddLog("Processing %d tables", definition->numTables);
    definition_new_tables(&definition->tables, definition->numTables);

    /*
    numCells = 0;
    if (tableSelect) {
        free(tableSelect);
        tableSelect = NULL;
    }
    tableSelect = (bool*)malloc(sizeof(bool) * definition->numTables);
    assert(tableSelect);
    memset(tableSelect, 0, sizeof(bool) * definition->numTables);
    */

    // tables can contain more tables, so recurse each node again
    // also assign scaling

    table = rom->FirstChildElement("table");
    index = 0;
    while (table) {
        tinyxml2::XMLElement* Subtable;
        Subtable = table->FirstChildElement("table");
        definition->tables[index].numTables = 0;
        while (Subtable) {
            definition->tables[index].numTables += 1;
            Subtable = Subtable->NextSiblingElement("table");
        }
        definition_new_tables(&definition->tables[index].tables, 
                               definition->tables[index].numTables);

        table = table->NextSiblingElement("table");
        index += 1;
    }

    // itterate them again now that they're allocated
    table = rom->FirstChildElement("table");
    index = 0;
    jndex = 0;
    while (table) {
        loadTable(parse, definition, &definition->tables[index], table);
        //numCells += definition->tables[index].elements;
        tinyxml2::XMLElement* Subtable;
        Subtable = table->FirstChildElement("table");
        jndex = 0;
        while (Subtable) {
            loadTable(parse, definition, &definition->tables[index].tables[jndex], Subtable);
            //numCells += definition->tables[index].tables[jndex].elements;
            jndex += 1;
            Subtable = Subtable->NextSiblingElement("table");
        }
        index += 1;
        table = table->NextSiblingElement("table");
    }
    /*
    cellSelect = (bool*)malloc(sizeof(bool) * numCells);
    assert(cellSelect);
    memset(cellSelect, 0, sizeof(bool) * numCells);
    */
}

bool loadMetadataFile(struct DefinitionParse* parse,
                      struct Definition* definition)
{
    assert(parse->metadataFilePath);
    assert(parse->metadataFile == NULL);
    parse->metadataFile = new tinyxml2::XMLDocument();
    
    parse->console->AddLog("loading definition file %s\n", parse->metadataFilePath);
    if (parse->metadataFile->LoadFile(parse->metadataFilePath) != tinyxml2::XML_SUCCESS) {
        parse->console->AddLog("Failed to open metadata file %s\n", parse->metadataFile->ErrorStr());
        closeMetadataFile(parse, definition);
        return false;
    }
    printf("opened definition file %s\n", parse->metadataFilePath);
    parse->console->AddLog("Opened %s\n", parse->metadataFilePath);
    tinyxml2::XMLElement* def;
    def = parse->metadataFile->RootElement();
    tinyxml2::XMLNode* node;
    tinyxml2::XMLNode* rom;
    tinyxml2::XMLNode* romID;

    if (strcmp(def->Name(), "roms") == 0) {
        rom = def->FirstChildElement("rom");
        if (rom == NULL) {
            fprintf(stderr, "invalid metadata\n");
        }
        romID = rom->FirstChildElement("romid");
        node = romID->FirstChild();
        while (node) {
            const char* nodeName = node->Value();
            tinyxml2::XMLNode* nodeValue = node->FirstChild();
            if (nodeValue)
                definition_add_value(definition, nodeName, nodeValue->Value());

            node = node->NextSiblingElement();
        }
        parse->console->AddLog("metadata xmlid = %s", definition->xmlid);
        loadScalings(parse, definition, rom);
        loadTables(parse, definition, rom);
        delete parse->metadataFile;
        parse->metadataFile = NULL;
    }
    return true;
}

bool setMetadataFilePath(struct DefinitionParse* parse, char* path)
{
    assert(path);
    assert(parse->metadataFilePath == NULL);

    int len = strlen(path);
    if (len > 0) {
        parse->metadataFilePath = (char*)malloc(len + 1);
        assert(parse->metadataFilePath);
        memset(parse->metadataFilePath, 0, len + 1);
        strncpy(parse->metadataFilePath, path, len);
        return true;
    }
    return false;
}