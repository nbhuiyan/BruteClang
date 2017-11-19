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
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Basic/BruteClangDiagnostic.h" //access the functionality of BruteClangDiagnostic classes
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/Utils.h"
#include "clang/FrontendTool/Utils.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Option/OptTable.h"
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

bool isInFileList(std::string configFile, std::string &fileName){ 
  //this function checks if fileName exists in the configFile
  std::ifstream CFStream;
  CFStream.open(configFile);

  std::set<std::string> FileSet;

  //copy contents of the config file into a set
  std::copy(std::istream_iterator<std::string>(CFStream),
            std::istream_iterator<std::string>(),
            std::inserter(FileSet, FileSet.end()));

  std::set<std::string>::iterator it;
  it = FileSet.find(fileName);
  if (it == FileSet.end()){
    return false;
  }
  else{
    return true;
  }
}

void ExecuteCI(std::string platform, frontend::IncludeDirGroup Group,
   CustomDiagContainer &DiagContainer, ArrayRef<const char *> Argv, 
   const char *Argv0, void *MainAddr){
  std::string current_CI;
  current_CI = platform;
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
  

  Clang->createDiagnostics();
  
  std::string input_string;
  std::ifstream config;
  current_CI.append(".config");
  config.open(current_CI);

  while (1){
    config >> input_string;
    if(input_string.front() == '-'){
      input_string.erase(input_string.begin()); //remove '-'
      if (input_string.front() == 'I'){ //handle includes
        input_string.pop_back(); //remove single quote ['] at the back
        input_string.erase(input_string.begin(), input_string.begin()+2); //remove I and single quote [']
        Clang->getHeaderSearchOpts().AddPath(input_string, Group, false, true);
      }
      else if(input_string.front() == 'D'){ //handle macrodefs
        input_string.erase(input_string.begin()); //remove D
        //invokes new AssignMacroDef function
        CompilerInvocation::AssignMacroDef(Clang->getInvocation() , llvm::StringRef(input_string));
      }
    }
    if (config.eof()){
      break;
    }
  }
  // Set an error handler, so that any LLVM backend diagnostics go through our
  // error handler.
  llvm::install_fatal_error_handler(LLVMErrorHandler,
                                static_cast<void*>(&Clang->getDiagnostics()));

  DiagsBuffer->FlushDiagnostics(Clang->getDiagnostics());

  //setting up the diagnostic client to our custom one.
  Clang->getDiagnostics().setClient(new CustomDiagConsumer(DiagContainer), true);

  //setting error limit to unlimited (0)
  Clang->getDiagnostics().setErrorLimit(0);

  // Execute the frontend actions.
  Success = ExecuteCompilerInvocation(Clang.get());

  // Our error handler depends on the Diagnostics object, which we're
  // potentially about to delete. Uninstall the handler now so that any
  // later errors use the default handling behavior instead.
  llvm::remove_fatal_error_handler();
}

int cc1_main(ArrayRef<const char *> Argv, const char *Argv0, void *MainAddr) {
  std::unique_ptr<CompilerInstance> Clang(new CompilerInstance());
  IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());

  /**
  // Register the support for object-file-wrapped Clang modules.
  auto PCHOps = Clang->getPCHContainerOperations();
  PCHOps->registerWriter(llvm::make_unique<ObjectFilePCHContainerWriter>());
  PCHOps->registerReader(llvm::make_unique<ObjectFilePCHContainerReader>());
  */

  // Initialize targets first, so that --version shows registered targets.
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  std::string inputString; //string buffer to store input from config files
  frontend::IncludeDirGroup Group = frontend::Angled; //for -I command line arguments

  CustomDiagContainer DiagContainer;

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

  DiagContainer.PrintDiagnostics();
  //tryting to separate current diagnostic info from the next execution
  llvm::outs() << "------------------------------------------------------\n";
  llvm::outs() << "\n";

  return 0;

  /*
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

  // Create the actual diagnostics engine.
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

  // Execute the frontend actions.
  Success = ExecuteCompilerInvocation(Clang.get());

  // If any timers were active but haven't been destroyed yet, print their
  // results now.  This happens in -disable-free mode.
  llvm::TimerGroup::printAll(llvm::errs());

  // Our error handler depends on the Diagnostics object, which we're
  // potentially about to delete. Uninstall the handler now so that any
  // later errors use the default handling behavior instead.
  llvm::remove_fatal_error_handler();

  // When running with -disable-free, don't do any destruction or shutdown.
  if (Clang->getFrontendOpts().DisableFree) {
    if (llvm::AreStatisticsEnabled() || Clang->getFrontendOpts().ShowStats)
      llvm::PrintStatistics();
    BuryPointer(std::move(Clang));
    return !Success;
  }

  // Managed static deconstruction. Useful for making things like
  // -time-passes usable.
  llvm::llvm_shutdown();

  return !Success;
  */
}
