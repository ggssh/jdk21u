/*
 * Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_UTILITIES_PAIR_HPP
#define SHARE_UTILITIES_PAIR_HPP

#include "memory/allocation.hpp"

template<typename T, typename V,  typename ALLOC_BASE = ResourceObj>
class Pair : public ALLOC_BASE {
 public:
  T first;
  V second;

  Pair() {}
  Pair(T t, V v) : first(t), second(v) {}

  bool operator==(const Pair<T, V>& other) const {
    return (this->first == other.first) && (this->second == other.second);
  }
};

template<typename ALLOC_BASE>
class Pair<const char*, const char*, ALLOC_BASE> : public ALLOC_BASE {
public:
  const char* first;
  const char* second;

  Pair() : first(nullptr), second(nullptr) {}
  Pair(const char* t, const char* v) : first(t), second(v) {}

  bool operator==(const Pair<const char*, const char*, ALLOC_BASE>& other) const {
    bool firstEq = string_equal(this->first, other.first);
    bool secondEq = string_equal(this->second, other.second);
    return firstEq && secondEq;
  }

  bool operator!=(const Pair<const char*, const char*, ALLOC_BASE>& other) const {
    return !(*this == other);
  }

  bool equals(const Pair<const char*, const char*, ALLOC_BASE>& other) const {
    return (*this == other);
  }

private:
  static bool string_equal(const char* s1, const char* s2) {
    if (s1 == nullptr && s2 == nullptr) {
      return true;
    }
    if (s1 == nullptr || s2 == nullptr) {
      return false;
    }
    return strcmp(s1, s2) == 0;
  }
};


#endif // SHARE_UTILITIES_PAIR_HPP
