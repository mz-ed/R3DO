CXX      ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -Wshadow -std=c++17
DEPFLAGS  = -MMD -MP
LDFLAGS   = -lX11 -lXext -lpthread

SRCS = main.cpp display.cpp render.cpp ui.cpp saver.cpp startscreen.cpp \
       settings.cpp mesh.cpp obj_loader.cpp billboard.cpp overhead.cpp
OBJS = $(SRCS:.cpp=.o)
DEPS = $(OBJS:.o=.d)
BIN  = R3DO

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c -o $@ $<

-include $(DEPS)

clean:
	rm -f $(OBJS) $(DEPS) $(BIN)

.PHONY: all clean
