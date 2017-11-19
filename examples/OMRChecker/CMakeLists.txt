add_llvm_loadable_module(OMRChecker OMRChecker.cpp)

if(LLVM_ENABLE_PLUGINS AND (WIN32 OR CYGWIN))
  target_link_libraries(OMRChecker PRIVATE
    clangAST
    clangBasic
    clangFrontend
    LLVMSupport
    )
endif()