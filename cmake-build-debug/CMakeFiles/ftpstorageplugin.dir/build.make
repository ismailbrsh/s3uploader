# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.14

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
CMAKE_COMMAND = /work/clion-2019.1.3/bin/cmake/linux/bin/cmake

# The command to remove a file.
RM = /work/clion-2019.1.3/bin/cmake/linux/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/serg/dev/sample/ftpstorageplugin/src

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/serg/dev/sample/ftpstorageplugin/src/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/ftpstorageplugin.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/ftpstorageplugin.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/ftpstorageplugin.dir/flags.make

CMakeFiles/ftpstorageplugin.dir/s3_library.cpp.o: CMakeFiles/ftpstorageplugin.dir/flags.make
CMakeFiles/ftpstorageplugin.dir/s3_library.cpp.o: ../s3_library.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/serg/dev/sample/ftpstorageplugin/src/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/ftpstorageplugin.dir/s3_library.cpp.o"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/ftpstorageplugin.dir/s3_library.cpp.o -c /home/serg/dev/sample/ftpstorageplugin/src/s3_library.cpp

CMakeFiles/ftpstorageplugin.dir/s3_library.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/ftpstorageplugin.dir/s3_library.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/serg/dev/sample/ftpstorageplugin/src/s3_library.cpp > CMakeFiles/ftpstorageplugin.dir/s3_library.cpp.i

CMakeFiles/ftpstorageplugin.dir/s3_library.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/ftpstorageplugin.dir/s3_library.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/serg/dev/sample/ftpstorageplugin/src/s3_library.cpp -o CMakeFiles/ftpstorageplugin.dir/s3_library.cpp.s

# Object files for target ftpstorageplugin
ftpstorageplugin_OBJECTS = \
"CMakeFiles/ftpstorageplugin.dir/s3_library.cpp.o"

# External object files for target ftpstorageplugin
ftpstorageplugin_EXTERNAL_OBJECTS =

libftpstorageplugin.so: CMakeFiles/ftpstorageplugin.dir/s3_library.cpp.o
libftpstorageplugin.so: CMakeFiles/ftpstorageplugin.dir/build.make
libftpstorageplugin.so: CMakeFiles/ftpstorageplugin.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/serg/dev/sample/ftpstorageplugin/src/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX shared library libftpstorageplugin.so"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/ftpstorageplugin.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/ftpstorageplugin.dir/build: libftpstorageplugin.so

.PHONY : CMakeFiles/ftpstorageplugin.dir/build

CMakeFiles/ftpstorageplugin.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/ftpstorageplugin.dir/cmake_clean.cmake
.PHONY : CMakeFiles/ftpstorageplugin.dir/clean

CMakeFiles/ftpstorageplugin.dir/depend:
	cd /home/serg/dev/sample/ftpstorageplugin/src/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/serg/dev/sample/ftpstorageplugin/src /home/serg/dev/sample/ftpstorageplugin/src /home/serg/dev/sample/ftpstorageplugin/src/cmake-build-debug /home/serg/dev/sample/ftpstorageplugin/src/cmake-build-debug /home/serg/dev/sample/ftpstorageplugin/src/cmake-build-debug/CMakeFiles/ftpstorageplugin.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/ftpstorageplugin.dir/depend

