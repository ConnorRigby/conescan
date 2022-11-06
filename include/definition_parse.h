#pragma once

#include "tinyxml2.h"

#include "history.h"
#include "definition_parse.h"
#include "definition.h"

#include "console.h"

struct DefinitionParse {
	// handle to the XML documetn
	tinyxml2::XMLDocument* metadataFile = NULL;
	
	// file path string
	char* metadataFilePath = NULL;
	
	// console for reporting error messages
	// wish this was elsewhhere but oh well
	ConeScan::Console* console;
};

// load the definition file and populate definition
bool loadMetadataFile(struct DefinitionParse* parse, struct Definition* definition);

// close the definition and free up all memory for it
void closeMetadataFile(struct DefinitionParse* parse, struct Definition* definition);

// coppies path into parse
bool setMetadataFilePath(struct DefinitionParse* parse, char* path);
