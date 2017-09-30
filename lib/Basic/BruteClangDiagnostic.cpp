#include "clang/Basic/CharInfo.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/PartialDiagnostic.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/CrashRecoveryContext.h"
#include "llvm/Support/Locale.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/Basic/BruteClangDiagnostic.h"

using namespace clang;

void CustomDiagConsumer::anchor() { }

void CustomDiagConsumer::HandleDiagnostic(DiagnosticsEngine::Level DiagLevel, const Diagnostic &Info){

  llvm::SmallVector<char, 256> message_SmallVector; //character buffer for formatting diagnostics messages
  Info.FormatDiagnostic(message_SmallVector); //format the diagnostic message into the message buffer
  std::string message(message_SmallVector.begin(), message_SmallVector.end()); //convert the llvm::SmallVector buffer to a std::string obj
  
  unsigned LineNumber = Info.getSourceManager().getSpellingLineNumber(Info.getLocation());
  //get column number using PresumedLocation

  unsigned ColumnNumber = Info.getSourceManager().getPresumedColumnNumber(Info.getLocation());
  
  llvm::StringRef FileName_StringRef = Info.getSourceManager().getFilename(Info.getLocation());
  std::string FileName(FileName_StringRef.begin(), FileName_StringRef.end()); //file name as std::string

  DiagContainer.AddDiagnostic(FileName, ColumnNumber, LineNumber, message);
}


bool CustomDiagContainer::DiagExists(std::string &message, unsigned line, std::string &file){
  for (std::list<DiagData>::iterator it = DiagList.begin(); it != DiagList.end(); it++){
    if((it->msg == message)&&(it->LineNumber == line)&&(it->FileName == file)){
      return true; //return true if any of the structs match line number and message and file name
    }
  }
  return false; //if for loop did not return true, then return false.
}

void CustomDiagContainer::AddNewDiagData(std::string &FileName, unsigned ColumnNumber, unsigned LineNumber, std::string &message){
  DiagData DD;
  DD.CI_Names = CompilerInstanceName;
  DD.msg = message;
  DD.FileName = FileName;
  DD.LineNumber = LineNumber;
  DiagList.push_back(DD);
  return;
}

void CustomDiagContainer::AddToExistingDiagData(std::string &message, unsigned line, std::string &file){

  for (std::list<DiagData>::iterator it = DiagList.begin(); it != DiagList.end(); it++){
    if((it->msg == message)&&(it->LineNumber == line)&&(it->FileName == file)){
      it->CI_Names.append(", ");
      it->CI_Names.append(CompilerInstanceName);
    }
  }
}

void CustomDiagContainer::SetCompilerInstanceName(std::string &CI_Name){
  CompilerInstanceName = CI_Name;
  return;
}

void CustomDiagContainer::AddDiagnostic(std::string &FileName, unsigned ColumnNumber, unsigned LineNumber, std::string &message){

  //if diaglist is empty, then does not exist & create new struct
  if (DiagList.empty()){
    AddNewDiagData(FileName, ColumnNumber, LineNumber, message);
  }
  //if diaglist is not empty, use AlreadyExists to check if already exists
  else{
    if(!(DiagExists(message, LineNumber, FileName))){
      //does not already exist, so add new struct
      AddNewDiagData(FileName, ColumnNumber, LineNumber, message);
    }
    else{
      AddToExistingDiagData(message, LineNumber, FileName);
    }
  }
}

void CustomDiagContainer::PrintDiagnostics(){ //TODO: Multiple structs case not handled yet
  unsigned NumStructs = DiagList.size();
  if (NumStructs == 0){
    llvm::outs() << "No errors reported!\n";
  }
  else{
    for (std::list<DiagData>::iterator it = DiagList.begin(); it != DiagList.end(); it++){
      llvm::errs() << it->CI_Names << ":\n In file ";
      llvm::errs() << it->FileName << ": Line " << it->LineNumber << ":" << " error: " << it->msg << "\n";
    }
  }
    
}