add_llvm_loadable_module(OMRChecker OMRChecker.cpp PLUGIN_TOOL clang)

if(LLVM_ENABLE_PLUGINS AND (WIN32 OR CYGWIN))
  target_link_libraries(PrintFunctionNames PRIVATE
    clangAST
    clangBasic
    clangFrontend
    LLVMSupport
    )
endif()
