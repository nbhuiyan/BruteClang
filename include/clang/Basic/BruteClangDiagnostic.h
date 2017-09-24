#include "clang/Basic/Diagnostic.h"

namespace clang {
    class CustomDiagContainer;
    class CustomDiagConsumer;


// ------ Custom Diagnostic Contanier for OMRChecker
class CustomDiagContainer{
    typedef struct DiagData{
      std::string CI_Names;
      std::string msg;
      std::string FileName;
      unsigned LineNumber;
      unsigned ColumnNumber;
    } DiagData;
  private:
    std::string CompilerInstanceName;
    std::list<DiagData> DiagList;

    //this function checks if the line number and diag message combination already exists
    bool DiagExists(std::string &message, unsigned line, std::string &file);

    //if a diagnostic message and line number combination does not already exist, create a new one.
    void AddNewDiagData(std::string &FileName, unsigned ColumnNumber, unsigned LineNumber, std::string &message);

    //if a diagnostic message and line number combination exists, then add to the existing corresponding struct.
    void AddToExistingDiagData(std::string &message, unsigned line, std::string &file);


  public:
    //from cc1_main, this will be used to let the container know what compiler instance this is.
    void SetCompilerInstanceName(std::string &CI_Name);

    //from HandleDiagnostics, this will be used to pass a new diagnostic to the container.
    void AddDiagnostic(std::string &FileName, unsigned ColumnNumber, unsigned LineNumber, std::string &message);

    //from cc1-main, this will be used for handling 
    void PrintDiagnostics();
};

// ------ custom diagnostic consumer for OMRChecker
class CustomDiagConsumer : public DiagnosticConsumer {
  virtual void anchor();
  void HandleDiagnostic(DiagnosticsEngine::Level DiagLevel, const Diagnostic &Info);

  private:
    CustomDiagContainer &DiagContainer;

  public:
    CustomDiagConsumer(CustomDiagContainer& Container) : DiagContainer(Container) {}
};

} //end namespace clang