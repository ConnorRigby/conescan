#include <stdio.h>

#include "file_open_dialog.h"

#ifdef WASM
#include "emscripten.h"
#else
#include "nfd.h"
#endif

char* getFileOpenPath(char* defaultPath, bool save)
#ifdef WASM
{
    return "/metadata/lfg2ee.xml";
}
#else
{
    nfdchar_t* outPath = NULL;
    nfdresult_t result;
    if (save) {
        result = NFD_PickFolder(NULL, &outPath);
    }
    else {
        result = NFD_OpenDialog(NULL, NULL, &outPath);
    }

    if (result == NFD_OKAY) {
        return outPath;
    }
    else if (result == NFD_CANCEL) {
        fprintf(stderr, "User pressed cancel.");
        return NULL;
    }

    printf("Error: %s\n", NFD_GetError());
    return NULL;
}
#endif
