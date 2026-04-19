# ---- Compiler ----
CXX := clang++

# ---- Output executable ----
TARGET := game_engine_linux

# ---- C++ flags ----
CXXFLAGS := -std=c++17 -O3 -Wall -Wextra -Wno-deprecated-declarations

# ---- Include paths (-I) ----
INCLUDES := -Iinclude -Ithird_party -Ithird_party/box2d/src -Ithird_party/box2d/extern/imgui

# ---- Source files (.cpp) ----
ENGINE_SRCS := \
    src/engine/ComponentDB.cpp \
    src/engine/Engine.cpp \
    src/engine/SceneDB.cpp \
    src/editor/SceneEditor.cpp \
    src/engine/TemplateDB.cpp \
    src/engine/Actor.cpp \
    src/engine/utils/JsonUtils.cpp \
    src/physics/Rigidbody.cpp \
    src/physics/Raycast.cpp \
    src/inputManager/Input.cpp \
    src/engine/Event.cpp \
    src/particle/ParticleSystem.cpp

LUA_SRCS := $(filter-out third_party/lua/lua.c third_party/lua/luac.c,$(wildcard third_party/lua/*.c))

RENDER_SRCS := \
    src/renderer/Renderer.cpp \
    src/renderer/ImageDB.cpp \
    src/renderer/TextDB.cpp \
    src/renderer/AudioDB.cpp

BOX2D_SRCS := $(sort $(shell find third_party/box2d/src -name '*.cpp'))

IMGUI_SRCS := \
    third_party/box2d/extern/imgui/imgui.cpp \
    third_party/box2d/extern/imgui/imgui_draw.cpp \
    third_party/box2d/extern/imgui/imgui_widgets.cpp

SRCS := \
    apps/main.cpp \
    $(ENGINE_SRCS) \
    $(RENDER_SRCS) \
    $(IMGUI_SRCS) \
    $(BOX2D_SRCS)

# ---- SDL2 flags (cross-platform) ----
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
# macOS: use frameworks, but expose them like Linux's /usr/include/SDL2
    SDL_CFLAGS := \
        -Ithird_party/SDL2/SDL2.framework/Headers \
        -Ithird_party/SDL2/SDL2_image.framework/Headers \
        -Ithird_party/SDL2/SDL2_ttf.framework/Headers \
        -Ithird_party/SDL2/SDL2_mixer.framework/Headers \

    SDL_LDFLAGS := \
        -Fthird_party/SDL2 \
        -framework SDL2 \
        -framework SDL2_image \
        -framework SDL2_ttf \
        -framework SDL2_mixer \
        -Wl,-rpath,@executable_path/third_party/SDL2
else
# Linux: rely on system-installed SDL2
    SDL_CFLAGS := $(shell sdl2-config --cflags)

    SDL_LDFLAGS := $(shell sdl2-config --libs) \
        -lSDL2_image \
        -lSDL2_ttf \
        -lSDL2_mixer \
        -llua5.4
endif

# ---- Default rule: `make` ----
all: $(TARGET)

# ---- Link/Build ----
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SDL_CFLAGS) $^ $(SDL_LDFLAGS) -o $@

# ---- Clean ----
clean:
	rm -f $(TARGET)
