CXX      ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -Wshadow -std=c++17 -I.
DEPFLAGS  = -MMD -MP
LDFLAGS   = -lX11 -lXext -lpthread

SRCS = sculpt_mesh.cpp sculpt_engine.cpp sculpt_io.cpp sculpt_ui.cpp main.cpp display.cpp render.cpp ui.cpp saver.cpp settings.cpp \
       mesh.cpp obj_loader.cpp billboard.cpp overhead.cpp animation.cpp \
       ui/nkui_core.cpp ui/nkui_sidebar.cpp ui/nkui_anim.cpp ui/startscreen.cpp
OBJS = $(SRCS:.cpp=.o)
DEPS = $(OBJS:.o=.d)
BIN  = R3DO

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

# Nuklear's big headers stay behind -w so project warnings stay readable.
ui/nkui_core.o: ui/nkui_core.cpp vendor/nuklear/nuklear.h vendor/nuklear/nuklear_xlib.h
	$(CXX) -O2 -w -std=c++17 $(DEPFLAGS) -I. -c -o $@ $<

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS) $(BIN) old_ui.o old_ui.d

.PHONY: all clean
