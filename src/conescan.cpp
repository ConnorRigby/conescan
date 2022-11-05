
#include <unistd.h>
#include <errno.h>
// #include <GL/glext.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

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

struct ConeScanDB db;

char* romFilePath = NULL;
unsigned char* romFile = NULL;
char* metadataFilePath = NULL;

struct Definition definition;
tinyxml2::XMLDocument* metadataFile = NULL;

static ConeScan::Console console;
bool show_console_window = true;
bool show_demo_window = false;

// stores opened table editors
bool* tableSelect = NULL;

int pathHistoryMax = 5;

int metadataHistoryCount = 0;
char **metadataFilePathHistory;

int romHistoryCount = 0;
char **romFilePathHistory;

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
  // sanity checks
  if(table == NULL) return;
  if(xml == NULL) return;
  if(definition.scalings == NULL) return;

  const char* address_string = xml->Attribute("address");
  table->address = strtol(address_string, NULL, 16);
  table->level = xml->IntAttribute("level");
  table->elements = xml->IntAttribute("elements");
  definition_scaling_add_string_value(&table->name, xml->Attribute("name"));
  definition_scaling_add_string_value(&table->type, xml->Attribute("type"));
  definition_scaling_add_string_value(&table->category, xml->Attribute("category"));
  definition_scaling_add_string_value(&table->scaling, xml->Attribute("scaling"));
  assert(table->scaling);
  for(int i = 0; i < definition.numScalings; i++) {
    if(strcmp(table->scaling, definition.scalings[i].name) == 0) {
      table->Scaling = &definition.scalings[i];
      break;
    }
  }
  if(table->Scaling == NULL) {
    console.AddLog("Could not locate scaling for table %s", table->name);
  }
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
  for(int i = 0; i < metadataHistoryCount; i++) {
    if(strcmp(metadataFilePathHistory[i], metadataFilePath) == 0)
      addToHistory = false;
  }
  if(addToHistory) {
    printf("adding %s to history\n", metadataFilePath);
    conescan_db_add_history(&db, "Definition", metadataFilePath);
    printf("added %s to history\n", metadataFilePath);
    conescan_db_load_history(&db, "Definition", pathHistoryMax, metadataFilePathHistory, &metadataHistoryCount);
    console.AddLog("added entry to history%s", metadataFilePath);
  }  
}

void closeRomFile()
{
  if(romFile) {
    free(romFile);
    romFile = NULL;
  }
  if(romFilePath) {
    free(romFilePath);
    romFile = NULL;
  }
}

void loadRomFile(void)
{
  if(!romFilePath) return;
  if(romFile) {
    free(romFile);
    romFile = NULL;
  }
  bool addToHistory = true;

  FILE* fp = fopen(romFilePath, "rb");
  int length = 0;
  if(!fp) {
    console.AddLog("Failed to open %s for reading", romFilePath);
    free(romFilePath);
    romFilePath = NULL;
    return;
  }

  if (fseek(fp, 0, SEEK_END)) goto io_error;
  length = ftell(fp);
  if(!(length > 0)) goto io_error;
  rewind(fp);

  romFile = (unsigned char*)malloc(length);
  if(!romFile) goto io_error;
  fread(romFile, 1, length, fp);
  console.AddLog("Read %d bytes from %s", length, romFilePath);

  for(int i = 0; i < romHistoryCount; i++) {
    if(strcmp(romFilePathHistory[i], romFilePath) == 0)
      addToHistory = false;
  }
  if(addToHistory) {
    printf("adding %s to history\n", romFilePath);
    conescan_db_add_history(&db, "Rom", romFilePath);
    printf("added %s to history\n", romFilePath);
    conescan_db_load_history(&db, "Rom", pathHistoryMax, romFilePathHistory, &romHistoryCount);
    console.AddLog("added rom entry to history %s", romFilePath);
  }  

  return;

io_error:
  console.AddLog("IO error opening rom %s %s", romFilePath, strerror(errno));
  free(romFilePath);
  romFilePath = NULL;
  return;
}

void ConeScan::Init(void)
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

  metadataFilePathHistory = (char**)malloc((sizeof(char*) * pathHistoryMax));
  assert(metadataFilePathHistory);
  memset(metadataFilePathHistory, 0, sizeof(char*) * pathHistoryMax);
  for(int i = 0; i < pathHistoryMax; i++) {
    metadataFilePathHistory[i] = (char*)malloc(sizeof(char) * PATH_MAX);
    assert(metadataFilePathHistory[i]);
    memset(metadataFilePathHistory[i], 0, sizeof(char) * PATH_MAX);
  }
  conescan_db_load_history(&db, "Definition", pathHistoryMax, metadataFilePathHistory, &metadataHistoryCount);
  console.AddLog("Loaded definition history");

  romFilePathHistory = (char**)malloc((sizeof(char*) * pathHistoryMax));
  assert(romFilePathHistory);
  memset(romFilePathHistory, 0, sizeof(char*) * pathHistoryMax);
  for(int i = 0; i < pathHistoryMax; i++) {
    romFilePathHistory[i] = (char*)malloc(sizeof(char) * PATH_MAX);
    assert(romFilePathHistory[i]);
    memset(romFilePathHistory[i], 0, sizeof(char) * PATH_MAX);
  }

  conescan_db_load_history(&db, "Rom", pathHistoryMax, romFilePathHistory, &romHistoryCount);
  console.AddLog("Loaded definition history");
  
  console.RegisterDB(&db);
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

void Render2DTable(struct Table* table)
{
  assert(romFile);
  assert(table);
  assert(table->numTables == 1);
  // struct Table* x = table;
  struct Table* x = &table->tables[0];
  
  static ImGuiTableFlags tableflags = ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBodyUntilResize;
  // printf("table elements %d %d\n", table->elements, x->elements);
  if(ImGui::BeginTable("2D Table", x->elements+1, tableflags)) {
    unsigned long x_axis_address = x->address;
    unsigned long d_axis_address = table->address;
    float output;
    // X header
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 100.0f);
    for(int xi = 1; xi < x->elements+1; xi++,x_axis_address+=4) {
      char buffer[50] = {0};
      float output;

      *((unsigned char*)(&output) + 3) = romFile[x_axis_address];
      *((unsigned char*)(&output) + 2) = romFile[x_axis_address+1];
      *((unsigned char*)(&output) + 1) = romFile[x_axis_address+2];
      *((unsigned char*)(&output) + 0) = romFile[x_axis_address+3];
      sprintf(buffer, "%0.2F", output);

      ImGui::Text("%0.2F", output);
      ImGui::TableSetupColumn(buffer, ImGuiTableColumnFlags_WidthFixed, 50.0f);

    }
    ImGui::TableHeadersRow();

    ImGui::EndTable();
  }
}

void Render3DTable(struct Table* table) 
{
  assert(romFile);
  assert(table);
  assert(table->numTables == 2);
  struct Table* x = &table->tables[0];
  struct Table* y = &table->tables[1];
  
  // Using those as a base value to create width/height that are factor of the size of our font
  // const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("7.70").x;
  const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

  static ImGuiTableFlags tableflags = ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBodyUntilResize;

  if (ImGui::BeginTable("3D Table",  x->elements+1, tableflags)) {
    unsigned long x_axis_address = x->address;
    unsigned long y_axis_address = y->address;
    unsigned long d_axis_address = table->address;
    float output;
    // X header
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    for(int xi = 1; xi < x->elements+1; xi++,x_axis_address+=4) {
      char buffer[50] = {0};
      float output;

      *((unsigned char*)(&output) + 3) = romFile[x_axis_address];
      *((unsigned char*)(&output) + 2) = romFile[x_axis_address+1];
      *((unsigned char*)(&output) + 1) = romFile[x_axis_address+2];
      *((unsigned char*)(&output) + 0) = romFile[x_axis_address+3];
      sprintf(buffer, x->Scaling->format, output);

      ImGui::Text(buffer);
      ImGui::TableSetupColumn(buffer, ImGuiTableColumnFlags_WidthFixed, 50.0f);

    }
    ImGui::TableHeadersRow();
    
    for(int yi = 1; yi < y->elements+1; yi++) {
      float min_row_height = (float)(int)(TEXT_BASE_HEIGHT * 0.30f);
      ImGui::TableNextRow(ImGuiTableRowFlags_None, min_row_height);

      ImGui::TableSetColumnIndex(0);
      ImU32 row_bg_color = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 0.65f));
      ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, row_bg_color);
      *((unsigned char*)(&output) + 3) = romFile[y_axis_address];
      *((unsigned char*)(&output) + 2) = romFile[y_axis_address+1];
      *((unsigned char*)(&output) + 1) = romFile[y_axis_address+2];
      *((unsigned char*)(&output) + 0) = romFile[y_axis_address+3];
      ImGui::Text("%0.2F", output);
      y_axis_address+=4;

      for(int xi = 1, x_axis_address=x->address; xi < x->elements+1; xi++,x_axis_address+=4,d_axis_address+=4) {
        ImGui::TableSetColumnIndex(xi);
        *((unsigned char*)(&output) + 3) = romFile[y_axis_address];
        *((unsigned char*)(&output) + 2) = romFile[y_axis_address+1];
        *((unsigned char*)(&output) + 1) = romFile[y_axis_address+2];
        *((unsigned char*)(&output) + 0) = romFile[y_axis_address+3];
        ImGui::Text(y->Scaling->format, output);
        // ImGui::Text("%0.2F", romFile[d_axis_address]);
      }
    }

    ImGui::EndTable();
  }
}

void RenderTables()
{
  if (ImGui::TreeNode("Tables")) {
    
    for(int i = 0; i < definition.numTables; i++) {
      if(romFile) {
        ImGui::Selectable(definition.tables[i].name, &tableSelect[i]);
      } else {
        ImGui::TextDisabled(definition.tables[i].name);
      }
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
          if (ImGui::MenuItem("Close"))
              tableSelect[i] = false;
          ImGui::EndPopup();
      }
      ImGui::Text(definition.tables[i].name);
      assert(definition.tables[i].type);
      if(strcmp(definition.tables[i].type, "3D") == 0) {
        Render3DTable(&definition.tables[i]);
      } else if(strcmp(definition.tables[i].type, "2D") == 0) {
        Render2DTable(&definition.tables[i]);
      // } else if(strcmp(definition.tables[i], "1D") == 0) {

      } else {
        ImGui::Text("Unknown table type: %s", definition.tables[i].type);
      }
      ImGui::End();
    }
  }
}

void RenderMenu(bool* exit_requested)
{
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

      for(int i = 0; i < metadataHistoryCount; i++) {
        assert(metadataFilePathHistory[i]);
        if(ImGui::MenuItem(metadataFilePathHistory[i], NULL)) {
          if(metadataFile) closeMetadataFile();
          if(metadataFilePath) {
            free(metadataFilePath);
            metadataFilePath = NULL;
          }
          int len = strlen(metadataFilePathHistory[i]);
          if(len > 0) {
            metadataFilePath = (char*)malloc(len + 1);
            assert(metadataFilePath);
            memset(metadataFilePath, 0, len+1);
            strncpy(metadataFilePath, metadataFilePathHistory[i], len);
            loadMetadataFile();
          }
        }
      }
      ImGui::Separator();
      ImGui::MenuItem("Load ROM", NULL, false, false);

      if(romFilePath == NULL) {
        if (ImGui::MenuItem("Open ROM file", NULL)) {
          char* tmp = getFileOpenPath();
          if(tmp) {
            int len = strlen(tmp);
            if(len > 0) {
              romFilePath = (char*)malloc(len+1);
              assert(romFilePath);
              memset(romFilePath, 0 , len+1);
              strncpy(romFilePath, tmp, len);
            }
            free(tmp);
            loadRomFile();
          } 
        }
      } else {
        if (ImGui::MenuItem("Close ROM file", NULL)) {
          free(romFilePath);
          romFilePath = NULL;
          free(romFile);
          romFile = NULL;
        }
      }

      ImGui::MenuItem("ROM History", NULL, false, false);

      for(int i = 0; i < romHistoryCount; i++) {
        assert(romFilePathHistory[i]);
        if(ImGui::MenuItem(romFilePathHistory[i], NULL)) {
          if(romFilePath) {
            if(romFile) closeRomFile();
            free(romFilePath);
            romFilePath = NULL;
          }
          int len = strlen(romFilePathHistory[i]);
          if(len > 0) {
            romFilePath = (char*)malloc(len + 1);
            assert(romFilePath);
            memset(romFilePath, 0, len+1);
            strncpy(romFilePath, romFilePathHistory[i], len);
            loadRomFile();
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
}

void ConeScan::RenderUI(bool* exit_requested)
{

  if(show_console_window)
    console.Draw("Console", &show_console_window);

  if(show_demo_window)
    ImGui::ShowDemoWindow(&show_demo_window);

  // title menu bar
  RenderMenu(exit_requested);
  
  ImGui::Begin("Definition Info", NULL);
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