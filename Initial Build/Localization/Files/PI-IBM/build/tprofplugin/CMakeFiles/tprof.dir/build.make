# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.13

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
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
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/src

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/build

# Include any dependencies generated for this target.
include tprofplugin/CMakeFiles/tprof.dir/depend.make

# Include the progress variables for this target.
include tprofplugin/CMakeFiles/tprof.dir/progress.make

# Include the compile flags for this target's objects.
include tprofplugin/CMakeFiles/tprof.dir/flags.make

tprofplugin/CMakeFiles/tprof.dir/TprofPlugin.o: tprofplugin/CMakeFiles/tprof.dir/flags.make
tprofplugin/CMakeFiles/tprof.dir/TprofPlugin.o: /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/src/tprofplugin/TprofPlugin.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object tprofplugin/CMakeFiles/tprof.dir/TprofPlugin.o"
	cd /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/build/tprofplugin && /usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/tprof.dir/TprofPlugin.o -c /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/src/tprofplugin/TprofPlugin.cpp

tprofplugin/CMakeFiles/tprof.dir/TprofPlugin.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tprof.dir/TprofPlugin.i"
	cd /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/build/tprofplugin && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/src/tprofplugin/TprofPlugin.cpp > CMakeFiles/tprof.dir/TprofPlugin.i

tprofplugin/CMakeFiles/tprof.dir/TprofPlugin.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tprof.dir/TprofPlugin.s"
	cd /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/build/tprofplugin && /usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/src/tprofplugin/TprofPlugin.cpp -o CMakeFiles/tprof.dir/TprofPlugin.s

# Object files for target tprof
tprof_OBJECTS = \
"CMakeFiles/tprof.dir/TprofPlugin.o"

# External object files for target tprof
tprof_EXTERNAL_OBJECTS =

/home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/lib/plugins/libtprof.so: tprofplugin/CMakeFiles/tprof.dir/TprofPlugin.o
/home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/lib/plugins/libtprof.so: tprofplugin/CMakeFiles/tprof.dir/build.make
/home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/lib/plugins/libtprof.so: /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/lib/libutils.a
/home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/lib/plugins/libtprof.so: /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/lib/libperfutil.so
/home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/lib/plugins/libtprof.so: /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/lib/liba2n.so
/home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/lib/plugins/libtprof.so: /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/lib/libutils.a
/home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/lib/plugins/libtprof.so: tprofplugin/CMakeFiles/tprof.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX shared library /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/lib/plugins/libtprof.so"
	cd /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/build/tprofplugin && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/tprof.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
tprofplugin/CMakeFiles/tprof.dir/build: /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/lib/plugins/libtprof.so

.PHONY : tprofplugin/CMakeFiles/tprof.dir/build

tprofplugin/CMakeFiles/tprof.dir/clean:
	cd /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/build/tprofplugin && $(CMAKE_COMMAND) -P CMakeFiles/tprof.dir/cmake_clean.cmake
.PHONY : tprofplugin/CMakeFiles/tprof.dir/clean

tprofplugin/CMakeFiles/tprof.dir/depend:
	cd /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/src /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/Dpiperf/src/tprofplugin /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/build /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/build/tprofplugin /home/nahid/Projects/IBM-1106/Benchmarks/PI-IBM/build/tprofplugin/CMakeFiles/tprof.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : tprofplugin/CMakeFiles/tprof.dir/depend

