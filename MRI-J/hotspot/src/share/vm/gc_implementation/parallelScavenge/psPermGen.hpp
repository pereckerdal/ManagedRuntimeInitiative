/*
 * Copyright 2001-2005 Sun Microsystems, Inc.  All Rights Reserved.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *  
 */
// This file is a derivative work resulting from (and including) modifications
// made by Azul Systems, Inc.  The date of such changes is 2010.
// Copyright 2010 Azul Systems, Inc.  All Rights Reserved.
//
// Please contact Azul Systems, Inc., 1600 Plymouth Street, Mountain View, 
// CA 94043 USA, or visit www.azulsystems.com if you need additional information 
// or have any questions.
#ifndef PSPERMGEN_HPP
#define PSPERMGEN_HPP

#include "psOldGen.hpp"

class AdaptivePaddedAverage;

class PSPermGen : public PSOldGen {
 protected:
  AdaptivePaddedAverage* _avg_size;  // Used for sizing
  size_t _last_used;                 // Amount used at last GC, used for sizing

 public:
  // Initialize the generation.
  PSPermGen(ReservedSpace rs, size_t alignment, size_t initial_byte_size,
	    size_t minimum_byte_size, size_t maximum_byte_size,
            const char* gen_name, int level);

  // Permanent Gen special allocation. Uses the OldGen allocation
  // routines, which should not be directly called on this generation.
  HeapWord* allocate_permanent(size_t word_size);

  // Size calculation. 
  void compute_new_size(size_t used_before_collection);

  // MarkSweep code
  virtual void precompact();

  // Parallel old
  virtual void move_and_update(ParCompactionManager* cm);

  virtual const char* name() const { return "PSPermGen"; }
};

#endif // PSPERMGEN_HPP

