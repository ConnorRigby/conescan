#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_memory_editor.h"

#if defined(_WIN32) || defined(WIN32) || defined (_WIN64) || defined (WIN64)
#include <tchar.h>
#include <windows.h>
#include <conio.h>
#include <pathcch.h>
// access
#include <io.h>
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#endif

#include <errno.h>

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include "conescan.h"
#include "conescan_db.h"
#include "history.h"
#include "definition.h"
#include "definition_parse.h"
#include "console.h"
#include "layout.h"
#include "file_open_dialog.h"

//#define OP20PT32_USE_LIB
#include "J2534.h"
#include "librx8.h"
#include "util.h"

#include "uds_request_download.h"

#ifdef __EMSCRIPTEN__
const char* db_path = "/conescan.db";
#else
const char* db_path = "conescan.db";
#endif

// memory editor for a UDS transfer
static MemoryEditor mem_edit;

// memory editor for the rom editor
static MemoryEditor rom_edit;

// console window has logs and other information
// enabled by default, but not active
static ConeScan::Console console;
bool show_console_window = true;

// demo window is for the IMGui window
bool show_demo_window = false;

// sqlite database handle
struct ConeScanDB db;

// for saving/loading layout
// this buffer gets saved into the database
const char* iniData = NULL;
size_t iniSize = 0;

// J2534 interface
static unsigned long devID, chanID;
static const unsigned int CAN_BAUD = 500000;
J2534 j2534;
size_t j2534Initialize();
bool j2534InitOK = false;
char dllName[PATH_MAX] = { 0 };
char apiVersion[255] = { 0 };
char dllVersion[255] = { 0 };
char firmwareVersion[255] = { 0 };

// J2534 -> UDS interface
RX8* ecu;
char* vin;
char* calID;
struct UDSRequestDownload uds_transfer;

// binary file for holding the ROM
// this buffer can be modified with the
// [rom_edit] editor
char* romFilePath = NULL;
unsigned char* romFile = NULL;
long romFileLength = 0;

// ROM layout
struct Definition definition;

// Handles Parsing definitions
struct DefinitionParse definition_parse;

/* Holds bools for each table */
bool* tableSelect = NULL;

/* Holds one bool per cell in every table
 * when the metadata file is opened, we count
 * the number of cells in every table,
 * then allocate one giant block for every
 * cell. */
bool* cellSelect = NULL;
int numCells = 0;

union cellValue {
    float    f32;
    uint32_t u32;
    uint16_t u16;
    uint8_t  u8;
};

enum cellType {
    CELL_F32,
    CELL_U32,
    CELL_U16,
    CELL_U8
};

struct cellState {
    union cellValue value;
    enum  cellType  type;
    bool            selected;
};

/* Holds Every editor value for the editor */
struct cellState* cellValues = NULL;

// All path history buffers will be at max this long
int pathHistoryMax = 5;

// holds up to [pathHistoryMax] strings
char** metadataFilePathHistory = NULL;
int metadataHistoryCount = 0;

// holds up to [pathHistoryMax] strings
char **romFilePathHistory;
int romHistoryCount = 0;

lua_State *L = NULL;

extern const char * RunString(const char* szLua);
extern void LoadImguiBindings();

void addMetadataFileToHistory(char* metadataFilePath)
{
    bool addToHistory = true;
    for (int i = 0; i < metadataHistoryCount; i++) {
        if (strcmp(metadataFilePathHistory[i], metadataFilePath) == 0)
            addToHistory = false;
    }
    if (addToHistory) {
        printf("adding %s to history\n", metadataFilePath);
        conescan_db_add_history(&db, "Definition", metadataFilePath);
        printf("added %s to history\n", metadataFilePath);
        conescan_db_load_history(&db, "Definition", pathHistoryMax, metadataFilePathHistory, &metadataHistoryCount);
        console.AddLog("added entry to history%s", metadataFilePath);
    }
}

void addRomFileToHistory(char* rormFilePath)
{
    bool addToHistory = true;

    for (int i = 0; i < romHistoryCount; i++) {
        if (strcmp(romFilePathHistory[i], romFilePath) == 0)
            addToHistory = false;
    }
    if (addToHistory) {
        printf("adding %s to history\n", romFilePath);
        conescan_db_add_history(&db, "Rom", romFilePath);
        printf("added %s to history\n", romFilePath);
        conescan_db_load_history(&db, "Rom", pathHistoryMax, romFilePathHistory, &romHistoryCount);
        console.AddLog("added rom entry to history %s", romFilePath);
    }
}

void initSelects()
{
    assert(tableSelect == NULL);
    assert(cellSelect == NULL);
    assert(definition.numTables);
    
    tableSelect = (bool*)malloc(sizeof(bool) * definition.numTables);
    assert(tableSelect);
    memset(tableSelect, 0, sizeof(bool) * definition.numTables);
    
    numCells = definition_count_cells(&definition);
    cellSelect = (bool*)malloc(sizeof(bool) * numCells);
    assert(cellSelect);
    memset(cellSelect, 0, sizeof(bool) * numCells);

    cellValues = (struct cellState*)malloc(sizeof(struct cellState) * numCells);
    assert(cellValues);
    memset(cellValues, 0, sizeof(struct cellState) * numCells);
}

void deinitSelects()
{
    if (tableSelect) {
        free(tableSelect);
        tableSelect = NULL;
    }
    if (cellSelect) {
        free(cellSelect);
        cellSelect = NULL;
    }
    if (cellValues) {
        free(cellValues);
        cellValues = NULL;
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
    romFilePath = NULL;
  }
}

bool setRomFilePath(char* path)
{
    assert(path);

    int len = strlen(path);
    if (len > 0) {
        romFilePath = (char*)malloc(len + 1);
        assert(romFilePath);
        memset(romFilePath, 0, len + 1);
        strncpy(romFilePath, path, len);
        return true;
    }
    return false;
}

void loadRomFile(void)
{
  if(!romFilePath) return;
  if(romFile) {
    free(romFile);
    romFile = NULL;
  }

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
  assert(romFile);
  romFileLength = length;
  if(!romFile) goto io_error;
  fread(romFile, 1, length, fp);
  console.AddLog("Read %d bytes from %s", length, romFilePath);
  addRomFileToHistory(romFilePath);
  return;

io_error:
  console.AddLog("IO error opening rom %s %s", romFilePath, strerror(errno));
  free(romFilePath);
  romFilePath = NULL;
  return;
}

void ConeScan::Init(void)
{
  LoadImguiBindings();
  memset(&definition, 0, sizeof(struct Definition));
  memset((void*)&definition_parse, 0, sizeof(struct DefinitionParse));
  definition_parse.console = &console;
  definition_parse.metadataFile = NULL;
  definition_parse.metadataFilePath = NULL;

  memset(&uds_transfer, 0, sizeof(struct UDSRequestDownload));
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
  if (ecu) delete ecu;

  int rc = j2534Initialize();
  if (rc) {
      console.AddLog("ERROR: j2534 init fail");
      j2534InitOK = false;
      mem_edit.Open = false;
  }
  else {
      j2534InitOK = true;
      ecu = new RX8(&j2534, devID, chanID);
  }

  rom_edit.Open = false;
  rom_edit.HighlightColor = ImColor(0x1f1f1ffe);
  rom_edit.OptShowDataPreview = true;
  rom_edit.PreviewDataType = ImGuiDataType_Float;
  rom_edit.PreviewEndianess = 1;

  L = luaL_newstate();
  assert(L);
  luaL_openlibs(L);
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
  char buffer[64] = {0};
  ImVec4 valueColor(0.5f, 0.5f, 0.5f, 1.0f);
  if (ImGui::TreeNode("Scalings")) {
    for(int i = 0; i < definition.numScalings; i++) {
      sprintf(buffer, "%s##%d", definition.scalings[i].name, i);
      ImGui::TextColored(valueColor, buffer);
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

void tableCopyF32(float* value, unsigned char* data, unsigned long address)
{
    assert(value); assert(data);
    *((unsigned char*)(value) + 3) = data[address];
    *((unsigned char*)(value) + 2) = data[address + 1];
    *((unsigned char*)(value) + 1) = data[address + 2];
    *((unsigned char*)(value) + 0) = data[address + 3];
}

void Render3DTable(struct Table* table, struct cellState* cellIndex) 
{
  assert(romFile);
  assert(table);
  assert(table->numTables == 2);
  struct Table* x;
  struct Table* y;
  if(table->swapxy) {
    x = &table->tables[1];
    y = &table->tables[0];
  } else {
    x = &table->tables[0];
    y = &table->tables[1];
  }
  assert(x->elements == 10);
  assert(y->elements == 18);
  char buffer[120] = {0};

  // Using those as a base value to create width/height that are factor of the size of our font
  const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("777.777").x;
  const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

  static ImGuiTableFlags tableflags = ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBodyUntilResize;

  unsigned long x_axis_address = x->address;
  unsigned long y_axis_address = y->address;
  unsigned long base = table->address;
  ImGui::Text("Table Def: data=%04lX X=%04lX Y=%04lX", base, x_axis_address, y_axis_address);

  sprintf(buffer, "[3D] %s ##%04lx", table->name, base);
  if (ImGui::BeginTable(buffer,  x->elements+1, tableflags)) {
    // build X header

    // 0,0
    sprintf(buffer, "##-%s-x0", table->name);
    ImGui::TableSetupColumn(buffer, ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH);
    cellIndex += sizeof(struct cellState);

    // 0,1 - 0,xelements
    for(int xi = 1; xi < x->elements+1; xi++,x_axis_address+=4) {
      tableCopyF32(&cellIndex->value.f32, romFile, x_axis_address);
      assert(x->Scaling);

      // 0,xi
      sprintf(buffer, "%0.2F##3d-x-%d", cellIndex->value.f32, xi);
      ImGui::TableSetupColumn(buffer, ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH);
      cellIndex += sizeof(struct cellState);
    }
    ImGui::TableHeadersRow();

    float min_row_height = (float)(int)(TEXT_BASE_HEIGHT * 0.30f);
    // 1,0 - yelements,xelements
    for(int yi = 1, yo=0; yi < y->elements+1; yi++, yo++) {
      ImGui::TableNextRow(ImGuiTableRowFlags_None, min_row_height);

      // first column in yi
      ImGui::TableSetColumnIndex(0);
      ImU32 row_bg_color = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 0.65f));
      ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, row_bg_color);
      tableCopyF32(&cellIndex->value.f32, romFile, y_axis_address);
      sprintf(buffer, "%0.2F##3d-y-%d", cellIndex->value.f32, yi);
      ImGui::Selectable(buffer, &cellIndex->selected, 0, ImVec2(0.0, 0.0));
      if (cellIndex->selected) {
          rom_edit.GotoAddrAndHighlight(y_axis_address, y_axis_address+ 4);
          cellIndex->selected = false;
          console.AddLog("selected row 0x%04lX-0x%04lX", y_axis_address, y_axis_address+ 4);
      }

      cellIndex += sizeof(struct cellState);
      y_axis_address+=4;

      // for the remaining data cells after the first column
      unsigned long lastBase = base;
      for(int xi = 1; 
              xi < x->elements+1; 
              xi++)
      {
        ImGui::TableSetColumnIndex(xi);
        tableCopyF32(&cellIndex->value.f32, romFile, base);
        sprintf(buffer, "%0.2F##3d-xy-%d%d", cellIndex->value.f32, xi, yi);
        ImU32 cell_bg_color;
        if(cellIndex->value.f32 < 33.33) {
         cell_bg_color = ImGui::GetColorU32(ImVec4(0.0f, 0.8f, 0.0f, 0.65f));
        } else if(cellIndex->value.f32 < 66.66) {
         cell_bg_color = ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.8f, 0.65f));
        } else {
         cell_bg_color = ImGui::GetColorU32(ImVec4(0.8f, 0.0f, 0.0f, 0.65f));
        }
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, cell_bg_color);
        ImGui::Selectable(buffer, &cellIndex->selected, 0, ImVec2(0.0, 0.0));
        if(cellIndex->selected) {
          rom_edit.GotoAddrAndHighlight(base, base+ 4);
          cellIndex->selected = false;
          console.AddLog("selected row 0x%04lX-0x%04lX", base, base+ 4);
        }
        cellIndex += sizeof(struct cellState);
        base+=y->elements*4;
      }
      base = lastBase+4;
    }

    ImGui::EndTable();
  }
}

void RenderTables()
{
  char buffer[120] = {0};
  if (ImGui::TreeNode("Tables")) {
    for(int i = 0; i < definition.numTables; i++) {
      sprintf(buffer, "%s##%d", definition.tables[i].name, i);
      if(romFile) {
        ImGui::Selectable(buffer, &tableSelect[i]);
      } else {
        ImGui::TextDisabled(buffer);
      }
    }
    ImGui::TreePop();
  }

  // TODO: load into categories
  long j = 0;
  for(int i = 0; i < definition.numTables; i++) {
    if(tableSelect[i]) {
      ImGui::SetNextWindowSize(ImVec2(655, 420), ImGuiCond_FirstUseEver);
      sprintf(buffer, "Table Editor %s##%d", definition.tables[i].name, i);
      ImGui::Begin(buffer, &tableSelect[i], ImGuiWindowFlags_MenuBar);

      if(ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Options")) {
          if(ImGui::MenuItem("Enable Memory Editor", NULL, &rom_edit.Open, true));
          ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
      }

      // if (ImGui::BeginPopupContextItem())
      // {
      //     if (ImGui::MenuItem("Close"))
      //         tableSelect[i] = false;
      //     ImGui::EndPopup();
      // }
      sprintf(buffer, "%s", definition.tables[i].name);
      ImGui::Text(buffer);
      assert(definition.tables[i].type);
      if(strcmp(definition.tables[i].type, "3D") == 0) {
        Render3DTable(&definition.tables[i], &cellValues[j]);
        j += definition.tables[i].elements;
        j += definition.tables[i].tables[0].elements;
        j += definition.tables[i].tables[1].elements;
        sprintf(buffer, "Save##%d", i);
        ImGui::Button(buffer);
      } else if(strcmp(definition.tables[i].type, "2D") == 0) {
        Render2DTable(&definition.tables[i]);
        j += definition.tables[i].elements;
        j += definition.tables[i].tables[0].elements;
      } else if(strcmp(definition.tables[i].type, "1D") == 0) {
          j += definition.tables[i].elements;
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

      if(definition.xmlid == NULL) {
        if (ImGui::MenuItem("Open metadata file", NULL)) {
          char* tmp = getFileOpenPath(NULL, false);
          if(tmp) {
            setMetadataFilePath(&definition_parse, tmp);
            free(tmp);
            if (loadMetadataFile(&definition_parse, &definition)) {
                addMetadataFileToHistory(definition_parse.metadataFilePath);
                initSelects();
            }
          } 
        }
      } else {
        if (ImGui::MenuItem("close metadata file", NULL)) {
          closeMetadataFile(&definition_parse, &definition);
          deinitSelects();
        }
      }
      ImGui::Separator();
      
      ImGui::MenuItem("Definition History", NULL, false, false);

      for(int i = 0; i < metadataHistoryCount; i++) {
        assert(metadataFilePathHistory[i]);
        if(ImGui::MenuItem(metadataFilePathHistory[i], NULL)) {
          // close the file if it was open
          closeMetadataFile(&definition_parse, &definition);
          deinitSelects();
          setMetadataFilePath(&definition_parse, metadataFilePathHistory[i]);
          if (loadMetadataFile(&definition_parse, &definition)) {
              initSelects();
          }
        }
      }

      ImGui::Separator();
      ImGui::MenuItem("Load ROM", NULL, false, false);

      if(romFilePath == NULL) {
        if (ImGui::MenuItem("Open ROM file", NULL)) {
            closeRomFile();
            char* tmp = getFileOpenPath(NULL , false);
            if (tmp) {
                if (setRomFilePath(tmp)) {
                    loadRomFile();
                    addRomFileToHistory(romFilePath);
                    loadRomFile();
                }
                free(tmp);
            }
          } 
      } else {
        if (ImGui::MenuItem("Close ROM file", NULL)) {
            closeRomFile();
        }
      }

      ImGui::MenuItem("ROM History", NULL, false, false);

      for(int i = 0; i < romHistoryCount; i++) {
        assert(romFilePathHistory[i]);
        // TODO: this should be a ## unique id
        if(ImGui::MenuItem(romFilePathHistory[i], NULL)) {
            closeRomFile();
            if(setRomFilePath(romFilePathHistory[i]))
                loadRomFile();
        }
      }

      ImGui::Separator();

      if(ImGui::MenuItem("Show ImGui Demo", NULL, &show_demo_window));
      if(ImGui::MenuItem("Show Console", NULL, &show_console_window));

      ImGui::Separator();
      if (ImGui::MenuItem("Delete History", NULL)) {
          conescan_db_purge_history(&db);
          for (int i = 0; i < pathHistoryMax; i++) {
              if (metadataFilePathHistory[i]) {
                  memset(metadataFilePathHistory[i], 0, sizeof(char) * PATH_MAX);
              }
              if (romFilePathHistory[i]) {
                  memset(romFilePathHistory[i], 0, sizeof(char) * PATH_MAX);
              }
          }
          metadataHistoryCount = 0;
          romHistoryCount = 0;
      }
      if(ImGui::MenuItem("Quit", NULL)) *exit_requested = true;
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("ECU")) {
        bool allowSaveRom = false;
        if (uds_transfer.payload && (uds_transfer.downloadinProgress == false)) allowSaveRom = true;

        if (ImGui::MenuItem("Save Rom", NULL, false, allowSaveRom)) {
            //uds_transfer.payload
            assert(vin);
            assert(calID);
            assert(uds_transfer.payload);

            // todo: paths are hard
            char* defaultFileName = (char*)malloc(strnlen(vin, 18) + strnlen(calID, 20) + 7);
            assert(defaultFileName);
            char  fullPath[PATH_MAX] = { 0 };
            FILE* outFile = NULL;

            assert(defaultFileName);
            memset(defaultFileName, 0, strnlen(vin, PATH_MAX) + strnlen(calID, 20) + 7);
            strncpy(defaultFileName, vin, strnlen(vin, 18));
            defaultFileName[strnlen(vin, 18)] = '-';
            strncpy(defaultFileName + strnlen(vin, 18) + 1, calID, strlen(calID));
            strncpy(defaultFileName + strnlen(vin, 18) + 1 + strlen(calID), ".bin", 4);

            char* path = getFileOpenPath(NULL, true);
            if (!path) goto cleanup;

            //int pathLen = strlen(path) + strlen(defaultFileName) + 1;
            strncpy(fullPath, path, strlen(path));
#if defined(_WIN32) || defined(WIN32) || defined (_WIN64) || defined (WIN64)
            fullPath[strlen(path)] = '\\';
#else
            fullPath[strlen(path)] = '/';
#endif
            strncpy(fullPath + strlen(path) + 1, defaultFileName, strlen(defaultFileName));

            int rc;
            console.AddLog("[UDS] Saving transfer to %s (default='%s')", path, defaultFileName);
            outFile = fopen(fullPath, "wb");
            if (!outFile) goto cleanup;
            assert(uds_transfer.payload);
            rc = fwrite(uds_transfer.payload, 1, uds_transfer.transferBytes, outFile);
            if (rc != 1) goto cleanup;
            
            fflush(outFile);
            console.AddLog("[UDS] Transfer saved to %s", path);
        cleanup:
            if (outFile) fclose(outFile);
            if (path) free(path);
            if (defaultFileName) free(defaultFileName);
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
}

void RenderConnection()
{
    ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);
    char buf[255];
    sprintf(buf, "J2534 Interface %s %s", dllName, dllVersion);

    ImGui::Begin(buf, NULL);

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Close")) {
            assert(false); // fixme
        }
        ImGui::EndPopup();
    }
    if (j2534InitOK) {
      if (ImGui::Button("Identify Vehicle")) {
          if (vin) {
              free(vin);
              vin = NULL;
          }
          if (calID) {
              free(calID);
              calID = NULL;
          }

          if (ecu->getVIN(&vin)) {
              console.AddLog("ERROR: Failed to get VIN");
          }
          else {
              console.AddLog("Got VIN: %s", vin);
          }

          if (ecu->getCalibrationID(&calID)) {
              console.AddLog("ERROR: failed to read calibration ID");
          }
          else {
              console.AddLog("Got CALID: %s", calID);
          }
      }
      // don't display these unless the VIN is loaded
      // probably need a better way off handling this
      if (vin) {
        ImGui::SameLine();
        if (ImGui::Button("Download ROM")) {
            uds_transfer.startAddress = 0;
            uds_transfer.transferSize = 0x80000;
            uds_transfer.transferChunkSize = 0x100;
            uds_request_start_download(ecu, &uds_transfer, &console);
            mem_edit.Open = true;
        }
      }
    }
    ImGui::Separator();

    ImGui::Text("VIN: ");

    if (vin) {
        ImGui::SameLine();
        ImGui::Text("%s", vin);
    }

    ImGui::Text("Calibration: ");
    if (calID) {
        ImGui::SameLine();
        ImGui::Text("SW%s", calID);
    }
    ImGui::Separator();

    ImGui::ProgressBar(uds_transfer.transferProgress, ImVec2(0.0f, 0.0f));
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::Text("Transfer Progress %0.2F", uds_transfer.transferProgress);
    if (uds_transfer.payload) {
        if(mem_edit.Open)
            mem_edit.DrawWindow("ECU Memory Editor", uds_transfer.payload, uds_transfer.transferBytes);

        if (mem_edit.Open == false) {
            uds_transfer.downloadinProgress = false;
        }
    }


  ImGui::End();
}

void RenderLiveData()
{
    return;
    ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);
    ImGui::Begin("Live Data Viewer", NULL);
    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Close")) {
            assert(false); // fixme
        }
        ImGui::EndPopup();
    }
    ImGuiTableFlags flags = //ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_SizingFixedSame | 
                            //ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_SizingStretchSame | 
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoHostExtendX;

    if (ImGui::BeginTable("Data", 2, flags)) {
        ImGui::TableNextRow(ImGuiTableRowFlags_None, TEXT_BASE_HEIGHT * 5);
        ImGui::TableNextColumn();

        static float value;
        ImGui::InputFloat("test", &value);
        ImGui::TableNextColumn();
        ImGui::Text("test");
        ImGui::EndTable();
    }
    ImGui::End();
    return;
    if (ImGui::BeginTable("Data", 2, flags)) {
        ImGui::TableNextRow(ImGuiTableRowFlags_None, TEXT_BASE_HEIGHT * 5);
        
        ImGui::TableNextColumn();
        ImGui::Text("RPM");
        ImGui::Separator();
        ImGui::Text("%d RPM", 0);

        ImGui::TableNextColumn();
        ImGui::Text("MAF");
        ImGui::Separator();
        ImGui::Text("%0.F G/s", 0.0);

        ImGui::TableNextRow(ImGuiTableRowFlags_None, TEXT_BASE_HEIGHT * 5);
        ImGui::TableNextColumn();
        ImGui::Text("APP1");
        ImGui::Separator();
        ImGui::Text("%0.2F%%", 0.0);

        ImGui::TableNextColumn();
        ImGui::Text("APP2");
        ImGui::Separator();
        ImGui::Text("%0.2F%%", 0.0);

        ImGui::TableNextRow(ImGuiTableRowFlags_None, TEXT_BASE_HEIGHT * 5);
        ImGui::TableNextColumn();
        ImGui::Text("Lambda");
        ImGui::Separator();
        ImGui::Text("%0.2F:1", 0.0);

        ImGui::TableNextColumn();
        ImGui::Text("CLT");
        ImGui::Separator();
        ImGui::Text("%0.2FC", 0.0);

        ImGui::EndTable();
    }
    ImGui::End();
}

void RenderRomEdit()
{
    if(!romFile) return;
    if (rom_edit.Open)
        rom_edit.DrawWindow("Rom Memory Editor", romFile, romFileLength);

    if (rom_edit.Open == false) {
        uds_transfer.downloadinProgress = false;
    }
}

void ConeScan::RenderUI(bool* exit_requested)
{
  if(show_console_window)
    console.Draw("Console", &show_console_window);

  if(show_demo_window)
    ImGui::ShowDemoWindow(&show_demo_window);

  RunString("ret = imgui.RadioButton(\"String goes here\", isActive)");

  // title menu bar
  RenderMenu(exit_requested);
  
  ImGui::Begin("Definition Info", NULL);
  RenderDefinitionInfo();
  RenderScalings();
  RenderTables();
  ImGui::End();

  RenderConnection();
  RenderLiveData();
  RenderRomEdit();
}

void ConeScan::Cleanup()
{
  uds_request_complete(&uds_transfer);
  closeMetadataFile(&definition_parse, &definition);
  deinitSelects();
  int layoutID = 0;
  if(iniData) {
    // if ini data was loaded, assume we are using layout 1 for now
    layoutID = 1;
    free((char*)iniData);
  }
  iniData = ImGui::SaveIniSettingsToMemory(&iniSize);
  conescan_db_save_layout(&db, layoutID, iniData);

  conescan_db_close(&db);
  if (j2534InitOK) {
    j2534.PassThruClose(devID);
    j2534.PassThruDisconnect(chanID);
  }
}

size_t j2534Initialize()
{
    console.AddLog("initializing J2534\n");
#ifdef __EMSCRIPTEN__
    console.AddLog("J2534 Does not work in the browser currently");
    return 1;
#endif
    j2534.getDLLName(dllName);
#ifdef DEBUG
    j2534.debug(true);
#endif
    if (!j2534.init()) {
        console.AddLog("failed to connect to J2534 DLL.");
        return 1;
    }

    if (j2534.PassThruOpen(NULL, &devID)) {
        console.AddLog("failed to PassThruOpen()");
        return 1;
    }

    if (j2534.PassThruConnect(devID, ISO15765, CAN_ID_BOTH, CAN_BAUD, &chanID)) {
        reportJ2534Error(j2534);
        return 1;
    }

    j2534.PassThruReadVersion(apiVersion, dllVersion, firmwareVersion, devID);
    console.AddLog("Connected to J2534 Interface: %s\n API Version: %s\n DLL Version: %s\n Firmware Version: %s", dllName, apiVersion, dllVersion, firmwareVersion);

    j2534.PassThruIoctl(chanID, CLEAR_MSG_FILTERS, NULL, NULL);

    unsigned long filterID = 0;

    PASSTHRU_MSG maskMSG = { 0 };
    PASSTHRU_MSG maskPattern = { 0 };
    PASSTHRU_MSG flowControlMsg = { 0 };
    for (uint8_t i = 0; i < 7; i++) {
        maskMSG.ProtocolID = ISO15765;
        maskMSG.TxFlags = ISO15765_FRAME_PAD;
        maskMSG.Data[0] = 0x00;
        maskMSG.Data[1] = 0x00;
        maskMSG.Data[2] = 0x07;
        maskMSG.Data[3] = 0xff;
        maskMSG.DataSize = 4;

        maskPattern.ProtocolID = ISO15765;
        maskPattern.TxFlags = ISO15765_FRAME_PAD;
        maskPattern.Data[0] = 0x00;
        maskPattern.Data[1] = 0x00;
        maskPattern.Data[2] = 0x07;
        maskPattern.Data[3] = (0xE8 + i);
        maskPattern.DataSize = 4;

        flowControlMsg.ProtocolID = ISO15765;
        flowControlMsg.TxFlags = ISO15765_FRAME_PAD;
        flowControlMsg.Data[0] = 0x00;
        flowControlMsg.Data[1] = 0x00;
        flowControlMsg.Data[2] = 0x07;
        flowControlMsg.Data[3] = (0xE0 + i);
        flowControlMsg.DataSize = 4;

        if (j2534.PassThruStartMsgFilter(chanID, FLOW_CONTROL_FILTER, &maskMSG, &maskPattern, &flowControlMsg, &filterID))
        {
            reportJ2534Error(j2534);
            return 1;

        }
    }
    maskMSG.ProtocolID = ISO15765;
    maskMSG.TxFlags = ISO15765_FRAME_PAD;
    maskMSG.Data[0] = 0x00;
    maskMSG.Data[1] = 0x00;
    maskMSG.Data[2] = 0x07;
    maskMSG.Data[3] = 0xf8;
    maskMSG.DataSize = 4;

    maskPattern.ProtocolID = ISO15765;
    maskPattern.TxFlags = ISO15765_FRAME_PAD;
    maskPattern.Data[0] = 0x00;
    maskPattern.Data[1] = 0x00;
    maskPattern.Data[2] = 0x07;
    maskPattern.Data[3] = 0xE8;
    maskPattern.DataSize = 4;

    if (j2534.PassThruStartMsgFilter(chanID, PASS_FILTER, &maskMSG, &maskPattern, NULL, &filterID)) {
        console.AddLog("Failed to set message filter");
        reportJ2534Error(j2534);
        return 1;
    }

    if (j2534.PassThruIoctl(chanID, CLEAR_TX_BUFFER, NULL, NULL)) {
        console.AddLog("Failed to clear j2534 TX buffer");
        reportJ2534Error(j2534);
        return 1;
    }

    if (j2534.PassThruIoctl(chanID, CLEAR_RX_BUFFER, NULL, NULL)) {
        console.AddLog("Failed to clear j2534 RX buffer");
        reportJ2534Error(j2534);
        return 1;
    }
    return 0;
}