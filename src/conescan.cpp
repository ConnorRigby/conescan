
#include <unistd.h>

#include "imgui.h"
#include "tinyxml2.h"
#include "nfd.h"

#include "conescan.h"
#include "definition.h"

char* metadataFilePath = NULL;
tinyxml2::XMLDocument* metadataFile = NULL;

char* getFileOpenPath()
{
  nfdchar_t *outPath = NULL;
  nfdchar_t * cwd = NULL;
  getcwd(cwd,255);
  nfdresult_t result = NFD_OpenDialog(NULL, cwd, &outPath);
      
  if (result == NFD_OKAY) {
    return outPath;
  } else if (result == NFD_CANCEL) {
    fprintf(stderr, "User pressed cancel.");
    return NULL;
  }

  printf("Error: %s\n", NFD_GetError() );
  return NULL;
}

void loadMetadataFile() 
{
  if(metadataFilePath == NULL) {
    if(metadataFile) delete metadataFile;
    return;
  }

  if(metadataFile == NULL) {
    metadataFile = new tinyxml2::XMLDocument();
  }
  if(metadataFile->LoadFile(metadataFilePath) != tinyxml2::XML_SUCCESS) {
    fprintf(stderr, "Failed to open metadata file %s\n", metadataFile->ErrorStr());
    return;
  }
  tinyxml2::XMLElement *element;
  element = metadataFile->RootElement();
  tinyxml2::XMLNode *node;
  tinyxml2::XMLNode *rom;
  tinyxml2::XMLNode *romID;
  struct Definition definition;
  memset(&definition, 0, sizeof(struct Definition));

  if(strcmp(element->Name(), "roms") == 0) {
    rom = element->FirstChildElement("rom");
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
    fprintf(stderr, "xmlid=%s\n", definition.xmlid);
    fprintf(stderr, "internalidaddress=%08lx\n", definition.internalidaddress);
  
  }
  
  // metadataFile->RootElement()->Attribute("roms", value);

  // fprintf(stderr, "failed to open metadata file unknown error\n");
}

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
}

void ConeScan::Init()
{
}

void ConeScan::RenderUI(bool* exit_requested)
{
  if (ImGui::BeginMainMenuBar())
      {
        if (ImGui::BeginMenu("File")) {
          if(metadataFile == NULL) {
            if (ImGui::MenuItem("Open metadata file", NULL)) {
              metadataFilePath = getFileOpenPath();
              if(metadataFilePath) {loadMetadataFile();}
            }
          } else {
            if (ImGui::MenuItem("close metadata file", NULL)) {
              closeMetadataFile();
            }
          }

          if (ImGui::MenuItem("Quit", NULL)) {*exit_requested = true;}
          ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
      }

}


void ConeScan::Cleanup()
{
  closeMetadataFile();
}