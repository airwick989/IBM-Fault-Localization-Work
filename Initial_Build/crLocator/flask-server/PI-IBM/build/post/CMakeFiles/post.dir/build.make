# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

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
CMAKE_SOURCE_DIR = "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src"

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build"

# Include any dependencies generated for this target.
include post/CMakeFiles/post.dir/depend.make

# Include the progress variables for this target.
include post/CMakeFiles/post.dir/progress.make

# Include the compile flags for this target's objects.
include post/CMakeFiles/post.dir/flags.make

post/CMakeFiles/post.dir/bputil.o: post/CMakeFiles/post.dir/flags.make
post/CMakeFiles/post.dir/bputil.o: Dpiperf/src/post/bputil.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir="/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_1) "Building C object post/CMakeFiles/post.dir/bputil.o"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/post.dir/bputil.o   -c "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/bputil.c"

post/CMakeFiles/post.dir/bputil.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/post.dir/bputil.i"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/bputil.c" > CMakeFiles/post.dir/bputil.i

post/CMakeFiles/post.dir/bputil.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/post.dir/bputil.s"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/bputil.c" -o CMakeFiles/post.dir/bputil.s

post/CMakeFiles/post.dir/hash.o: post/CMakeFiles/post.dir/flags.make
post/CMakeFiles/post.dir/hash.o: Dpiperf/src/post/hash.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir="/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_2) "Building C object post/CMakeFiles/post.dir/hash.o"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/post.dir/hash.o   -c "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/hash.c"

post/CMakeFiles/post.dir/hash.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/post.dir/hash.i"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/hash.c" > CMakeFiles/post.dir/hash.i

post/CMakeFiles/post.dir/hash.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/post.dir/hash.s"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/hash.c" -o CMakeFiles/post.dir/hash.s

post/CMakeFiles/post.dir/i386dis.o: post/CMakeFiles/post.dir/flags.make
post/CMakeFiles/post.dir/i386dis.o: Dpiperf/src/post/i386dis.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir="/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_3) "Building C object post/CMakeFiles/post.dir/i386dis.o"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/post.dir/i386dis.o   -c "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/i386dis.c"

post/CMakeFiles/post.dir/i386dis.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/post.dir/i386dis.i"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/i386dis.c" > CMakeFiles/post.dir/i386dis.i

post/CMakeFiles/post.dir/i386dis.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/post.dir/i386dis.s"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/i386dis.c" -o CMakeFiles/post.dir/i386dis.s

post/CMakeFiles/post.dir/main.o: post/CMakeFiles/post.dir/flags.make
post/CMakeFiles/post.dir/main.o: Dpiperf/src/post/main.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir="/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_4) "Building C object post/CMakeFiles/post.dir/main.o"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/post.dir/main.o   -c "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/main.c"

post/CMakeFiles/post.dir/main.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/post.dir/main.i"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/main.c" > CMakeFiles/post.dir/main.i

post/CMakeFiles/post.dir/main.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/post.dir/main.s"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/main.c" -o CMakeFiles/post.dir/main.s

post/CMakeFiles/post.dir/tree.o: post/CMakeFiles/post.dir/flags.make
post/CMakeFiles/post.dir/tree.o: Dpiperf/src/post/tree.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir="/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_5) "Building C object post/CMakeFiles/post.dir/tree.o"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/post.dir/tree.o   -c "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/tree.c"

post/CMakeFiles/post.dir/tree.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/post.dir/tree.i"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/tree.c" > CMakeFiles/post.dir/tree.i

post/CMakeFiles/post.dir/tree.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/post.dir/tree.s"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/tree.c" -o CMakeFiles/post.dir/tree.s

post/CMakeFiles/post.dir/tree2.o: post/CMakeFiles/post.dir/flags.make
post/CMakeFiles/post.dir/tree2.o: Dpiperf/src/post/tree2.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir="/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_6) "Building C object post/CMakeFiles/post.dir/tree2.o"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/post.dir/tree2.o   -c "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/tree2.c"

post/CMakeFiles/post.dir/tree2.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/post.dir/tree2.i"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/tree2.c" > CMakeFiles/post.dir/tree2.i

post/CMakeFiles/post.dir/tree2.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/post.dir/tree2.s"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/tree2.c" -o CMakeFiles/post.dir/tree2.s

post/CMakeFiles/post.dir/post.o: post/CMakeFiles/post.dir/flags.make
post/CMakeFiles/post.dir/post.o: Dpiperf/src/post/post.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir="/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_7) "Building C object post/CMakeFiles/post.dir/post.o"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/post.dir/post.o   -c "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/post.c"

post/CMakeFiles/post.dir/post.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/post.dir/post.i"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/post.c" > CMakeFiles/post.dir/post.i

post/CMakeFiles/post.dir/post.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/post.dir/post.s"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && /usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post/post.c" -o CMakeFiles/post.dir/post.s

# Object files for target post
post_OBJECTS = \
"CMakeFiles/post.dir/bputil.o" \
"CMakeFiles/post.dir/hash.o" \
"CMakeFiles/post.dir/i386dis.o" \
"CMakeFiles/post.dir/main.o" \
"CMakeFiles/post.dir/tree.o" \
"CMakeFiles/post.dir/tree2.o" \
"CMakeFiles/post.dir/post.o"

# External object files for target post
post_EXTERNAL_OBJECTS =

Dpiperf/bin/post: post/CMakeFiles/post.dir/bputil.o
Dpiperf/bin/post: post/CMakeFiles/post.dir/hash.o
Dpiperf/bin/post: post/CMakeFiles/post.dir/i386dis.o
Dpiperf/bin/post: post/CMakeFiles/post.dir/main.o
Dpiperf/bin/post: post/CMakeFiles/post.dir/tree.o
Dpiperf/bin/post: post/CMakeFiles/post.dir/tree2.o
Dpiperf/bin/post: post/CMakeFiles/post.dir/post.o
Dpiperf/bin/post: post/CMakeFiles/post.dir/build.make
Dpiperf/bin/post: Dpiperf/lib/libutils.a
Dpiperf/bin/post: Dpiperf/lib/libperfutil.so
Dpiperf/bin/post: Dpiperf/lib/liba2n.so
Dpiperf/bin/post: Dpiperf/lib/libutils.a
Dpiperf/bin/post: post/CMakeFiles/post.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir="/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/CMakeFiles" --progress-num=$(CMAKE_PROGRESS_8) "Linking CXX executable ../Dpiperf/bin/post"
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && $(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/post.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
post/CMakeFiles/post.dir/build: Dpiperf/bin/post

.PHONY : post/CMakeFiles/post.dir/build

post/CMakeFiles/post.dir/clean:
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" && $(CMAKE_COMMAND) -P CMakeFiles/post.dir/cmake_clean.cmake
.PHONY : post/CMakeFiles/post.dir/clean

post/CMakeFiles/post.dir/depend:
	cd "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build" && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src" "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/Dpiperf/src/post" "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build" "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post" "/mnt/d/Desktop/GitHub/IBM-Fault-Localization-Work/Initial Build/Localization/Files/PI-IBM/build/post/CMakeFiles/post.dir/DependInfo.cmake" --color=$(COLOR)
.PHONY : post/CMakeFiles/post.dir/depend

