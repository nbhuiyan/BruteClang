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

# How to test Eclipse OMR's Compiler Component using BruteClang.

# A sample of BruteClang output

# What should I modify to run my own analysis?

# Known issues
In the diagnostic reporting stage, the column number is currently not available. We will add that in a future update.
