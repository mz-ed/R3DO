CXX      ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -Wshadow -std=c++17
DEPFLAGS  = -MMD -MP
LDFLAGS   = -lX11 -lXext -lpthread

# One -I per source folder: includes stay bare ("v3.hpp") so moving files
# between folders never touches include statements.
INC       = -I. -Isrc -Ianim -Ishapes -Imodes -Iui -Isculpt

SRCS = src/main.cpp src/display.cpp src/render.cpp src/saver.cpp src/settings.cpp \
       anim/animation.cpp \
       shapes/mesh.cpp shapes/obj_loader.cpp \
       sculpt/sculpt_mesh.cpp sculpt/sculpt_engine.cpp sculpt/sculpt_io.cpp sculpt/sculpt_ui.cpp \
       ui/ui.cpp ui/nkui_core.cpp ui/nkui_sidebar.cpp ui/nkui_anim.cpp ui/startscreen.cpp \
       modes/modes.cpp modes/billboard.cpp modes/overhead.cpp modes/static.cpp
OBJS = $(SRCS:.cpp=.o)
DEPS = $(OBJS:.o=.d)
BIN  = R3DO

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INC) $(DEPFLAGS) -c -o $@ $<

# Nuklear's big headers stay behind -w so project warnings stay readable.
ui/nkui_core.o: ui/nkui_core.cpp vendor/nuklear/nuklear.h vendor/nuklear/nuklear_xlib.h
	$(CXX) -O2 -w -std=c++17 $(INC) $(DEPFLAGS) -c -o $@ $<

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS) $(BIN)

.PHONY: all clean
