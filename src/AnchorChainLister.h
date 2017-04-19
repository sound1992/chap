// Copyright (c) 2017 VMware, Inc. All Rights Reserved.
// SPDX-License-Identifier: GPL-2.0

#pragma once
#include "Allocations/Graph.h"
#include "Commands/Runner.h"
#include "Explainer.h"
#include "InModuleExplainer.h"
#include "SignatureDirectory.h"
#include "StackExplainer.h"
namespace chap {
template <typename Offset>
class AnchorChainLister
    : public Allocations::Graph<Offset>::AnchorChainVisitor {
 public:
  AnchorChainLister(const InModuleExplainer<Offset>& inModuleExplainer,
                    const StackExplainer<Offset>& stackExplainer,
                    const Allocations::Graph<Offset>& graph,
                    const SignatureDirectory<Offset>* signatureDirectory,
                    Commands::Context& context, Offset anchoree)
      : _graph(graph),
        _inModuleExplainer(inModuleExplainer),
        _stackExplainer(stackExplainer),
        _signatureDirectory(signatureDirectory),
        _context(context),
        _anchoree(anchoree),
        _numStaticAnchorChainsShown(0),
        _numStackAnchorChainsShown(0),
        _numRegisterAnchorChainsShown(0),
        _numDirectStaticAnchorChainsShown(0),
        _numDirectStackAnchorChainsShown(0),
        _numDirectRegisterAnchorChainsShown(0) {
    // head vs whole chain
  }

  bool VisitStaticAnchorChainHeader(const std::vector<Offset>& staticAddrs,
                                    Offset address, Offset size,
                                    const char* image) {
    Commands::Output& output = _context.GetOutput();
    const bool isDirect = (address == _anchoree);
    if (!isDirect && (_numDirectStaticAnchorChainsShown > 0 ||
                      _numStaticAnchorChainsShown == 10)) {
      // Report at most 10 static anchor chains.
      return true;
    }
    output << "Allocation at " << std::hex << _anchoree << " appears to be ";
    if (isDirect) {
      output << "directly statically anchored.\n";
    } else {
      output << "indirectly statically anchored\n... via anchor point "
             << address;
      ShowSignatureIfPresent(output, address, size, image);
      output << "\n";
    }
    for (typename std::vector<Offset>::const_iterator it = staticAddrs.begin();
         it != staticAddrs.end(); ++it) {
      Offset staticAddr = *it;
      _inModuleExplainer.Explain(_context, staticAddr);
      output << "Static address " << staticAddr << " references"
             << (isDirect ? " " : " anchor point ") << address << "\n";
    }
    _numStaticAnchorChainsShown++;
    if (isDirect) {
      _numDirectStaticAnchorChainsShown++;
    }
    return false;
  }

  bool VisitStackAnchorChainHeader(const std::vector<Offset>& stackAddrs,
                                   Offset address, Offset size,
                                   const char* image) {
    Commands::Output& output = _context.GetOutput();
    const bool isDirect = (address == _anchoree);
    if (!isDirect && (_numDirectStackAnchorChainsShown > 0 ||
                      _numStackAnchorChainsShown == 10)) {
      // Report at most 10 stack anchor chains.
      return true;
    }
    output << "Allocation at " << std::hex << _anchoree << " appears to be ";
    if (isDirect) {
      output << "directly anchored from at least one stack.\n";
    } else {
      output << "indirectly anchored from at least one stack\n"
                "via anchor point "
             << address;
      ShowSignatureIfPresent(output, address, size, image);
      output << "\n";
    }
    for (typename std::vector<Offset>::const_iterator it = stackAddrs.begin();
         it != stackAddrs.end(); ++it) {
      Offset stackAddr = *it;
      _stackExplainer.Explain(_context, stackAddr);
      output << "Stack address " << std::hex << stackAddr << " references"
             << (isDirect ? " " : " anchor point ") << address << "\n";
    }
    _numStackAnchorChainsShown++;
    if (isDirect) {
      _numDirectStackAnchorChainsShown++;
    }
    return false;
  }

  bool VisitRegisterAnchorChainHeader(
      const std::vector<std::pair<size_t, const char*> >& anchors,
      Offset address, Offset size, const char* image) {
    Commands::Output& output = _context.GetOutput();
    const bool isDirect = (address == _anchoree);
    if (!isDirect && (_numDirectRegisterAnchorChainsShown > 0 ||
                      _numRegisterAnchorChainsShown == 10)) {
      // Report at most 10 register anchor chains.
      return true;
    }
    output << "Allocation at " << std::hex << _anchoree << " appears to be ";
    if (isDirect) {
      output << "directly anchored from at least one register.\n";
    } else {
      output << "indirectly anchored from at least one register\n"
                "via anchor point "
             << address;
      ShowSignatureIfPresent(output, address, size, image);
      output << "\n";
    }
    for (typename std::vector<std::pair<size_t, const char*> >::const_iterator
             it = anchors.begin();
         it != anchors.end(); ++it) {
      output << "Register " << (*it).second << " for thread " << std::dec
             << (*it).first << " references"
             << (isDirect ? " " : " anchor point ") << std::hex << address
             << "\n";
    }
    _numRegisterAnchorChainsShown++;
    if (isDirect) {
      _numDirectRegisterAnchorChainsShown++;
    }
    return false;
  }

  bool VisitChainLink(Offset address, Offset size, const char* image) {
    Commands::Output& output = _context.GetOutput();
    output << "... which references " << std::hex << address;
    if (address != _anchoree) {
      ShowSignatureIfPresent(output, address, size, image);
    }
    output << "\n";
    return false;
  }

 private:
  const Allocations::Graph<Offset>& _graph;
  const InModuleExplainer<Offset>& _inModuleExplainer;
  const StackExplainer<Offset>& _stackExplainer;
  const SignatureDirectory<Offset>* _signatureDirectory;
  Commands::Context& _context;
  const Offset _anchoree;
  size_t _numStaticAnchorChainsShown;
  size_t _numStackAnchorChainsShown;
  size_t _numRegisterAnchorChainsShown;
  size_t _numDirectStaticAnchorChainsShown;
  size_t _numDirectStackAnchorChainsShown;
  size_t _numDirectRegisterAnchorChainsShown;

  void ShowSignatureIfPresent(Commands::Output& output, Offset address,
                              Offset size, const char* image) {
    if (size >= sizeof(Offset)) {
      Offset signature = *((Offset*)image);
      if ((_signatureDirectory != ((SignatureDirectory<Offset>*)(0))) &&
          _signatureDirectory->IsMapped(signature)) {
        output << " with signature " << signature;
        std::string name = _signatureDirectory->Name(signature);
        if (!name.empty()) {
          output << "(" << name << ")";
        }
      }
    }
  }
};
}  // namespace chap
