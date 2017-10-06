# BruteClang

BruteClang is a modified implementation of Clang, based on Clang release_50, and LLVM release_50. BruteClang's primary purpose is to analyze multiple variants of a project simultaneously and report errors, if there are any, and specify in which variant the error was reported.

# Background and Motivation

IBM's open-source [Eclipse OMR](https://github.com/eclipse/omr) project is a collection of components for building language run-times such as compiler, garbage collector, etc. These components are built to work for multiple languages and architectures, for which a variability mechanism was needed to support the different variants (platform, language and optional features). As a starting point, we focused on the compiler componet, and found that the variability mechanism mainly uses static polymorphism ([extensible classes](https://github.com/eclipse/omr/blob/master/doc/compiler/extensible_classes/Extensible_Classes.md)). Eclipse OMR leverages C++ compiler's prioritaztion of include directives (`-I` command line arguments) to specify the inheritence structure. To ensure the extensible classes are implemented correctly, Eclipse OMR has a Clang plugin called OMRChecker. 

OMR has a [linter](https://github.com/eclipse/omr/blob/master/fvtest/compilertest/linter.mk) that checks if the extensible classes in the Compiler component were implemented correctly. The linter analyzes the source files in one platform's configuration before analyzing the files in the configuration of the next platform. This results in diagnostic information of a file shared across different platforms isolated from one another, as the linter needs to cycle through the entire list of files before analyzing the same file with a different configuration. Since most of the files are shared between platforms, a more meaningful analysis would be to analyze all variants of a file across different platforms and group results together. This enables seeing what worked and what did not work in a single run.

To achieve that, Clang's components were modified to execute several compiler instances in a single run, and this version was named BruteClang. As the name indicates, BruteClang implements a brute force mechanism. Hidden from the end user, BruteClang executes itself multiple times on a source file with different `-I` and `-D` arguments, and presents the user with the outcome for all variants.

BruteClang is not just designed to work with OMRChecker. Any Clang plug-in that will benefit from having the analysis output grouped across multiple variants of a source file will benefit from using BruteClang.

Beside the purpose described above, BruteClang will serve the purpose of being the baseline in our effort to build a complete variability-aware analysis tool for C++, where we aim to leverage sharing of common code across multiple variants and prevent repeated analysis of the same code. 

# How BruteClang works
Here is what is needed to use BruteClang:
* A script that calls BruteClang on all files in a file list
* A set of config files for BruteClang to determine what platform it needs to analyze a file, and what the arguments are
* A set of config files for BruteClang to read the `-I`s and `-D`s for different platforms

[___The Script___](https://github.com/nbhuiyan/public-files/blob/master/BruteClang-files/run_BruteClang.py)  
The script works similarly to `linter.mk` located in `/fvtest/compilertest`, executing BruteClang on all files available in a file list. Unlike the linter,  The file containing the list of files to run BruteClang on is set to `all_files.config`. It can be changed by modifying line 15 of the script.

Line 9 specifies the name of the plugin in Clang's plugin registry (omr-checker in this case). Lines 6 and 12 specify where the the clang executables and the plugin's shared library are located.

Perhaps later down the line, `linter.mk` could be modified to run BruteClang instead of running Clang for one platform at a time, removing the need for this script.

[___Config files containing list of files for each platform___](https://github.com/nbhuiyan/public-files/tree/master/file-config)  
These config files contain the list of files that are relevant to each platform. If a file is in [`common_files.config`](https://github.com/nbhuiyan/public-files/blob/master/file-config/common_files.config), then BruteClang will execute that file for all platforms. If a file is present in [`x_files.config`](https://github.com/nbhuiyan/public-files/blob/master/file-config/x_files.config), then the file will be tested for `amd64` and `i386` platforms. If a file is in `amd64_files.config`, then the file will be tested for just `amd64` platform, and so on.

[___Config files containing the set of arguments___](https://github.com/nbhuiyan/public-files/tree/master/arg-config)
The next set of config files deal with the arguments that BruteClang will need to apply to the different platforms: the set of `-D` and `-I` argument that are normally passed to Clang in the command line. The set of arguments required for the i386 platform, for example, are stored in the file [i386.config](https://github.com/nbhuiyan/public-files/blob/master/arg-config/i386.config) Here is an example showing how the argument look like for the i386 platform:
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
-DTR_TARGET_32BIT
-DSUPPORTS_THREAD_LOCAL
-D_LONG_LONG
-DJ9HAMMER
-DDEBUG
-I'../../include_core'
-I'../../fvtest/compilertest/x/i386'
-I'../../fvtest/compilertest/x'
-I'../../fvtest/compilertest'
-I'../../fvtest'
-I'../../compiler/x/i386'
-I'../../compiler/x'
-I'../../compiler'
-I'../..'
-I'../..'
-I'../../third_party/gtest-1.8.0'
-I'../../third_party/gtest-1.8.0/include'
```

It does not matter how you order the `-D` arguments, but it is important to order the `-I` arguments in the order you want BruteClang to search the directories, as the searching order of the directories for the header files depend on the order the `-I` arguments were given to Clang.

___The Execution___

The Python script calls BruteClang on the list of all the files to be analyzed one by one. Suppose the first file the script executes BruteClang with OMRChecker plguin loaded is called `Random.cpp`.
![](https://github.com/nbhuiyan/public-files/blob/master/BruteClang-images/BruteClang%20files%20search%20and%20execution.png)
BruteClang reads informationn off a set of `.config` files in its current implementation. Based on where `Random.cpp` is found, BruteClang decides how many different configurations it will need to execute.

By reading the common set of files, BruteClang sees that Random.cpp exists in that set. Therefore, it will need to be executed for all platforms. So Now BruteClang will need to read the -Is and -Ds for each of those architectures.
![](https://github.com/nbhuiyan/public-files/blob/master/BruteClang-images/BruteClang%20reading%20-I%20and%20-D.png)

Next, BruteClang will copy the execution instance to multiple execution instances, and assign `-I`s and `-D`s of each architecture for them, and then process the outcome of these executions, reporting errors if there are any.
![](https://github.com/nbhuiyan/public-files/blob/master/BruteClang-images/BruteClang%20full%20flowchart.png)

Now that it is done, the script will call BruteClang with OMRChecker plugin loaded on the next file on the list, and the process continues until the last file is done. 

# How to build BruteClang and replicate Eclipse OMR's compiler test (the hard way)
In our experience, this is perhaps the worst way to build LLVM and Clang, although it is supposed to be the [official](https://clang.llvm.org/get_started.html) way.

This method of building takes around 2 hours on 2 desktops we attempted to build it in. The first desktop was powered by a 4 core, 8 thread Intel Core i7-7700, and the second desktop was powered by a 6-core, 12 thread AMD Ryzen 5 1600X. Both desktops had 16 GB of DDR4 RAM, and used SSDs. If you do not have a lot of patience and want to build in a shorter period of time (using the experiemental ninja-build system), do not build BruteClang using the instructions in this section. However, reading through this section will give you a better understanding of how BruteClang works without going too deep into the build parameters we will have to alter to speed up the build process. In the next section we will talk about how to configure Ninja and CMake build parameters to speed up the build process. This section also includes a snippet of the output of executing BruteClang.

We will assume that you are using a 64-bit Linux machine. First, let's set up BruteClang. Make a directory to contain the llvm and clang source code, as well as the build files. Let's called it `BruteClang`. Navigate into llvm-forked.
```plaintext
mkdir BruteClang && cd BruteClang
```
Now, clone llvm from my repository, into a directory called llvm
```plaintext
git clone https://github.com/nbhuiyan/llvm-forked.git llvm
```
Navigate into the subdirectory where clang needs to be placed.
```plaintext
cd llvm/tools
```
Now, clone my forked Clang repository into the directory called clang.
```plaintext
git clone https://github.com/nbhuiyan/clang-forked.git clang
```
Navigate into the clang directory and checkout BruteClang branch.
```plaintext
cd clang
git checkout BruteClang
```
Now that we have all the source files, we can start the build process. Navigate to the BruteClang directory (a level above llvm). In this case, it should be:
```plaintext
cd ../../../
```
Make a directory for the build files and navigate into it.
```plaintext
mkdir build && cd build
```
From within the build directory, we need to setup CMake. For that, ensure you have at least CMake version 3.4.3. If you need to upgrade, get it done from [here](https://cmake.org/download/). Here is the command to execute from within the `build` directory:
```plaintext
cmake -G "Unix Makefiles" ../llvm
```
After build files have been generated, run make. Be prepared to wait a long time.
```
make
```
Once LLVM and Clang has been built, it is time to build OMRChecker. I have already included a modified version of OMRChecker that works with Clang release in `clang/examples/OMRChecker`. Within the `build` directory, run the following command:
```plaintext
make OMRChecker
```
This will create the shared library `OMRChecker.so` and store it in `build/lib`. Now that everything is setup, it is time to checkout the OMR project. Checkout OMR in a directory called `omr-master`, independent of where LLVM and Clang were built.
```plaintext
git clone https://github.com/eclipse/omr.git omr-master
```
Next, navigate to `omr-master/fvtest/compilertest`
```plaintext
cd omr-master/fvtest/compilertest
```
Here, download the set of files I packaged together that are required for executing BruteClang on amd64, i386, p, and z located [here](https://github.com/nbhuiyan/public-files/blob/master/BruteClang-OMR.tar.gz). Using wget corrupts the archive so do not try that. Extract the contents of the archive.

Next, open run_BruteClang.py in your favourite text editor. Currently, the python script is set up with my local environment. You will need to change the variables `clang` in line 6 and `pluginLib` in line 12 with the correct directory address.

When that is done, change the file permission so that you are able to run the script.
```plaintext
chmod u+x run_BruteClang.py
```
Now, finally, you can execute BruteClang.
```plaintext
./run_BruteClang.py
```

Here is a sample output of running BruteClang. The first 2 files, doubleExtension.cpp and functionCallUsingImplicitThis.cpp are just bad test files that would fail for any configuration. We set up the config files in a way that doubleExtension.cpp and  was tested for every platform, and functionCallUsingImplicitThis.cpp was tested on just amd64 and p. As you can see, the common errors across different platforms were grouped together. The remainder of the files visible in this output snippet contains actual files from OMR's compiler component.


```plaintext
nazim@acer:~/omr-master/fvtest/compilertest$ ./run_BruteClang.py
Running on file doubleExtension.cpp:
amd64, i386, p, z:
 In file doubleExtension.cpp: Line 54: error: self call is resolving outside the current extensible class. You probably didn't define self() in all extensible OMR layers.
------------------------------------------------------

Running on file functionCallUsingImplicitThis.cpp:
amd64, p:
 In file functionCallUsingImplicitThis.cpp: Line 56: error: Implicit this receivers are prohibited in extensible classes
------------------------------------------------------

Running on file ../../compiler/compile/OSRData.cpp:
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 155: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 156: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 81: error: cast from pointer to smaller type 'uintptrj_t' (aka 'unsigned int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 202: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
------------------------------------------------------

Running on file ../../compiler/compile/Method.cpp:
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 155: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 156: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 81: error: cast from pointer to smaller type 'uintptrj_t' (aka 'unsigned int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 202: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
------------------------------------------------------

Running on file ../../compiler/compile/VirtualGuard.cpp:
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 155: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 156: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 81: error: cast from pointer to smaller type 'uintptrj_t' (aka 'unsigned int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 202: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/compile/VirtualGuard.cpp: Line 194: error: cast from pointer to smaller type 'uintptrj_t' (aka 'unsigned int') loses information
------------------------------------------------------

Running on file ../../compiler/control/OptimizationPlan.cpp:
No errors reported!
------------------------------------------------------
```

# Speeding up the LLVM and Clang build process using Ninja, shared libraries, and Clang itself

This was a recent discovery we made, so we are not entirely certain about whether this method of building Clang has any disadvantages, other than removing optional Clang tools, tests, and debugging information. There isn't much documentation available online talking about this method, but it works. The benefits so far appear to far outweigh any potential disadvantages. So far we have not come across any issues, and this README file will be updated if we do come across any issues in the future. All of your CPU cores will be utilized in this build process, so you may not be able to run other program if you are building it on your local machine.

We will be copying the contents of the last section and modifying the parts you would need to do differently for this method, so that it is easier to follow along than jumping back and forth between the two sections. Again, we will be assuming that you are running a 64-bit Linux machine.

First, you will need to set up Ninja. [Here](https://ninja-build.org/) is the link to the website that contains some instructions on how to build it, or obtain the binaries/packages for your Linux distro. It is pretty straightforward. I recommend building the latest release from [source](https://github.com/ninja-build/ninja) or downloading the binaries from the website, since apt packages tend to be outdated. Add to path or copy the ninja binary to a system `/bin` directory if you wish to execute it from anywhere without specifying the full path.

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

Make a directory to contain the llvm and clang source code, as well as the build files. Let's called it `BruteClang`. Navigate into llvm-forked.
```plaintext
mkdir BruteClang && cd BruteClang
```
Now, clone llvm from my repository, into a directory called llvm
```plaintext
git clone https://github.com/nbhuiyan/llvm-forked.git llvm
```
Navigate into the subdirectory where clang needs to be placed.
```plaintext
cd llvm/tools
```
Now, clone my forked Clang repository into the directory called clang.
```plaintext
git clone https://github.com/nbhuiyan/clang-forked.git clang
```
Navigate into the clang directory and checkout BruteClang branch.
```plaintext
cd clang
git checkout BruteClang
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
* CMAKE_BUILD_TYPE: Release
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

Now, press `c` to configure and `g` to generate and exit. That's all you have to do. There are perhaps other things you could try and disable to speed things up even further. However, we have not experimented on exploring what other options are okay to disable, as the subsequent builds do not take longer than a couple of seconds (as opposed to 20 to 30 minutes building it in the way outlined in the previous section).

Now we are ready to build BruteClang. So in the command line, enter:
```plaintext
ninja
```

Once LLVM and Clang has been built, it is time to build OMRChecker. I have already included a modified version of OMRChecker that works with Clang release in `clang/examples/OMRChecker`. Within the `build` directory, run the following command:
```plaintext
ninja OMRChecker
```
This will create the shared library `OMRChecker.so` and store it in `build/lib`. Now that everything is setup, it is time to checkout the OMR project. Checkout OMR in a directory called `omr-master`, independent of where LLVM and Clang were built.
```plaintext
git clone https://github.com/eclipse/omr.git omr-master
```
Next, navigate to `omr-master/fvtest/compilertest`
```plaintext
cd omr-master/fvtest/compilertest
```
Here, download the set of files I packaged together that are required for executing BruteClang on amd64, i386, p, and z located [here](https://github.com/nbhuiyan/public-files/blob/master/BruteClang-OMR.tar.gz). Using wget corrupts the archive so do not try that. Extract the contents of the archive.

Next, open run_BruteClang.py in your favourite text editor. Currently, the python script is set up with my local environment. You will need to change the variables `clang` in line 6 and `pluginLib` in line 12 with the correct directory address.

When that is done, change the file permission so that you are able to run the script.
```plaintext
chmod u+x run_BruteClang.py
```
Finally, you can execute BruteClang.

```plaintext
./run_BruteClang.py
```


Here is a sample output of running BruteClang. The first 2 files, doubleExtension.cpp and functionCallUsingImplicitThis.cpp are just bad test files that would fail for any configuration. We set up the config files in a way that doubleExtension.cpp and  was tested for every platform, and functionCallUsingImplicitThis.cpp was tested on just amd64 and p. As you can see, the common errors across different platforms were grouped together. The remainder of the files visible in this output snippet contains actual files from OMR's compiler component.


```plaintext
nazim@acer:~/omr-master/fvtest/compilertest$ ./run_BruteClang.py
Running on file doubleExtension.cpp:
amd64, i386, p, z:
 In file doubleExtension.cpp: Line 54: error: self call is resolving outside the current extensible class. You probably didn't define self() in all extensible OMR layers.
------------------------------------------------------

Running on file functionCallUsingImplicitThis.cpp:
amd64, p:
 In file functionCallUsingImplicitThis.cpp: Line 56: error: Implicit this receivers are prohibited in extensible classes
------------------------------------------------------

Running on file ../../compiler/compile/OSRData.cpp:
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 155: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 156: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 81: error: cast from pointer to smaller type 'uintptrj_t' (aka 'unsigned int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 202: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
------------------------------------------------------

Running on file ../../compiler/compile/Method.cpp:
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 155: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 156: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 81: error: cast from pointer to smaller type 'uintptrj_t' (aka 'unsigned int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 202: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
------------------------------------------------------

Running on file ../../compiler/compile/VirtualGuard.cpp:
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 155: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 156: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 81: error: cast from pointer to smaller type 'uintptrj_t' (aka 'unsigned int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 202: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/compile/VirtualGuard.cpp: Line 194: error: cast from pointer to smaller type 'uintptrj_t' (aka 'unsigned int') loses information
------------------------------------------------------

Running on file ../../compiler/control/OptimizationPlan.cpp:
No errors reported!
------------------------------------------------------
```

# Where do I modify code to configure BruteClang my own way for different platforms?

The way BruteClang is set up now is for some very specific architectures hard coded in the main driver. In the future, when we automate the generating of config files within the build system, we will generate config files for BruteClang to read so that platforms can be specified externally. At this moment, when we have yet to figure out how to automate the generation of config files, hard coding the platform information is the easier approach, reducing the number of config files essential for the execution of BruteClang, which we currently have to handle manually. Once we can automate the generation of config files, it will be much easier to manage them.

The changes you would have to make are within the file `cc1_main.cpp`, located in [`clang/tools/driver`](https://github.com/nbhuiyan/clang-forked/blob/BruteClang/tools/driver/cc1_main.cpp) betweeen lines 289 to 318:

```c++
  std::string fileName = std::string(Argv.back()); //the last argument in the command line is the file name
  llvm::outs() << "Running on file " << fileName << ":\n";
  if (isInFileList("common_files.config", fileName)){
    //execute for all platforms
    ExecuteCI("amd64", Group, DiagContainer, Argv, Argv0, MainAddr);
    ExecuteCI("i386", Group, DiagContainer, Argv, Argv0, MainAddr);
    ExecuteCI("p", Group, DiagContainer, Argv, Argv0, MainAddr);
    ExecuteCI("z", Group, DiagContainer, Argv, Argv0, MainAddr);
  }
  else if(isInFileList("x_files.config", fileName)){
    //execute for just x family
    ExecuteCI("amd64", Group, DiagContainer, Argv, Argv0, MainAddr);
    ExecuteCI("i386", Group, DiagContainer, Argv, Argv0, MainAddr);
  }
  else if(isInFileList("amd64_files.config", fileName)){
    ExecuteCI("amd64", Group, DiagContainer, Argv, Argv0, MainAddr);
  }
  else if(isInFileList("i386_files.config", fileName)){
    ExecuteCI("i386", Group, DiagContainer, Argv, Argv0, MainAddr);
  }
  else if(isInFileList("p_files.config", fileName)){
    ExecuteCI("p", Group, DiagContainer, Argv, Argv0, MainAddr);
  }
  else if(isInFileList("z_files.config", fileName)){
    ExecuteCI("z", Group, DiagContainer, Argv, Argv0, MainAddr);
  }
  else{
    llvm::errs() << "Unknown file. Please ensure the file exists in one of the file lists.\n";
    return 0;
  }
```

The logic above is pretty simple. You can play around with it and build your own BruteClang using the instructions in the previous section.

# Prebuilt BruteClang

Using CPack, we built both .deb and tar.gz packages of BruteClang that works with Ubuntu. A built BruteClang is available in [this](https://github.com/nbhuiyan/BruteClang-binaries) repository. It also contains a built `OMRChecker.so` shared lib in the lib/ directory. You can obtain all of the build files by simply cloning the repository:
```plaintext
git clone https://github.com/nbhuiyan/BruteClang-binaries.git
```
Download the config files and the script, follow the instructions in the previous sections, and you should be able to test BruteClang out without wasting time building it.

# Known issues and future improvements
* In the diagnostic reporting stage, the column number is currently not available. We will add that in a future update.
* Perhaps the python script is not the best way to handle the tests. A makefile could be a better option.
* Currently, BruteClang runs using a single CPU thread at a time. The execution could be sped up a lot more if we could make use of more threads of the host machine.
* As mentioned earlier, BruteClang is not very platform flexible due to the fact that platform information is hard coded in the main driver. In the future, after figuring out how to automate the generating of config files, we will be adding support for reading platform information off config files
