
#
# Cross Platform Makefile
# Compatible with MSYS2/MINGW, Ubuntu 14.04.1 and Mac OS X
#
# You will need GLFW (http://www.glfw.org):
# Linux:
#   apt-get install libglfw-dev
# Mac OS X:
#   brew install glfw
# MSYS2:
#   pacman -S --noconfirm --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-glfw
#

#CXX = g++
#CXX = clang++

TARGET ?= x86_64

IMGUI_DIR = lib/imgui
TINYXML2_DIR = lib/tinyxml2
NATIVEFILEDIALOG_DIR = lib/nativefiledialog
SQLITE3_DIR = lib/sqlite3
LUA_DIR = lib/lua
RX8_ECU_DUMP_DIR = lib/rx8-ecu-dump
J2534_DIR = $(RX8_ECU_DUMP_DIR)/J2534

SOURCES = src/main.cpp
SOURCES += $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_demo.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp
SOURCES += $(TINYXML2_DIR)/tinyxml2.cpp
SOURCES += $(SQLITE3_DIR)/sqlite3.c
SOURCES += $(LUA_DIR)/
SOURCES += $(LUA_DIR)/lib/lua/lctype.c
SOURCES += $(LUA_DIR)/lib/lua/ltm.c
SOURCES += $(LUA_DIR)/lib/lua/ldo.c
SOURCES += $(LUA_DIR)/lib/lua/lzio.c
SOURCES += $(LUA_DIR)/lib/lua/lbaselib.c
SOURCES += $(LUA_DIR)/lib/lua/lmem.c
SOURCES += $(LUA_DIR)/lib/lua/lgc.c
SOURCES += $(LUA_DIR)/lib/lua/lundump.c
#SOURCES += $(LUA_DIR)/lib/lua/lua.c interpreter can't be bundled
SOURCES += $(LUA_DIR)/lib/lua/lmathlib.c
SOURCES += $(LUA_DIR)/lib/lua/lfunc.c
SOURCES += $(LUA_DIR)/lib/lua/lutf8lib.c
SOURCES += $(LUA_DIR)/lib/lua/liolib.c
SOURCES += $(LUA_DIR)/lib/lua/ldebug.c
SOURCES += $(LUA_DIR)/lib/lua/lapi.c
SOURCES += $(LUA_DIR)/lib/lua/ldump.c
SOURCES += $(LUA_DIR)/lib/lua/lparser.c
SOURCES += $(LUA_DIR)/lib/lua/ldblib.c
SOURCES += $(LUA_DIR)/lib/lua/lauxlib.c
SOURCES += $(LUA_DIR)/lib/lua/lcorolib.c
SOURCES += $(LUA_DIR)/lib/lua/lopcodes.c
SOURCES += $(LUA_DIR)/lib/lua/loslib.c
SOURCES += $(LUA_DIR)/lib/lua/lstate.c
SOURCES += $(LUA_DIR)/lib/lua/lvm.c
SOURCES += $(LUA_DIR)/lib/lua/loadlib.c
SOURCES += $(LUA_DIR)/lib/lua/linit.c
# SOURCES += $(LUA_DIR)/lib/lua/onelua.c # onelua contains all the other files
SOURCES += $(LUA_DIR)/lib/lua/llex.c
SOURCES += $(LUA_DIR)/lib/lua/lstring.c
SOURCES += $(LUA_DIR)/lib/lua/ltable.c
SOURCES += $(LUA_DIR)/lib/lua/lobject.c
SOURCES += $(LUA_DIR)/lib/lua/lstrlib.c
SOURCES += $(LUA_DIR)/lib/lua/ltablib.c
SOURCES += $(LUA_DIR)/lib/lua/lcode.c
SOURCES += $(LUA_DIR)/lib/lua/ltests.c
SOURCES += $(RX8_ECU_DUMP_DIR)/src/util.cpp
SOURCES += $(RX8_ECU_DUMP_DIR)/src/librx8.cpp
SOURCES += $(RX8_ECU_DUMP_DIR)/src/UDS.cpp
SOURCES += $(J2534_DIR)/J2534.cpp

ifeq ($(TARGET), x86_64)

EXE = conescan
SOURCES += $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp

endif

ifeq ($(TARGET), wasm)

CC = emcc
CXX = em++
EMSCRIPTEN_EXTRA_DIR = emscripten
WEB_DIR = $(EMSCRIPTEN_EXTRA_DIR)/web
EXE = $(WEB_DIR)/index.html

SOURCES += $(IMGUI_DIR)/backends/imgui_impl_sdl.cpp $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
EMS += -s USE_SDL=2
# EMS += -s DISABLE_EXCEPTION_CATCHING=1
# EMS += -g3
# EMS += -sASYNCIFY_IGNORE_INDIRECT
LDFLAGS += -lidbfs.js -s ALLOW_MEMORY_GROWTH=1 -s EXIT_RUNTIME=1 -s ASSERTIONS=1 -sASYNCIFY

# Emscripten allows preloading a file or folder to be accessible at runtime.
# The Makefile for this example project suggests embedding the misc/fonts/ folder into our application, it will then be accessible as "/fonts"
# See documentation for more details: https://emscripten.org/docs/porting/files/packaging_files.html
# (Default value is 0. Set to 1 to enable file-system and include the misc/fonts/ folder as part of the build.)
USE_FILE_SYSTEM ?= 1
ifeq ($(USE_FILE_SYSTEM), 0)
LDFLAGS += -s NO_FILESYSTEM=1
CPPFLAGS += -DIMGUI_DISABLE_FILE_FUNCTIONS
endif
ifeq ($(USE_FILE_SYSTEM), 1)
# LDFLAGS += --no-heap-copy --preload-file $(IMGUI_DIR)/misc/fonts@/fonts
LDFLAGS += --preload-file lib/metadata@/metadata
LDFLAGS += --preload-file conescan.db@conescan.db
endif

LDFLAGS += --shell-file $(EMSCRIPTEN_EXTRA_DIR)/shell_minimal.html $(EMS)
endif

OBJS = $(addsuffix .o, $(basename $(notdir $(SOURCES))))
OBJS += lib/imgui_lua_bindings/imgui_lua_bindings.o
UNAME_S := $(shell uname -s)
LINUX_GL_LIBS = -lGL

CXXFLAGS =  -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends
CXXFLAGS += -I$(TINYXML2_DIR)
CXXFLAGS += -I$(NATIVEFILEDIALOG_DIR)/src/include
CXXFLAGS += -I$(SQLITE3_DIR)
CXXFLAGS += -I$(LUA_DIR)
CXXFLAGS += -I$(RX8_ECU_DUMP_DIR)/src
CXXFLAGS += -I$(J2534_DIR)
CXXFLAGS += -g # TODO: remove in release builds
CXXFLAGS += -Wall -Wformat -Iinclude

SOURCES += src/conescan.cpp
SOURCES += src/definition.cpp
SOURCES += src/console.cpp
SOURCES += src/conescan_db.cpp
SOURCES += src/history.cpp
SOURCES += src/layout.cpp
SOURCES += src/uds_request_download.cpp
SOURCES += src/file_open_dialog.cpp
SOURCES += src/definition_parse.cpp

##---------------------------------------------------------------------
## OPENGL ES
##---------------------------------------------------------------------

## This assumes a GL ES library available in the system, e.g. libGLESv2.so
# CXXFLAGS += -DIMGUI_IMPL_OPENGL_ES2
# LINUX_GL_LIBS = -lGLESv2

##---------------------------------------------------------------------
## BUILD FLAGS PER PLATFORM
##---------------------------------------------------------------------
LIBS =
ifeq ($(TARGET), x86_64)

EXEDEPS += $(NATIVEFILEDIALOG_DIR)/build/lib/Release/x64/libnfd.a
EXELIBS = $(NATIVEFILEDIALOG_DIR)/build/lib/Release/x64/libnfd.a

ifeq ($(UNAME_S), Linux) #LINUX
	ECHO_MESSAGE = "Linux"
	LIBS += $(LINUX_GL_LIBS) `pkg-config --static --libs glfw3`
	LIBS += `pkg-config --static --libs glew`

	# for native file dialog
	LIBS += $(LINUX_GL_LIBS) `pkg-config --cflags --libs gtk+-3.0`

	CXXFLAGS += `pkg-config --cflags glfw3`
	CXXFLAGS += -Wl,-R,'$$ORIGIN'
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(UNAME_S), Darwin) #APPLE
	ECHO_MESSAGE = "Mac OS X"
	LIBS += -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
	LIBS += -L/usr/local/lib -L/opt/local/lib -L/opt/homebrew/lib
	LIBS += -lglfw

	CXXFLAGS += -I/usr/local/include -I/opt/local/include -I/opt/homebrew/include
	CFLAGS = $(CXXFLAGS)
endif

ifeq ($(OS), Windows_NT)
	ECHO_MESSAGE = "MinGW"
	LIBS += -lglfw3 -lgdi32 -lopengl32 -limm32

	CXXFLAGS += `pkg-config --cflags glfw3`
	CFLAGS = $(CXXFLAGS)
endif

endif

ifeq ($(TARGET), wasm)

EXEDEPS = $(WEB_DIR)
ECHO_MESSAGE = "WASM"
CXXFLAGS += -DWASM $(EMS)
CFLAGS = $(CXXFLAGS)

endif

##---------------------------------------------------------------------
## BUILD RULES
##---------------------------------------------------------------------

%.o:src/%.cpp
	$(CXX) -std=c++11 $(CXXFLAGS) -c -o $@ $<

%.o:$(SQLITE3_DIR)/%.c
	$(CC) $(CXXFLAGS) -c -o $@ $<

%.o:$(LUA_DIR)/%.c
	$(CC) $(CXXFLAGS) -DLUA_COMPAT_5_3 -DLUA_USE_LINUX -c -o $@ $<

%.o:$(IMGUI_DIR)/%.cpp
	$(CXX) -std=c++11 $(CXXFLAGS) -c -o $@ $<

%.o:$(TINYXML2_DIR)/%.cpp
	$(CXX) -std=c++11 $(CXXFLAGS) -c -o $@ $<

%.o:$(IMGUI_DIR)/backends/%.cpp
	$(CXX) -std=c++11 $(CXXFLAGS) -c -o $@ $<

%.o:$(RX8_ECU_DUMP_DIR)/src/%.cpp
	$(CXX) -std=c++11 $(CXXFLAGS) -c -o $@ $<

%.o:$(J2534_DIR)/%.cpp
	$(CXX) -std=c++11 $(CXXFLAGS) -c -o $@ $<

all: $(EXE)
	@echo Build complete for $(ECHO_MESSAGE)

ifeq ($(TARGET), x86_64)

$(EXE): $(OBJS) $(EXEDEPS)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS) $(EXELIBS)

$(NATIVEFILEDIALOG_DIR)/build/lib/Release/x64/libnfd.a:
	$(MAKE) -C $(NATIVEFILEDIALOG_DIR)/build/gmake_linux config=release_x64

run: $(EXE)
	./$(EXE)

debug: $(EXE)
	gdb ./$(EXE)

endif

ifeq ($(TARGET), wasm)

$(EXE): $(OBJS) $(WEB_DIR) $(EMSCRIPTEN_EXTRA_DIR)/shell_minimal.html
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

$(WEB_DIR):
	mkdir $@

serve: all
	python3 -m http.server -d $(WEB_DIR)

endif


clean:
	rm -f $(EXE) $(OBJS) $(WEB_DIR)/*.js $(WEB_DIR)/*.wasm $(WEB_DIR)/*.wasm.pre $(WEB_DIR)/index.data