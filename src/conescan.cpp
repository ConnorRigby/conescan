
#include <unistd.h>

#include "imgui.h"
#include "tinyxml2.h"

#ifdef WASM
// nothing to see here
#include "emscripten.h"
#else
#include "nfd.h"
#endif

#include "conescan.h"
#include "conescan_db.h"
#include "history.h"
#include "definition.h"
#include "console.h"
#include "layout.h"

#ifdef WASM
const char* db_path = "/conescan/conescan.db";
#else
const char* db_path = "conescan.db";
#endif

char* metadataFilePath = NULL;
tinyxml2::XMLDocument* metadataFile = NULL;
static ConeScan::Console console;
bool show_console_window = true;
bool show_demo_window = false;
struct Definition definition;
struct ConeScanDB db;

// stores opened table editors
bool* tableSelect = NULL;

// set by the db history API
int historyCount = 0;
int pathHistoryMax = 5;
char **pathHistory;

// for saving/loading layout
const char *iniData = NULL;
size_t iniSize = 0;

char* getFileOpenPath() 
#ifdef WASM
{
  return "/metadata/lfg2ee.xml";
}
#else
{
  nfdchar_t *outPath = NULL;
  nfdresult_t result = NFD_OpenDialog(NULL, NULL, &outPath);
      
  if (result == NFD_OKAY) {
    return outPath;
  } else if (result == NFD_CANCEL) {
    fprintf(stderr, "User pressed cancel.");
    return NULL;
  }

  printf("Error: %s\n", NFD_GetError());
  return NULL;
}
#endif

void closeMetadataFile() 
{
  if(metadataFile) {
    delete metadataFile;
    metadataFile = NULL;
  }
  if(metadataFilePath) {
    free(metadataFilePath);
    metadataFilePath = NULL;
  }
  definition_deinit(&definition);
  memset(&definition, 0, sizeof(struct Definition));
  console.AddLog("Closed metadata");
}

void loadScaling(struct Scaling* scaling, tinyxml2::XMLElement * xml)
{
  if(xml == NULL) return;
  if(scaling == NULL) return;

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

void loadScalings(tinyxml2::XMLNode *rom)
{
  tinyxml2::XMLElement *scaling;
  scaling = rom->FirstChildElement("scaling");
  while(scaling) {
    definition.numScalings +=1;
    scaling = scaling->NextSiblingElement("scaling");
  }
  console.AddLog("Processing %d scalings", definition.numScalings);
  definition_new_scalings(&definition);

  scaling = rom->FirstChildElement("scaling");
  int index = 0;
  while(scaling) {
    const char* name = scaling->Attribute("name");
    if(name == NULL) {
      console.AddLog("[error] Invalid scaling: missing name");
    }
    loadScaling(&definition.scalings[index], scaling);

    scaling = scaling->NextSiblingElement("scaling");
    index+=1;
  }
}

void loadTable(struct Table* table, tinyxml2::XMLElement *xml)
{
  if(table == NULL) return;
  if(xml == NULL) return;
  table->level = xml->IntAttribute("level");
  table->elements = xml->IntAttribute("elements");
  definition_scaling_add_string_value(&table->name, xml->Attribute("name"));
  definition_scaling_add_string_value(&table->category, xml->Attribute("category"));
  definition_scaling_add_string_value(&table->scaling, xml->Attribute("scaling"));
}

void loadTables(tinyxml2::XMLNode *rom)
{
  int index = 0;
  int jndex = 0;
  tinyxml2::XMLElement *table;
  definition.numTables = 0;
  table = rom->FirstChildElement("table");
  while(table) {
    definition.numTables +=1;
    table = table->NextSiblingElement("table");
  }
  console.AddLog("Processing %d tables", definition.numTables);
  definition_new_tables(&definition.tables, definition.numTables);
  if(tableSelect) {
    free(tableSelect);
    tableSelect = NULL;
  }
  tableSelect = (bool*)malloc(sizeof(bool) * definition.numTables);
  memset(tableSelect, 0, sizeof(bool) * definition.numTables);

  // tables can contain more tables, so recurse each node again
  // also assign scaling

  table = rom->FirstChildElement("table");
  index = 0;
  while(table) {
    tinyxml2::XMLElement *Subtable;
    Subtable = table->FirstChildElement("table");
    // definition.tables[index].numTables = 0;
    while(Subtable) {
      definition.tables[index].numTables+=1;
      Subtable = Subtable->NextSiblingElement("table");
    }
    definition_new_tables(&definition.tables[index].tables, definition.tables[index].numTables);

    table = table->NextSiblingElement("table");
    index+=1;
  }

  // itterate them again now that they're allocated
  table = rom->FirstChildElement("table");
  index = 0;
  jndex = 0;
  while(table) {
    loadTable(&definition.tables[index], table);
    tinyxml2::XMLElement *Subtable;
    Subtable = table->FirstChildElement("table");
    jndex = 0;
    while(Subtable) {
      loadTable(&definition.tables[index].tables[jndex], Subtable);
      jndex +=1;
      Subtable = Subtable->NextSiblingElement("table");
    }
    index+=1;
    table = table->NextSiblingElement("table");
  }
}

void loadMetadataFile(void) 
{
  if(metadataFilePath == NULL) {
    if(metadataFile) delete metadataFile;
    return;
  }

  if(metadataFile == NULL) {
    metadataFile = new tinyxml2::XMLDocument();
  }
  printf("loading definition file %s\n", metadataFilePath);
  console.AddLog("loading definition file %s\n", metadataFilePath);
  if(metadataFile->LoadFile(metadataFilePath) != tinyxml2::XML_SUCCESS) {
    console.AddLog("Failed to open metadata file %s\n", metadataFile->ErrorStr());
    closeMetadataFile();
    return;
  }
  printf("opened definition file %s\n", metadataFilePath);
  console.AddLog("Opened %s\n", metadataFilePath);
  tinyxml2::XMLElement *def;
  def = metadataFile->RootElement();
  tinyxml2::XMLNode *node;
  tinyxml2::XMLNode *rom;
  tinyxml2::XMLNode *romID;

  if(strcmp(def->Name(), "roms") == 0) {
    rom = def->FirstChildElement("rom");
    if(rom == NULL) {
      fprintf(stderr, "invalid metadata\n");
    }
    romID = rom->FirstChildElement("romid");
    node = romID->FirstChild();
    while(node) {
      const char* nodeName = node->Value();
      tinyxml2::XMLNode* nodeValue = node->FirstChild();
      if(nodeValue)
        definition_add_value(&definition, nodeName, nodeValue->Value());

      node = node->NextSiblingElement();
    }
    console.AddLog("metadata xmlid = %s", definition.xmlid);
    loadScalings(rom);
    loadTables(rom);
  }
  bool addToHistory = true;
  for(int i = 0; i < historyCount; i++) {
    if(strcmp(pathHistory[i], metadataFilePath) == 0)
      addToHistory = false;
  }
  if(addToHistory) {
    printf("adding %s to history\n", metadataFilePath);
    conescan_db_add_history(&db, "Definition", metadataFilePath);
    printf("added %s to history\n", metadataFilePath);
    conescan_db_load_history(&db, "Definition", pathHistoryMax, pathHistory, &historyCount);
    console.AddLog("added entry to history%s", metadataFilePath);
    assert(sqlite3_db_cacheflush(db.db) == SQLITE_OK);
  }  
}

void ConeScan::Init()
{
  memset(&definition, 0, sizeof(struct Definition));
  memset(&db, 0, sizeof(struct ConeScanDB));
  conescan_db_open(&db, db_path);
  console.AddLog("loaded database %s", db_path);

  iniSize = conescan_db_load_layout(&db, 1, &iniData);
  if(iniSize) {
    console.AddLog("Loading layout 1");
    ImGui::LoadIniSettingsFromMemory(iniData, iniSize);
  }

  pathHistory = (char**)malloc((sizeof(char*) * pathHistoryMax));
  assert(pathHistory);
  memset(pathHistory, 0, sizeof(char*) * pathHistoryMax);
  for(int i = 0; i < pathHistoryMax; i++) {
    pathHistory[i] = (char*)malloc(sizeof(char) * 255);
    assert(pathHistory[i]);
    memset(pathHistory[i], 0, sizeof(char) * 255);
  }
  conescan_db_load_history(&db, "Definition", pathHistoryMax, pathHistory, &historyCount);
  console.RegisterDB(&db);
  console.AddLog("Loaded definition history");
}

void RenderDefinitionInfo()
{
  ImVec4 keyColor(0.9f, 0.9f, 0.9f, 1.0f);
  ImVec4 valueColor(0.5f, 0.5f, 0.5f, 1.0f);
  if (ImGui::TreeNode("Definition Info")) {
    ImGui::Text("ECU ID: ");
    if(definition.ecuid) {
      ImGui::SameLine(); 
      ImGui::TextColored(valueColor, definition.ecuid);
    }

    ImGui::Text("Internal ID: ");
    if(definition.internalidstring) {
      ImGui::SameLine(); 
      ImGui::TextColored(valueColor, definition.internalidstring);
    }

    ImGui::Text("Internal ID Address: ");
    if(definition.internalidaddress) {
      ImGui::SameLine(); 
      ImGui::TextColored(valueColor, "0x%08lX", definition.internalidaddress);
    }

    ImGui::Text("Flash Method: ");
    if(definition.flashmethod) {
      ImGui::SameLine(); 
      ImGui::TextColored(valueColor, definition.flashmethod);
    }

    ImGui::Text("Memory Model: ");
    if(definition.memmodel) {
      ImGui::SameLine(); 
      ImGui::TextColored(valueColor, definition.memmodel);
    }

    ImGui::Text("Checksum Module: ");
    if(definition.memmodel) {
      ImGui::SameLine(); 
      ImGui::TextColored(valueColor, definition.checksummodule);
    }

    ImGui::Text("Year: ");
    if(definition.year) {
      ImGui::SameLine(); 
      ImGui::TextColored(valueColor, definition.year);
    }

    ImGui::Text("Make: ");
    if(definition.make) {
      ImGui::SameLine(); 
      ImGui::TextColored(valueColor, definition.make);
    }

    ImGui::Text("Model: ");
    if(definition.model) {
      ImGui::SameLine(); 
      ImGui::TextColored(valueColor, definition.model);
    }

    ImGui::Text("SubModel: ");
    if(definition.submodel) {
      ImGui::SameLine(); 
      ImGui::TextColored(valueColor, definition.submodel);
    }

    ImGui::Text("Transmission: ");
    if(definition.transmission) {
      ImGui::SameLine(); 
      ImGui::TextColored(valueColor, definition.transmission);
    }

    ImGui::Text("Market: ");
    if(definition.market) {
      ImGui::SameLine(); 
      ImGui::TextColored(valueColor, definition.market);
    }

    ImGui::TreePop();
  }
}

void RenderScalings()
{
  ImVec4 valueColor(0.5f, 0.5f, 0.5f, 1.0f);
  if (ImGui::TreeNode("Scalings")) {
    for(int i = 0; i < definition.numScalings; i++) {
      ImGui::TextColored(valueColor, definition.scalings[i].name);
    }
    ImGui::TreePop();
  }
}

void RenderTables()
{
  if (ImGui::TreeNode("Tables")) {
    
    for(int i = 0; i < definition.numTables; i++) {

      ImGui::Selectable(definition.tables[i].name, &tableSelect[i]);
    }
    ImGui::TreePop();
  }

  // TODO: load into categories
  for(int i = 0; i < definition.numTables; i++) {
    if(tableSelect[i]) {
      ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
      char buf[64];
      sprintf(buf, "Table Editor #%2d", i);
      if (!ImGui::Begin(buf, &tableSelect[i])) {
        // ImGui::End();
      }

      if (ImGui::BeginPopupContextItem())
      {
          if (ImGui::MenuItem("Close "))
              tableSelect[i] = false;
          ImGui::EndPopup();
      }
      ImGui::Text(definition.tables[i].name);
      ImGui::End();
    }
  }
}

void ConeScan::RenderUI(bool* exit_requested)
{
  if(show_console_window)
    console.Draw("Console", &show_console_window);

  if(show_demo_window)
    ImGui::ShowDemoWindow(&show_demo_window);

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      ImGui::MenuItem("Load Definition", NULL, false, false);

      if(metadataFile == NULL) {
        if (ImGui::MenuItem("Open metadata file", NULL)) {
          char* tmp = getFileOpenPath();
          if(tmp) {
            int len = strlen(tmp);
            if(len > 0) {
              metadataFilePath = (char*)malloc(len+1);
              assert(metadataFilePath);
              memset(metadataFilePath, 0 , len+1);
              strncpy(metadataFilePath, tmp, len);
            }
            free(tmp);
            loadMetadataFile();
          } 
        }
      } else {
        if (ImGui::MenuItem("close metadata file", NULL)) {
          closeMetadataFile();
        }
      }
      ImGui::Separator();
      
      ImGui::MenuItem("Definition History", NULL, false, false);

      for(int i = 0; i < historyCount; i++) {
        assert(pathHistory[i]);
        if(ImGui::MenuItem(pathHistory[i], NULL)) {
          if(metadataFile) closeMetadataFile();
          if(metadataFilePath) {
            free(metadataFilePath);
            metadataFilePath = NULL;
          }
          int len = strlen(pathHistory[i]);
          if(len > 0) {
            metadataFilePath = (char*)malloc(len + 1);
            assert(metadataFilePath);
            memset(metadataFilePath, 0, len+1);
            strncpy(metadataFilePath, pathHistory[i], len);
            loadMetadataFile();
          }
        }
      }
      ImGui::Separator();

      if(ImGui::MenuItem("Show ImGui Demo", NULL)) show_demo_window = true;
      if(ImGui::MenuItem("Show Console", NULL)) show_console_window = true;
      if(ImGui::MenuItem("Quit", NULL)) *exit_requested = true;
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
  
  // Side bar showing Definition info
  // Scalings
  // Tables
  ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Definition Info", NULL)) {
    ImGui::End();
    return;
  }

  RenderDefinitionInfo();
  RenderScalings();
  RenderTables();

  ImGui::End();
}

void ConeScan::Cleanup()
{
  printf("starting cleanup\n");
  closeMetadataFile();
  int layoutID = 0;
  if(iniData) {
    // if ini data was loaded, assume we are using layout 1 for now
    layoutID = 1;
    free((char*)iniData);
  }
  iniData = ImGui::SaveIniSettingsToMemory(&iniSize);
  conescan_db_save_layout(&db, layoutID, iniData);

  conescan_db_close(&db);
}