# Makefile for posix and gcc

# Note on old compilers  *cough*  DoC  *cough* you might need -std=c++0x instead
CPPFLAGS = -I include -Wall -std=c++11
LDFLAGS = 
LDLIBS = -lm -ltbb

# Turn on optimisations
CPPFLAGS += -O2

# Include directory
INC_DIR = include/

CPPFLAGS += -I $(INC_DIR)
CLFLAGS += -framework OpenCL

# implementations
HEAT_CORE_OBJS =				src/heat.o
								# src/make_world.o\
								# src/step_world.o\
								# src/render_world.o

HEAT_OBJS = $(HEAT_CORE_OBJS)

all: bin/make_world bin/step_world \
 	bin/render_world \
 	fs1910

bin/make_world: src/make_world.cpp $(HEAT_OBJS)
	-mkdir -p bin
	$(CXX) $(CPPFLAGS) $^ -o $@ #$(LDFLAGS) $(LDLIBS)

bin/step_world: src/step_world.cpp $(HEAT_OBJS)
	-mkdir -p bin
	$(CXX) $(CPPFLAGS) $^ -o $@ #$(LDFLAGS) $(LDLIBS)

bin/render_world: src/render_world.cpp $(HEAT_OBJS)
	-mkdir -p bin
	$(CXX) $(CPPFLAGS) $^ -o $@ #$(LDFLAGS) $(LDLIBS)

# fs1910 namespace

fs1910: bin/fs1910/step_world_v1_lambda \
		bin/fs1910/step_world_v2_function \
		bin/fs1910/step_world_v3_opencl \
		bin/fs1910/step_world_v4_double_buffered \
		bin/fs1910/step_world_v5_packed_properties

bin/fs1910/step_world_v1_lambda: src/fs1910/step_world_v1_lambda.cpp $(HEAT_OBJS)
	-mkdir -p bin
	-mkdir -p bin/fs1910
	$(CXX) $(CPPFLAGS) $^ -o $@

bin/fs1910/step_world_v2_function: src/fs1910/step_world_v2_function.cpp $(HEAT_OBJS)
	-mkdir -p bin
	-mkdir -p bin/fs1910
	$(CXX) $(CPPFLAGS) $^ -o $@

bin/fs1910/step_world_v3_opencl: src/fs1910/step_world_v3_opencl.cpp $(HEAT_OBJS)
	-mkdir -p bin
	-mkdir -p bin/fs1910
	$(CXX) $(CPPFLAGS) $(CLFLAGS) $^ -o $@ -DDEBUG

bin/fs1910/step_world_v4_double_buffered: src/fs1910/step_world_v4_double_buffered.cpp $(HEAT_OBJS)
	-mkdir -p bin
	-mkdir -p bin/fs1910
	$(CXX) $(CPPFLAGS) $(CLFLAGS) $^ -o $@ -DDEBUG

bin/fs1910/step_world_v5_packed_properties: src/fs1910/step_world_v5_packed_properties.cpp $(HEAT_OBJS)
	-mkdir -p bin
	-mkdir -p bin/fs1910
	$(CXX) $(CPPFLAGS) $(CLFLAGS) $^ -o $@ -DDEBUG




clean:
	-rm src/*.o
	-rm -r bin
