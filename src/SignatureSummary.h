// Copyright (c) 2017 VMware, Inc. All Rights Reserved.
// SPDX-License-Identifier: GPL-2.0

#pragma once
#include <algorithm>
#include <map>
#include <set>
#include "SignatureDirectory.h"
namespace chap {
template <class Offset>
class SignatureSummary {
 public:
  struct Tally {
    Tally() : _count(0), _bytes(0) {}
    Tally(Offset count, Offset bytes) : _count(count), _bytes(bytes) {}
    Tally(const Tally& other) : _count(other._count), _bytes(other._bytes) {}
    Offset _count;
    Offset _bytes;
  };
  typedef std::map<Offset, Tally> OffsetToTally;
  typedef typename OffsetToTally::iterator OffsetToTallyIterator;
  typedef typename OffsetToTally::const_iterator OffsetToTallyConstIterator;
  typedef std::map<std::string, Tally> NameToTally;
  typedef typename NameToTally::iterator NameToTallyIterator;
  typedef typename NameToTally::const_iterator NameToTallyConstIterator;
  struct Item {
    std::string _name;
    Tally _totals;
    std::vector<std::pair<Offset, Tally> > _subtotals;
    void AddSubtotal(Offset signature, const Tally& tally) {
      _subtotals.push_back(std::make_pair(signature, tally));
    }
  };

  SignatureSummary(const SignatureDirectory<Offset>& directory)
      : _directory(directory), _unsignedTally(0, 0) {}

  bool AdjustTally(Offset size, const char* image) {
    Offset signature = 0;
    if (size >= sizeof(Offset)) {
      signature = *((Offset*)image);
    }
    if (_directory.IsMapped(signature)) {
      TallyBySignature(signature, size);
      std::string name = _directory.Name(signature);
      if (!name.empty()) {
        TallyByName(name, size);
      }
    } else {
      _unsignedTally._count++;
      _unsignedTally._bytes += size;
      typename std::map<Offset, Offset>::iterator it =
          _unsignedSizeToCount.find(size);
      if (it == _unsignedSizeToCount.end()) {
        _unsignedSizeToCount[size] = 1;
      } else {
        it->second = it->second + 1;
      }
    }
    return false;
  }

  void SummarizeByCount(std::vector<Item>& items) const {
    FillItems(items);
    for (typename std::vector<Item>::iterator it = items.begin();
         it != items.end(); ++it) {
      if (it->_subtotals.size() > 1) {
        std::sort(it->_subtotals.begin(), it->_subtotals.end(),
                  CompareSubtotalsByCount());
      }
    }
    std::sort(items.begin(), items.end(), CompareItemsByCount());
  }

  void SummarizeByBytes(std::vector<Item>& items) const {
    FillItems(items);
    for (typename std::vector<Item>::iterator it = items.begin();
         it != items.end(); ++it) {
      if (it->_subtotals.size() > 1) {
        std::sort(it->_subtotals.begin(), it->_subtotals.end(),
                  CompareSubtotalsByBytes());
      }
    }
    std::sort(items.begin(), items.end(), CompareItemsByBytes());
  }

 private:
  const SignatureDirectory<Offset>& _directory;
  OffsetToTally _signatureToTally;
  NameToTally _nameToTally;
  Tally _unsignedTally;
  std::map<Offset, Offset> _unsignedSizeToCount;

  void TallyBySignature(Offset signature, Offset size) {
    OffsetToTallyIterator it = _signatureToTally.find(signature);
    if (it != _signatureToTally.end()) {
      it->second._count++;
      it->second._bytes += size;
    } else {
      _signatureToTally[signature] = Tally(1, size);
    }
  }
  void TallyByName(std::string name, Offset size) {
    NameToTallyIterator it = _nameToTally.find(name);
    if (it != _nameToTally.end()) {
      it->second._count++;
      it->second._bytes += size;
    } else {
      _nameToTally[name] = Tally(1, size);
    }
  }

  void FillItems(std::vector<Item>& items) const {
    items.clear();
    if (_unsignedTally._count > 0) {
      items.push_back(Item());
      Item& item = items.back();
      item._name = "-";
      item._totals = _unsignedTally;
      for (typename std::map<Offset, Offset>::const_iterator it =
               _unsignedSizeToCount.begin();
           it != _unsignedSizeToCount.end(); ++it) {
        item.AddSubtotal(it->first, Tally(it->second, it->first * it->second));
      }
    }
    FillUnnamedSignatures(items);
    FillNamedSignatures(items);
  }

  void FillUnnamedSignatures(std::vector<Item>& items) const {
    for (OffsetToTallyConstIterator it = _signatureToTally.begin();
         it != _signatureToTally.end(); ++it) {
      std::string name = _directory.Name(it->first);
      if (name.empty()) {
        items.push_back(Item());
        Item& item = items.back();
        item._totals = it->second;
        item.AddSubtotal(it->first, it->second);
      }
    }
  }
  void FillNamedSignatures(std::vector<Item>& items) const {
    for (NameToTallyConstIterator it = _nameToTally.begin();
         it != _nameToTally.end(); ++it) {
      const std::string& name = it->first;
      items.push_back(Item());
      Item& item = items.back();
      item._name = name;
      item._totals = it->second;

      const std::set<Offset>& signatures = _directory.Signatures(name);
      for (typename std::set<Offset>::const_iterator itSig = signatures.begin();
           itSig != signatures.end(); ++itSig) {
        Offset signature = *itSig;
        OffsetToTallyConstIterator itTally = _signatureToTally.find(signature);
        if (itTally != _signatureToTally.end()) {
          item.AddSubtotal(signature, itTally->second);
        }
      }
    }
  }
  struct CompareSubtotalsByCount {
    bool operator()(const std::pair<Offset, Tally>& left,
                    const std::pair<Offset, Tally>& right) {
      return (left.second._count > right.second._count) ||
             ((left.second._count == right.second._count) &&
              (left.first < right.first));
    }
  };

  struct CompareSubtotalsByBytes {
    bool operator()(const std::pair<Offset, Tally>& left,
                    const std::pair<Offset, Tally>& right) {
      return (left.second._bytes > right.second._bytes) ||
             ((left.second._bytes == right.second._bytes) &&
              (left.first < right.first));
    }
  };

  struct CompareItemsByCount {
    bool operator()(const Item& left, const Item& right) {
      return (left._totals._count > right._totals._count) ||
             ((left._totals._count == right._totals._count) &&
              ((left._name < right._name) ||
               ((left._name == right._name) &&
                (left._subtotals.begin()->first <
                 right._subtotals.begin()->first))));
    }
  };

  struct CompareItemsByBytes {
    bool operator()(const Item& left, const Item& right) {
      return (left._totals._bytes > right._totals._bytes) ||
             (left._totals._bytes == right._totals._bytes &&
              ((left._name < right._name) ||
               ((left._name == right._name) &&
                (left._subtotals.begin()->first) <
                    (right._subtotals.begin()->first))));
    }
  };
};
}  // namespace chap
