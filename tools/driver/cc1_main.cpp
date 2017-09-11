//===-- cc1_main.cpp - Clang CC1 Compiler Frontend ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is the entry point to the clang -cc1 functionality, which implements the
// core compiler functionality along with a number of additional tools for
// demonstration and testing purposes.
//
//===----------------------------------------------------------------------===//

#include "llvm/Option/Arg.h"
#include "clang/CodeGen/ObjectFilePCHContainerOperations.h"
#include "clang/Config/config.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Basic/BruteClangDiagnostic.h" //access the functionality of BruteClangDiagnostic classes
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/Utils.h"
#include "clang/FrontendTool/Utils.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Option/OptTable.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdio>
#include <fstream>
#include <list>
#include <set>
#include <iterator>
#include <algorithm>

#ifdef CLANG_HAVE_RLIMITS
#include <sys/resource.h>
#endif

using namespace clang;
using namespace llvm::opt;

//===----------------------------------------------------------------------===//
// Main driver
//===----------------------------------------------------------------------===//

static void LLVMErrorHandler(void *UserData, const std::string &Message,
                             bool GenCrashDiag) {
  DiagnosticsEngine &Diags = *static_cast<DiagnosticsEngine*>(UserData);

  Diags.Report(diag::err_fe_error_backend) << Message;

  // Run the interrupt handlers to make sure any special cleanups get done, in
  // particular that we remove files registered with RemoveFileOnSignal.
  llvm::sys::RunInterruptHandlers();

  // We cannot recover from llvm errors.  When reporting a fatal error, exit
  // with status 70 to generate crash diagnostics.  For BSD systems this is
  // defined as an internal software error.  Otherwise, exit with status 1.
  exit(GenCrashDiag ? 70 : 1);
}

#ifdef LINK_POLLY_INTO_TOOLS
namespace polly {
void initializePollyPasses(llvm::PassRegistry &Registry);
}
#endif

#ifdef CLANG_HAVE_RLIMITS
// The amount of stack we think is "sufficient". If less than this much is
// available, we may be unable to reach our template instantiation depth
// limit and other similar limits.
// FIXME: Unify this with the stack we request when spawning a thread to build
// a module.
static const int kSufficientStack = 8 << 20;

#if defined(__linux__) && defined(__PIE__)
static size_t getCurrentStackAllocation() {
  // If we can't compute the current stack usage, allow for 512K of command
  // line arguments and environment.
  size_t Usage = 512 * 1024;
  if (FILE *StatFile = fopen("/proc/self/stat", "r")) {
    // We assume that the stack extends from its current address to the end of
    // the environment space. In reality, there is another string literal (the
    // program name) after the environment, but this is close enough (we only
    // need to be within 100K or so).
    unsigned long StackPtr, EnvEnd;
    // Disable silly GCC -Wformat warning that complains about length
    // modifiers on ignored format specifiers. We want to retain these
    // for documentation purposes even though they have no effect.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#endif
    if (fscanf(StatFile,
               "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %*lu "
               "%*lu %*ld %*ld %*ld %*ld %*ld %*ld %*llu %*lu %*ld %*lu %*lu "
               "%*lu %*lu %lu %*lu %*lu %*lu %*lu %*lu %*llu %*lu %*lu %*d %*d "
               "%*u %*u %*llu %*lu %*ld %*lu %*lu %*lu %*lu %*lu %*lu %lu %*d",
               &StackPtr, &EnvEnd) == 2) {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
      Usage = StackPtr < EnvEnd ? EnvEnd - StackPtr : StackPtr - EnvEnd;
    }
    fclose(StatFile);
  }
  return Usage;
}

#include <alloca.h>

LLVM_ATTRIBUTE_NOINLINE
static void ensureStackAddressSpace(int ExtraChunks = 0) {
  // Linux kernels prior to 4.1 will sometimes locate the heap of a PIE binary
  // relatively close to the stack (they are only guaranteed to be 128MiB
  // apart). This results in crashes if we happen to heap-allocate more than
  // 128MiB before we reach our stack high-water mark.
  //
  // To avoid these crashes, ensure that we have sufficient virtual memory
  // pages allocated before we start running.
  size_t Curr = getCurrentStackAllocation();
  const int kTargetStack = kSufficientStack - 256 * 1024;
  if (Curr < kTargetStack) {
    volatile char *volatile Alloc =
        static_cast<volatile char *>(alloca(kTargetStack - Curr));
    Alloc[0] = 0;
    Alloc[kTargetStack - Curr - 1] = 0;
  }
}
#else
static void ensureStackAddressSpace() {}
#endif

/// Attempt to ensure that we have at least 8MiB of usable stack space.
static void ensureSufficientStack() {
  struct rlimit rlim;
  if (getrlimit(RLIMIT_STACK, &rlim) != 0)
    return;

  // Increase the soft stack limit to our desired level, if necessary and
  // possible.
  if (rlim.rlim_cur != RLIM_INFINITY && rlim.rlim_cur < kSufficientStack) {
    // Try to allocate sufficient stack.
    if (rlim.rlim_max == RLIM_INFINITY || rlim.rlim_max >= kSufficientStack)
      rlim.rlim_cur = kSufficientStack;
    else if (rlim.rlim_cur == rlim.rlim_max)
      return;
    else
      rlim.rlim_cur = rlim.rlim_max;

    if (setrlimit(RLIMIT_STACK, &rlim) != 0 ||
        rlim.rlim_cur != kSufficientStack)
      return;
  }

  // We should now have a stack of size at least kSufficientStack. Ensure
  // that we can actually use that much, if necessary.
  ensureStackAddressSpace();
}
#else
static void ensureSufficientStack() {}
#endif

bool isInFileList(std::string &configFile, std::string &fileName){ 
//this function checks if fileName exists in the configFile
  std::ifstream CFStream;
  CFStream.open(configFile);

  std::set<std::string> FileSet;

  //copy contents of the config file into a set
  std::copy(std::istream_iterator<std::string>(CFStream),
            std::istream_iterator<std::string>(),
            std::inserter(FileSet, FileSet.end()));

  std::set<std::string>iterator it;
  it = FileSet.find(fileName);
  if (it == FileSet.end()){
    return false;
  }
  else{
    return true;
  }
}

void ExecuteCI(std::string &platform, frontend::IncludeDirGroup Group, 
CustomDiagContainer &DiagContainer, ArrayRef<const char *> Argv, const char *Argv0, void *MainAddr){
  current_CI = platform;
  current_CI.pop_back();
  DiagContainer.SetCompilerInstanceName(current_CI);
  std::unique_ptr<CompilerInstance> Clang(new CompilerInstance());
  IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());

  // Next three blocks copied from default implementation. Doesn't work without it
  // Register the support for object-file-wrapped Clang modules.
  auto PCHOps = Clang->getPCHContainerOperations();
  PCHOps->registerWriter(llvm::make_unique<ObjectFilePCHContainerWriter>());
  PCHOps->registerReader(llvm::make_unique<ObjectFilePCHContainerReader>());

  // Buffer diagnostics from argument parsing so that we can output them using a
  // well formed diagnostic object.
  IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
  TextDiagnosticBuffer *DiagsBuffer = new TextDiagnosticBuffer;
  DiagnosticsEngine Diags(DiagID, &*DiagOpts, DiagsBuffer);
  bool Success = CompilerInvocation::CreateFromArgs(
    Clang->getInvocation(), Argv.begin(), Argv.end(), Diags);

  // Infer the builtin include path if unspecified.
  if (Clang->getHeaderSearchOpts().UseBuiltinIncludes &&
      Clang->getHeaderSearchOpts().ResourceDir.empty())
      Clang->getHeaderSearchOpts().ResourceDir =
      CompilerInvocation::GetResourcesPath(Argv0, MainAddr);
  
  while (1){
    config >> str;
    if(str.front() == '-'){
      str.erase(str.begin()); //remove '-'
      if (str.front() == 'I'){ //handle includes
        str.pop_back(); //remove single quote ['] at the back
        str.erase(str.begin(), str.begin()+2); //remove I and single quote [']
        Clang->getHeaderSearchOpts().AddPath(str, Group, false, true);
      }
      else if(str.front() == 'D'){ //handle macrodefs
        str.erase(str.begin()); //remove D
        //invokes new AssignMacroDef function
        CompilerInvocation::AssignMacroDef(Clang->getInvocation() , llvm::StringRef(str));
      }
    }
    if (config.eof()){
      break;
    }
  }

  Clang->createDiagnostics();
  if (!Clang->hasDiagnostics())
    return 1;

  // Set an error handler, so that any LLVM backend diagnostics go through our
  // error handler.
  llvm::install_fatal_error_handler(LLVMErrorHandler,
                                static_cast<void*>(&Clang->getDiagnostics()));

  DiagsBuffer->FlushDiagnostics(Clang->getDiagnostics());

  if (!Success)
    return 1;

  //setting up the diagnostic client to our custom one.
  Clang->getDiagnostics().setClient(new CustomDiagConsumer(DiagContainer), true);

  // Execute the frontend actions.
  Success = ExecuteCompilerInvocation(Clang.get());

  // Our error handler depends on the Diagnostics object, which we're
  // potentially about to delete. Uninstall the handler now so that any
  // later errors use the default handling behavior instead.
  llvm::remove_fatal_error_handler();

}

int cc1_main(ArrayRef<const char *> Argv, const char *Argv0, void *MainAddr) {
  ensureSufficientStack();

  // Initialize targets first, so that --version shows registered targets.
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  std::string inputString; //string buffer to store input from config files
  frontend::IncludeDirGroup Group = frontend::Angled; //for -I command line arguments

  CustomDiagContainer DiagContainer;

  std::string fileName(Argv.end()); //the last argument in the command line is the file name

  if (isInFileList("common_files.config", fileName)){
    //execute for all platforms
    ExecuteCI("amd64", Group, DiagContainer, Argv, Argv0, MainAddr);
    ExecuteCI("i386", Group, DiagContainer, Argv, Argv0, MainAddr);
    ExecuteCI("P", Group, DiagContainer, Argv, Argv0, MainAddr);
    ExecuteCI("Z", Group, DiagContainer, Argv, Argv0, MainAddr);
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
    ExecuteCI("P", Group, DiagContainer, Argv, Argv0, MainAddr);
  }
  else if(isInFileList("z_files.config", fileName)){
    ExecuteCI("P", Group, DiagContainer, Argv, Argv0, MainAddr);
  }
  else{
    llvm::errs() << "Unknown file. Please check the file lists.";
    return 0;
  }


  DiagContainer.PrintDiagnostics();

  return 0;
}
