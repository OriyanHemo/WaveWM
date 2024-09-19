CXXFLAGS ?= -Wall -g
CXXFLAGS += -std=c++1y
CXXFLAGS += -DGLOG_USE_GLOG_EXPORT
CXXFLAGS += `pkg-config --cflags x11 libglog`
LDFLAGS += `pkg-config --libs x11 libglog`

all: wave_wm

HEADERS = \
    util.hpp \
    wave_wm.hpp
SOURCES = \
    util.cpp \
    wave_wm.cpp \
    main.cpp
OBJECTS = $(SOURCES:.cpp=.o)

wave_wm: $(HEADERS) $(OBJECTS)
	$(CXX) -o $@ $(OBJECTS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f wave_wm $(OBJECTS)

