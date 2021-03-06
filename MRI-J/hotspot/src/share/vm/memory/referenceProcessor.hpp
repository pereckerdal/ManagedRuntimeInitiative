/*
 * Copyright 2001-2007 Sun Microsystems, Inc.  All Rights Reserved.
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
#ifndef REFERENCEPROCESSOR_HPP
#define REFERENCEPROCESSOR_HPP


#include "allocation.hpp"
#include "iterator.hpp"
#include "memRegion.hpp"
#include "objectRef_pd.hpp"

// ReferenceProcessor class encapsulates the per-"collector" processing
// of "weak" references for GC. The interface is useful for supporting
// a generational abstraction, in particular when there are multiple
// generations that are being independently collected -- possibly
// concurrently and/or incrementally.  Note, however, that the
// ReferenceProcessor class abstracts away from a generational setting
// by using only a heap interval (called "span" below), thus allowing
// its use in a straightforward manner in a general, non-generational
// setting.
//
// The basic idea is that each ReferenceProcessor object concerns
// itself with ("weak") reference processing in a specific "span"
// of the heap of interest to a specific collector. Currently,
// the span is a convex interval of the heap, but, efficiency
// apart, there seems to be no reason it couldn't be extended
// (with appropriate modifications) to any "non-convex interval".

// forward references
class ReferencePolicy;
class AbstractRefProcTaskExecutor;
class DiscoveredList;

class ReferenceProcessor : public CHeapObj {
 friend class DiscoveredList;
 friend class DiscoveredListIterator;
 protected:
  // End of list marker
  static objectRef  _sentinelRef;
  MemRegion         _span; // (right-open) interval of heap
                           // subject to wkref discovery
  bool              _discovering_refs;      // true when discovery enabled
  bool              _discovery_is_atomic;   // if discovery is atomic wrt
                                            // other collectors in configuration
  bool              _discovery_is_mt;       // true if reference discovery is MT.
  bool	            _enqueuing_is_done;     // true if all weak references enqueued
  bool              _processing_is_mt;      // true during phases when
                                            // reference processing is MT.
  int               _next_id;               // round-robin counter in
                                            // support of work distribution

  // For collectors that do not keep GC marking information
  // in the object header, this field holds a closure that
  // helps the reference processor determine the reachability
  // of an oop (the field is currently initialized to NULL for
  // all collectors but the CMS collector).
  BoolObjectClosure* _is_alive_non_header;

  // The discovered ref lists themselves
  int             _num_q;       // the MT'ness degree of the queues below
  DiscoveredList* _discoveredSoftRefs; // pointer to array of oops
  DiscoveredList* _discoveredWeakRefs;
  DiscoveredList* _discoveredFinalRefs;
  DiscoveredList* _discoveredPhantomRefs;

 public:
  int  num_q()                           { return _num_q; }
  DiscoveredList* discovered_soft_refs() { return _discoveredSoftRefs; }
  static objectRef* sentinel_ref()             { return &_sentinelRef; }

 public:
  // Process references with a certain reachability level.
  void process_discovered_reflist(DiscoveredList               refs_lists[],
                                  ReferencePolicy*             policy,
                                  bool                         clear_referent,
                                  BoolObjectClosure*           is_alive,
                                  OopClosure*                  keep_alive,
                                  VoidClosure*                 complete_gc,
                                  AbstractRefProcTaskExecutor* task_executor);
                                          
  void process_phaseJNI(BoolObjectClosure* is_alive,
                        OopClosure*        keep_alive,
                        VoidClosure*       complete_gc);

  // Work methods used by the method process_discovered_reflist
  // Phase1: keep alive all those referents that are otherwise
  // dead but which must be kept alive by policy (and their closure).
  void process_phase1(DiscoveredList&     refs_list_addr,
                      ReferencePolicy*    policy,
                      BoolObjectClosure*  is_alive,
                      OopClosure*         keep_alive,
                      VoidClosure*        complete_gc);
  // Phase2: remove all those references whose referents are
  // reachable.
  inline void process_phase2(DiscoveredList&    refs_list_addr,
                             BoolObjectClosure* is_alive,
                             OopClosure*        keep_alive, 
                             VoidClosure*       complete_gc) {
    if (discovery_is_atomic()) {
      // complete_gc is ignored in this case for this phase
      pp2_work(refs_list_addr, is_alive, keep_alive);
    } else {
      assert(complete_gc != NULL, "Error");
      pp2_work_concurrent_discovery(refs_list_addr, is_alive,
                                    keep_alive, complete_gc);
    }
  }
  // Work methods in support of process_phase2
  void pp2_work(DiscoveredList&    refs_list_addr,
                BoolObjectClosure* is_alive,
                OopClosure*        keep_alive);
  void pp2_work_concurrent_discovery(
                DiscoveredList&    refs_list_addr,
                BoolObjectClosure* is_alive,
                OopClosure*        keep_alive,
                VoidClosure*       complete_gc);
  // Phase3: process the referents by either clearing them
  // or keeping them alive (and their closure)
  void process_phase3(DiscoveredList&    refs_list_addr,
                      bool               clear_referent,
                      BoolObjectClosure* is_alive,
                      OopClosure*        keep_alive,
                      VoidClosure*       complete_gc);

  // Enqueue references with a certain reachability level
  void enqueue_discovered_reflist(DiscoveredList& refs_list, objectRef* pending_list_addr);
                                  
  // "Preclean" all the discovered reference lists
  // by removing references with strongly reachable referents.
  // The first argument is a predicate on an oop that indicates
  // its (strong) reachability and the second is a closure that
  // may be used to incrementalize or abort the precleaning process.
  // The caller is responsible for taking care of potential
  // interference with concurrent operations on these lists
  // (or predicates involved) by other threads. Currently
  // only used by the CMS collector.
  void preclean_discovered_references(BoolObjectClosure* is_alive,
                                      OopClosure*        keep_alive,
                                      VoidClosure*       complete_gc,
                                      YieldClosure*      yield);
                                      

  // Returns the name of the discovered reference list 
  // occupying the i / _num_q slot.
  const char* list_name(int i);

 protected:
  // "Preclean" the given discovered reference list
  // by removing references with strongly reachable referents.
  // Currently used in support of CMS only.
  void preclean_discovered_reflist(DiscoveredList&    refs_list,
                                   BoolObjectClosure* is_alive,
                                   OopClosure*        keep_alive,
                                   VoidClosure*       complete_gc,
                                   YieldClosure*      yield);
                                   
  void enqueue_discovered_reflists(objectRef* pending_list_addr, AbstractRefProcTaskExecutor* task_executor);
  int next_id() {
    int id = _next_id;
    if (++_next_id == _num_q) {
      _next_id = 0;
    }
    return id;
  }
  DiscoveredList* get_discovered_list(ReferenceType rt);
  inline void add_to_discovered_list_mt(DiscoveredList& refs_list, objectRef obj,
                                        objectRef* discovered_addr);
  void verify_ok_to_handle_reflists() PRODUCT_RETURN;

  void abandon_partial_discovered_list(DiscoveredList& refs_list);
  void abandon_partial_discovered_list_arr(DiscoveredList refs_lists[]);

  // Calculate the number of jni handles.
  unsigned int count_jni_refs();
  
  // Balances reference queues.
  void balance_queues(DiscoveredList ref_lists[]);
  
  // Update (advance) the soft ref master clock field.
  void update_soft_ref_master_clock();
  
 public:
  // constructor
  ReferenceProcessor():
    _span((HeapWord*)NULL, (HeapWord*)NULL),
    _discoveredSoftRefs(NULL),  _discoveredWeakRefs(NULL),
    _discoveredFinalRefs(NULL), _discoveredPhantomRefs(NULL),
    _discovering_refs(false),
    _discovery_is_atomic(true),
    _enqueuing_is_done(false),
    _discovery_is_mt(false),
    _is_alive_non_header(NULL),
    _num_q(0),
    _processing_is_mt(false),
    _next_id(0)
  {}

  ReferenceProcessor(MemRegion span, bool atomic_discovery,
                     bool mt_discovery, int mt_degree = 1, 
                     bool mt_processing = false);
   
  // Allocates and initializes a reference processor.
  static ReferenceProcessor* create_ref_processor(
    MemRegion          span, 
    bool               atomic_discovery,
    bool               mt_discovery,
    BoolObjectClosure* is_alive_non_header = NULL,
    int                parallel_gc_threads = 1, 
    bool               mt_processing = false);

  // RefDiscoveryPolicy values
  enum {
    ReferenceBasedDiscovery = 0,
    ReferentBasedDiscovery  = 1
  };

  static void init_statics();

 public:
  // get and set "is_alive_non_header" field
  BoolObjectClosure* is_alive_non_header() {
    return _is_alive_non_header;
  }
  void set_is_alive_non_header(BoolObjectClosure* is_alive_non_header) {
    _is_alive_non_header = is_alive_non_header;
  }

  // get and set span
  MemRegion span()                   { return _span; }
  void      set_span(MemRegion span) { _span = span; }

  // start and stop weak ref discovery
  void enable_discovery()   { _discovering_refs = true;  }
  void disable_discovery()  { _discovering_refs = false; }
  bool discovery_enabled()  { return _discovering_refs;  }

  // whether discovery is atomic wrt other collectors
  bool discovery_is_atomic() const { return _discovery_is_atomic; }
  void set_atomic_discovery(bool atomic) { _discovery_is_atomic = atomic; }

  // whether discovery is done by multiple threads same-old-timeously
  bool discovery_is_mt() const { return _discovery_is_mt; }
  void set_mt_discovery(bool mt) { _discovery_is_mt = mt; }

  // Whether we are in a phase when _processing_ is MT.
  bool processing_is_mt() const { return _processing_is_mt; }
  void set_mt_processing(bool mt) { _processing_is_mt = mt; }

  // whether all enqueuing of weak references is complete
  bool enqueuing_is_done()  { return _enqueuing_is_done; }
  void set_enqueuing_is_done(bool v) { _enqueuing_is_done = v; }

  // iterate over oops
  void weak_oops_do(OopClosure* f);       // weak roots
  static void oops_do(OopClosure* f);     // strong root(s)

  // Discover a Reference object, using appropriate discovery criteria
  bool discover_reference(oop obj, ReferenceType rt);

  friend class PauselessScavenger;
  // Process references found during GC (called by the garbage collector)
  void process_discovered_references(ReferencePolicy*             policy,
                                     BoolObjectClosure*           is_alive,
                                     OopClosure*                  keep_alive,
                                     VoidClosure*                 complete_gc,
                                     AbstractRefProcTaskExecutor* task_executor);

 public:
  // Enqueue references at end of GC (called by the garbage collector)
  bool enqueue_discovered_references(AbstractRefProcTaskExecutor* task_executor = NULL);

  // debugging
  void verify_no_references_recorded() PRODUCT_RETURN;
  static void verify();

  // clear the discovered lists (unlinking each entry).
  void clear_discovered_references() PRODUCT_RETURN;
};

// A utility class to temporarily mutate the span of the
// given ReferenceProcessor in the scope that contains it.
class ReferenceProcessorSpanMutator: StackObj {
 private:
  ReferenceProcessor* _rp;
  MemRegion           _saved_span;

 public:
  ReferenceProcessorSpanMutator(ReferenceProcessor* rp,
                                MemRegion span):
    _rp(rp) {
    _saved_span = _rp->span();
    _rp->set_span(span);
  }

  ~ReferenceProcessorSpanMutator() {
    _rp->set_span(_saved_span);
  }
};

// A utility class to temporarily change the MT'ness of
// reference discovery for the given ReferenceProcessor
// in the scope that contains it.
class ReferenceProcessorMTMutator: StackObj {
 private:
  ReferenceProcessor* _rp;
  bool                _saved_mt;

 public:
  ReferenceProcessorMTMutator(ReferenceProcessor* rp,
                              bool mt):
    _rp(rp) {
    _saved_mt = _rp->discovery_is_mt();
    _rp->set_mt_discovery(mt);
  }

  ~ReferenceProcessorMTMutator() {
    _rp->set_mt_discovery(_saved_mt);
  }
};


// A utility class to temporarily change the disposition
// of the "is_alive_non_header" closure field of the
// given ReferenceProcessor in the scope that contains it.
class ReferenceProcessorIsAliveMutator: StackObj {
 private:
  ReferenceProcessor* _rp;
  BoolObjectClosure*  _saved_cl;

 public:
  ReferenceProcessorIsAliveMutator(ReferenceProcessor* rp,
                                   BoolObjectClosure*  cl):
    _rp(rp) {
    _saved_cl = _rp->is_alive_non_header();
    _rp->set_is_alive_non_header(cl);
  }

  ~ReferenceProcessorIsAliveMutator() {
    _rp->set_is_alive_non_header(_saved_cl);
  }
};

// A utility class to temporarily change the disposition
// of the "discovery_is_atomic" field of the
// given ReferenceProcessor in the scope that contains it.
class ReferenceProcessorAtomicMutator: StackObj {
 private:
  ReferenceProcessor* _rp;
  bool                _saved_atomic_discovery;

 public:
  ReferenceProcessorAtomicMutator(ReferenceProcessor* rp,
                                  bool atomic):
    _rp(rp) {
    _saved_atomic_discovery = _rp->discovery_is_atomic();
    _rp->set_atomic_discovery(atomic);
  }

  ~ReferenceProcessorAtomicMutator() {
    _rp->set_atomic_discovery(_saved_atomic_discovery);
  }
};


// A utility class to temporarily change the MT processing
// disposition of the given ReferenceProcessor instance
// in the scope that contains it.
class ReferenceProcessorMTProcMutator: StackObj {
 private:
  ReferenceProcessor* _rp;
  bool  _saved_mt;

 public:
  ReferenceProcessorMTProcMutator(ReferenceProcessor* rp,
                                  bool mt):
    _rp(rp) {
    _saved_mt = _rp->processing_is_mt();
    _rp->set_mt_processing(mt);
  }

  ~ReferenceProcessorMTProcMutator() {
    _rp->set_mt_processing(_saved_mt);
  }
};


// This class is an interface used to implement task execution for the
// reference processing.
class AbstractRefProcTaskExecutor {
public:

  // Abstract tasks to execute.
  class ProcessTask;
  class EnqueueTask;
  
  // Executes a task using worker threads.  
  virtual void execute(ProcessTask& task) = 0;
  virtual void execute(EnqueueTask& task) = 0;
  
  // Switch to single threaded mode.
  virtual void set_single_threaded_mode() { };
};

// Abstract reference processing task to execute.
class AbstractRefProcTaskExecutor::ProcessTask {
protected:
  ProcessTask(ReferenceProcessor& ref_processor,
              DiscoveredList      refs_lists[],
              bool                marks_oops_alive)
    : _ref_processor(ref_processor),
      _refs_lists(refs_lists),
      _marks_oops_alive(marks_oops_alive)
  { }
    
public:
  virtual void work(unsigned int work_id, BoolObjectClosure& is_alive,
                    OopClosure& keep_alive,
                    VoidClosure& complete_gc) = 0;
    
  // Returns true if a task marks some oops as alive.
  bool marks_oops_alive() const
  { return _marks_oops_alive; }
    
protected:
  ReferenceProcessor& _ref_processor;
  DiscoveredList*     _refs_lists;
  const bool          _marks_oops_alive;
};

// Abstract reference processing task to execute.
class AbstractRefProcTaskExecutor::EnqueueTask {
protected:
  EnqueueTask(ReferenceProcessor& ref_processor,
              DiscoveredList      refs_lists[],
              objectRef*          pending_list_addr,
              objectRef           sentinel_ref, 
              int                 n_queues)
    : _ref_processor(ref_processor),
      _refs_lists(refs_lists),
      _pending_list_addr(pending_list_addr),
      _sentinel_ref(sentinel_ref),
      _n_queues(n_queues)
  { }
    
public:
  virtual void work(unsigned int work_id) = 0;
    
protected:
  ReferenceProcessor& _ref_processor;
  DiscoveredList*     _refs_lists;
  objectRef*          _pending_list_addr;
  objectRef           _sentinel_ref; 
  int                 _n_queues;
};

#endif // REFERENCEPROCESSOR_HPP
