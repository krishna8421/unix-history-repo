//===--- CheckerRegistration.cpp - Registration for the Analyzer Checkers -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Defines the registration function for the analyzer checkers.
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Frontend/CheckerRegistration.h"
#include "../Checkers/ClangSACheckerProvider.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"
#include "clang/StaticAnalyzer/Core/CheckerProvider.h"
#include "clang/Frontend/AnalyzerOptions.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Basic/Diagnostic.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/SmallVector.h"

using namespace clang;
using namespace ento;

CheckerManager *ento::registerCheckers(const AnalyzerOptions &opts,
                                       Diagnostic &diags) {
  llvm::OwningPtr<CheckerManager> checkerMgr(new CheckerManager());

  llvm::SmallVector<CheckerOptInfo, 8> checkerOpts;
  for (unsigned i = 0, e = opts.CheckersControlList.size(); i != e; ++i) {
    const std::pair<std::string, bool> &opt = opts.CheckersControlList[i];
    checkerOpts.push_back(CheckerOptInfo(opt.first.c_str(), opt.second));
  }

  llvm::OwningPtr<CheckerProvider> provider(createClangSACheckerProvider());
  provider->registerCheckers(*checkerMgr,
                             checkerOpts.data(), checkerOpts.size());

  // FIXME: Load CheckerProviders from plugins.

  for (unsigned i = 0, e = checkerOpts.size(); i != e; ++i) {
    if (checkerOpts[i].isUnclaimed())
      diags.Report(diag::warn_unkwown_analyzer_checker)
          << checkerOpts[i].getName();
  }

  return checkerMgr.take();
}
