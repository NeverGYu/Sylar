# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/gch/高性能服务器框架

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/gch/高性能服务器框架/build

# Include any dependencies generated for this target.
include src/CMakeFiles/sylar.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include src/CMakeFiles/sylar.dir/compiler_depend.make

# Include the progress variables for this target.
include src/CMakeFiles/sylar.dir/progress.make

# Include the compile flags for this target's objects.
include src/CMakeFiles/sylar.dir/flags.make

src/CMakeFiles/sylar.dir/main.cpp.o: src/CMakeFiles/sylar.dir/flags.make
src/CMakeFiles/sylar.dir/main.cpp.o: ../src/main.cpp
src/CMakeFiles/sylar.dir/main.cpp.o: src/CMakeFiles/sylar.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/gch/高性能服务器框架/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object src/CMakeFiles/sylar.dir/main.cpp.o"
	cd /home/gch/高性能服务器框架/build/src && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT src/CMakeFiles/sylar.dir/main.cpp.o -MF CMakeFiles/sylar.dir/main.cpp.o.d -o CMakeFiles/sylar.dir/main.cpp.o -c /home/gch/高性能服务器框架/src/main.cpp

src/CMakeFiles/sylar.dir/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/sylar.dir/main.cpp.i"
	cd /home/gch/高性能服务器框架/build/src && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/gch/高性能服务器框架/src/main.cpp > CMakeFiles/sylar.dir/main.cpp.i

src/CMakeFiles/sylar.dir/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/sylar.dir/main.cpp.s"
	cd /home/gch/高性能服务器框架/build/src && /usr/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/gch/高性能服务器框架/src/main.cpp -o CMakeFiles/sylar.dir/main.cpp.s

# Object files for target sylar
sylar_OBJECTS = \
"CMakeFiles/sylar.dir/main.cpp.o"

# External object files for target sylar
sylar_EXTERNAL_OBJECTS =

../bin/sylar: src/CMakeFiles/sylar.dir/main.cpp.o
../bin/sylar: src/CMakeFiles/sylar.dir/build.make
../bin/sylar: src/liblsylar.a
../bin/sylar: /usr/local/lib/libyaml-cpp.so.0.8.0
../bin/sylar: src/CMakeFiles/sylar.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/gch/高性能服务器框架/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable ../../bin/sylar"
	cd /home/gch/高性能服务器框架/build/src && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/sylar.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
src/CMakeFiles/sylar.dir/build: ../bin/sylar
.PHONY : src/CMakeFiles/sylar.dir/build

src/CMakeFiles/sylar.dir/clean:
	cd /home/gch/高性能服务器框架/build/src && $(CMAKE_COMMAND) -P CMakeFiles/sylar.dir/cmake_clean.cmake
.PHONY : src/CMakeFiles/sylar.dir/clean

src/CMakeFiles/sylar.dir/depend:
	cd /home/gch/高性能服务器框架/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/gch/高性能服务器框架 /home/gch/高性能服务器框架/src /home/gch/高性能服务器框架/build /home/gch/高性能服务器框架/build/src /home/gch/高性能服务器框架/build/src/CMakeFiles/sylar.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : src/CMakeFiles/sylar.dir/depend

