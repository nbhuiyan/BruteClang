#include "BruteClangDiagnostic.h"

void CustomDiagConsumer::anchor() { }

void CustomDiagConsumer::HandleDiagnostic(DiagnosticsEngine::Level DiagLevel, const Diagnostic &Info){

  llvm::SmallVector<char, 256> message_SmallVector; //character buffer for formatting diagnostics messages
  Info.FormatDiagnostic(message_SmallVector); //format the diagnostic message into the message buffer
  std::string message(message_SmallVector.begin(), message_SmallVector.end()); //convert the llvm::SmallVector buffer to a std::string obj
  
  unsigned ColumnNumber = Info.getSourceManager().getSpellingColumnNumber(Info.getLocation());
  unsigned LineNumber = Info.getSourceManager().getSpellingLineNumber(Info.getLocation());
  
  llvm::StringRef FileName_StringRef = Info.getSourceManager().getFilename(Info.getLocation());
  std::string FileName(FileName_StringRef.begin(), FileName_StringRef.end()); //file name as std::string

  DiagContainer.AddDiagnostic(FileName, ColumnNumber, LineNumber, message);
}


bool CustomDiagContainer::AlreadyExists(std::string &message, unsigned line){
  if (DiagList.size() == 1){
    if ((DiagList.begin()->msg == message)&&(DiagList.begin()->LineNumber == line)){
      return true;
    }
  }
  for (std::list<DiagData>::iterator it = DiagList.begin(); it != DiagList.end(); it++){
    if((it->msg == message)&&(it->LineNumber == line)){
      return true; //return true if any of the structs match line number and message
    }
  }
  return false; //if for loop did not return true, then return false.
}

void CustomDiagContainer::AddNewStruct(std::string &FileName, unsigned ColumnNumber, unsigned LineNumber, std::string &message){
  DiagData DD;
  DD.CI_Names = CompilerInstanceName;
  DD.msg = message;
  DD.FileName = FileName;
  DD.LineNumber = LineNumber;
  DiagList.push_back(DD);
  return;
}

void CustomDiagContainer::AddToExistingStruct(std::string &message, unsigned line){
  if (DiagList.size() == 1){
    DiagList.begin()->CI_Names.append(", ");
    DiagList.begin()->CI_Names.append(CompilerInstanceName);
  }
  else{
    for (std::list<DiagData>::iterator it = DiagList.begin(); it != DiagList.end(); it++){
      if((it->msg == message)&&(it->LineNumber == line)){
        it->CI_Names.append(", ");
        it->CI_Names.append(CompilerInstanceName);
      }
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
    AddNewStruct(FileName, ColumnNumber, LineNumber, message);
  }
  //if diaglist is not empty, use AlreadyExists to check if already exists
  else{
    if(!(AlreadyExists(message, LineNumber))){
      //does not already exist, so add new struct
      AddNewStruct(FileName, ColumnNumber, LineNumber, message);
    }
    else{
      AddToExistingStruct(message, LineNumber);
    }
  }
}

void CustomDiagContainer::PrintDiagnostics(){ //TODO: Multiple structs case not handled yet
  unsigned NumStructs = DiagList.size();
  if (NumStructs == 0){
    llvm::outs() << "No compiler instance reported any errors!\n";
  }
  else if(NumStructs == 1){
    llvm::errs() << DiagList.begin()->CI_Names << ":\n";
    llvm::errs() << DiagList.begin()->FileName << ":" << DiagList.begin()->LineNumber <<
    ":" << DiagList.begin()->ColumnNumber << ": error: "<< DiagList.begin()->msg << "\n";
  }
  else{
    for (std::list<DiagData>::iterator it = DiagList.begin(); it != DiagList.end(); it++){
      llvm::errs() << it->CI_Names << ":\n";
      llvm::errs() << it->FileName << ":" << it->LineNumber << ":" << it->ColumnNumber <<
      " error: " << it->msg << "\n";
    }
  }
    
}