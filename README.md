# BruteClang

BruteClang is a modified implementation of Clang, based on Clang release_50, and LLVM release_50. BruteClang's primary purpose is to analyze multiple variants of a project simultaneously and report errors, if there are any, and specify in which variant the error was reported.

# Background and Motivation

IBM's open-source [Eclipse OMR](https://github.com/eclipse/omr) project is a collection of components for building language run-times such as compiler, garbage collector, etc. These components are built to work for multiple languages and architectures, for which a variability mechanism was needed to support the different variants (platform, language and optional features). As a starting point, we focused on the compiler componet, and found that the variability mechanism uses static polymorphism ([extensible classes](https://github.com/eclipse/omr/blob/master/doc/compiler/extensible_classes/Extensible_Classes.md)) and `#ifdef` directives. Eclipse OMR leverages C++ compiler's prioritaztion of include directives (`-I` command line arguments) to specify the inheritence structure. To ensure the extensible classes are implemented correctly, Eclipse OMR has a Clang plugin called OMRChecker. 

The way the compiler component's variability implementation(the extensible classes) is analyzed is by using its [linter](https://github.com/eclipse/omr/blob/master/fvtest/compilertest/linter.mk), which goes through all the files used in a particular platform and runs them with the OMRChecker plugin loaded. It would then run through the entire list of component files for the next platform, resulting in diagnostic information for a file used across multiple platforms isolated from one another. Since most of the files are shared between platforms, a more meaningful analysis would be to analyze all variants of a file across different platforms and group results together. This enables seeing what worked and what did not work in a single run.

To achieved that, Clang's components were modified to execute several compiler instances in a single run, and this version was named BruteClang.

BruteClang is not just designed to work with OMRChecker. Any Clang plug-in that will benefit from having the analysis output grouped across multiple variants of a source file will benefit from using BruteClang.

Beside the purpose described above, BruteClang will serve the purpose of being the baseline in our effort to build a complete variability-aware analysis tool for C++, where we aim to leverage sharing of common code across multiple variants and prevent repeated analysis of the same code. 

# How BruteClang works
We will use an example of BruteClang running on a file in Eclipse OMR.

A script calls BruteClang on the list of all the files to be analyzed one by one. Suppose the first file the script executes BruteClang with OMRChecker plguin loaded is called `Random.cpp`.
![](https://github.com/nbhuiyan/public-files/blob/master/BruteClang-images/BruteClang%20files%20search%20and%20execution.png)
BruteClang reads informationn off a set of `.config` files in its current implementation. Based on where `Random.cpp` is found, BruteClang decides how many different configurations it will need to execute.

By reading the common set of files, BruteClang sees that Random.cpp exists in that set. Therefore, it will need to be executed for all platforms. So Now BruteClang will need to read the -Is and -Ds for each of those architectures.
![](https://github.com/nbhuiyan/public-files/blob/master/BruteClang-images/BruteClang%20reading%20-I%20and%20-D.png)

Next, BruteClang will copy the execution instance to multiple execution instances, and assign -Is and -Ds of each architecture for them, and then process the outcome of these executions, reporting errors if there are any.
![](https://github.com/nbhuiyan/public-files/blob/master/BruteClang-images/BruteClang%20full%20flowchart.png)

Now that it is done, the script will call BruteClang with OMRChecker plugin loaded on the next file on the list, and the process continues until the last file is done. 

# How to build BruteClang and test Eclipse OMR's Compiler Component
I am going to assume you are using a 64-bit Linux-based machine. This guide is meant to replace the linter in fvtest/compilertest.

First, let's set up BruteClang. Make a directory to contain the llvm and clang source code, as well as the build files. Let's called it `BruteClang`. Navigate into llvm-forked.
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
From within the build directory, we need to setup CMake. For that, ensure you have at least CMake version 3.5. If you need to upgrade, get it done from [here](https://cmake.org/download/). Here is the command to execute from within the `build` directory:
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

And it will look something like this:
```plaintext
Running on file ../../compiler/compile/OSRData.cpp:
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 155: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/control/OptionsUtil.hpp: Line 156: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 81: error: cast from pointer to smaller type 'uintptrj_t' (aka 'unsigned int') loses information
i386:
 In file ../../compiler/infra/HashTab.hpp: Line 202: error: cast from pointer to smaller type 'intptrj_t' (aka 'int') loses information
Running on file ../../compiler/compile/Method.cpp:
```


# Known issues
* In the diagnostic reporting stage, the column number is currently not available. We will add that in a future update.
