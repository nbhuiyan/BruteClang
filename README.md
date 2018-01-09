# BruteClang

BruteClang is a modified implementation of Clang, based on Clang release_38, and LLVM release_38. BruteClang's primary purpose is to analyze multiple build variants in a single run and report errors, if there are any, and specify in which variant the error was reported.

# Background and Motivation

IBM's open-source [Eclipse OMR](https://github.com/eclipse/omr) project is a collection of components for building language runtimes such as compiler, garbage collector, etc. These components are built to work for multiple languages and architectures, for which a variability mechanism was needed to support the different variants (platform, language, optional features, etc). As a starting point, we focused on the compiler component, and found that the variability mechanism mainly used static polymorphism ([extensible classes](https://github.com/eclipse/omr/blob/master/doc/compiler/extensible_classes/Extensible_Classes.md)). Eclipse OMR leverages C++ compiler's prioritization of include directives (`-I` command line arguments) to specify the inheritance structure. To ensure the extensible classes are implemented correctly, Eclipse OMR has a Clang plugin called OMRChecker. 

Eclipse OMR has a [linter](https://github.com/eclipse/omr/blob/master/fvtest/compilertest/linter.mk) that checks whether extensible classes were implemented correctly using Clang and the OMRChecker plugin. The linter analyzes the files in one configuration at a time. This results in diagnostic information of a source file shared across different configurations isolated from one another, as the linter needs to cycle through the entire set of files before analyzing the same file with a different configuration. Since most of the files are shared between platforms, a more meaningful analysis would be to analyze source files shared across different configurations and group results together.

To achieve that, Clang's components were modified to execute several compiler instances in a single run, and this version was named BruteClang. As the name indicates, BruteClang implements a brute force mechanism. Hidden from the end user, BruteClang executes itself multiple times on a source file with different `-I` and `-D` arguments, and presents the user with the outcome for all variants.

BruteClang is not just designed to work with OMRChecker. Any Clang plug-in that will benefit from having the analysis output grouped across multiple variants of a source file will benefit from using BruteClang.

Beside the purpose described above, BruteClang will serve the purpose of being the baseline in our effort to build a complete variability-aware analysis tool for C++, where we aim to leverage sharing of common code across multiple variants and prevent repeated analysis of the same code. 

# How BruteClang works
Here is what is needed to use BruteClang:
* A python script that calls BruteClang on all the files in the project
* A file named `bruteclang-configs.config` containing the names of the different configurations. An example:
```plaintext
testcompiler_amd64
testcompiler_z
jitbuilder_p
jitbuilder_z
```
* For each of the configurations above, we need a file containing set of `-D`s and `-I`s for that platform. They would need to be named as `<configuration_name>.config`. For example, the filename of the config file containig the set of arguments for testcompiler_amd64 configuration would be `testcompiler_amd64.config`, and here is what it would look like:
```plaintext
-DBITVECTOR_BIT_NUMBERING_MSB
-DUT_DIRECT_TRACE_REGISTRATION
-DJITTEST
-DTEST_PROJECT_SPECIFIC
-DTR_HOST_X86
-DTR_HOST_64BIT
-DBITVECTOR_64BIT
-DLINUX
-DTR_TARGET_X86
-DTR_TARGET_64BIT
-DSUPPORTS_THREAD_LOCAL
-D_LONG_LONG
-DJ9HAMMER
-DDEBUG
-I'../../include_core'
-I'../../fvtest/compilertest/x/amd64'
-I'../../fvtest/compilertest/x'
-I'../../fvtest/compilertest'
-I'../../fvtest'
-I'../../compiler/x/amd64'
-I'../../compiler/x'
-I'../../compiler'
-I'../..'
-I'../..'
-I'../../third_party/gtest-1.8.0'
-I'../../third_party/gtest-1.8.0/include'
``` 
* For each of the configurations above, we need a file containing the list of files that are part of the configuration, named as `<configuration_name>_files.config`. This allows BruteClang to know the configurations that a source file would need to be analyzed for.

# How to build BruteClang

First, you will need to set up Ninja, if you do not already have it.

Next, you will need to ensure you have CMake version 3.4.3+. If you do not have it already, obtain the latest release from [here](https://cmake.org/download/)

Another application you will need is the cmake-curses-gui, unless you really enjoy messing with CMake files manually. If you are running Ubuntu, the easiest way to obtain it would be using
```plaintext
sudo apt-get install cmake-curses-gui
```

Finally, we will be using clang and clang++ compilers to build BruteClang. If you are running Ubuntu, you should be able to obtain clang-3.8 from simply typing
```plaintext
sudo apt-get install clang
```

Now that everything is set, we can finally get to the real work.

Make a directory to contain the llvm and clang source code, as well as the build files. Let's called it `BruteClang`. Navigate into BruteClang.
```plaintext
mkdir BruteClang && cd BruteClang
```
Now, clone my forked repository llvm, into a directory called llvm. By default, you will have release_38 checked out as it is the default branch.
```plaintext
git clone https://github.com/nbhuiyan/llvm.git llvm
```
Navigate into the subdirectory where clang needs to be placed.
```plaintext
cd llvm/tools
```
Now, clone this repository into the directory called clang.
```plaintext
git clone https://github.com/nbhuiyan/BruteClang.git clang
```
Now that we have all the source files, we can start the build process. Navigate to the BruteClang directory (a level above llvm). In this case, it should be:
```plaintext
cd ../../../
```
Make a directory for the build files and navigate into it.
```plaintext
mkdir build && cd build
```
From within the build directory, we need to setup CMake build files for Ninja. Here is the command to execute from within the `build` directory:
```plaintext
cmake -G Ninja ../llvm
```
After build files have been generated, type in the following command:
```plaintext
ccmake ../llvm
```
A graphical interface will pop up with the build options. Press `t` to view the advanced mode, giving you access to more options. However, upon changing the compiler to clang and clang++, CMakeLists.txt will be deleted and a new one will be generated, resetting every CMake option to its default values, except for the new compilers you just set up. So the first time you open up the cmake-curses-gui, you should change the following options:

* CMAKE_ASM_COMPILER: /usr/bin/clang
* CMAKE_CXX_COMPILER: /usr/bin/clang++
* CMAKE_C_COMPILER: /usr/bin/clang

Press c to configure, and g to generate build files and exit. Now, open up the cmake-curses-gui again using the same command above, and press `t` again to view the advanced mode. Then, ensure all of the options listed below are set in the following way:

* BUILD_SHARED_LIBS: ON
* CLANG_INCLUDE_TESTS: OFF
* CLANG_TOOL_CLANG_ARCMNT_TEST_BUILD: OFF
* CLANG_TOOL_CLANG_CHECK_BUILD: OFF
* CLANG_TOOL_CLANG_FORMAT_BUILD: OFF
* CLANG_TOOL_CLANG_FORMAT_VS_BUI: OFF
* CLANG_TOOL_CLANG_FUZZER_BUILD: OFF
* CLANG_TOOL_CLANG_IMPORT_TEST_B: OFF
* CLANG_TOOL_CLANG_OFFLOAD_BUNDL: OFF
* CLANG_TOOL_CLANG_RENAME_BUILD: OFF
* CLANG_TOOL_CLANG_ARCMT_TEST_BUILD: OFF
* CLANG_TOOL_C_ARCMT_TEST_TEST_BUILD: OFF
* CLANG_TOOL_C_INDEX_TEST_BUILD: OFF
* CMAKE_ASM_COMPILER: /usr/bin/clang
* CMAKE_BUILD_TYPE: Release
* CMAKE_CXX_COMPILER: /usr/bin/clang++
* CMAKE_C_COMPILER: /usr/bin/clang

Now, press `c` to configure and `g` to generate and exit ccmake.

Now we are ready to build BruteClang. So in the command line, enter:
```plaintext
ninja
```

After completing the build, it's time to build OMRChecker plugin, and run BruteClang.

# Building OMRChecker and Running BruteClang

___to be added soon once I fixed a bug I encountered...___
