# BruteClang

BruteClang is a modified implementation of Clang, based on Clang release_50, and LLVM release_50. BruteClang's primary purpose is to analyze multiple variants of a project simultaneously and report errors, if there are any, and specify in which variant the error was reported.

# Background and Motivation

IBM's open-source [Eclipse OMR](https://github.com/eclipse/omr) project is a collection of components for building language run-times such as compiler, garbage collector, etc. These components are built to work for multiple languages and architectures, for which a variability mechanism was needed to support the different variants (platform, language and optional features). As a starting point, we focused on the compiler componet, and found that the variability mechanism uses static polymorphism ([extensible classes](https://github.com/eclipse/omr/blob/master/doc/compiler/extensible_classes/Extensible_Classes.md)) and `#ifdef` directives. Eclipse OMR leverages C++ compiler's prioritaztion of include directives (`-I` command line arguments) to specify the inheritence structure. To ensure the extensible classes are implemented correctly, Eclipse OMR has a Clang plugin called OMRChecker. 

The way the compiler component's variability implementation is analyzed is by using its [linter](https://github.com/eclipse/omr/blob/master/fvtest/compilertest/linter.mk), which goes through all the files used in a particular platform and ran them with the OMRChecker plugin loaded. It would then run through the entire list of component files for another platform, resulting in diagnostic information for a file used across multiple platforms isolated from one another. Since most of the files are shared between platforms, a more meaningful analysis would be to analyze all variants of a file across different platforms and group results across the variants together. Errors common across platforms would be grouped together.

To achieved that, Clang's components were modified to behave differently, and this version was named BruteClang.

Beside the purpose described above, BruteClang will serve the purpose of being the baseline in our effort to build a complete variability-aware analysis tool for C++, where we aim to leverage sharing of common code across multiple variants and prevent repeated analysis of the same code. 

# How BruteClang works

# How to test Eclipse OMR's Compiler Component using BruteClang.

# Sample test cases

# Known issues
In the diagnostic reporting stage, the column number is currently not available. We will add that in a future update.
