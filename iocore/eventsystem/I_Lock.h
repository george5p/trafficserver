/** @file

  Basic locks for threads

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#ifndef _I_Lock_h_
#define _I_Lock_h_

#include "inktomi++.h"
#include "I_Thread.h"

#define MAX_LOCK_TIME	HRTIME_MSECONDS(200)
#define THREAD_MUTEX_THREAD_HOLDING	(-1024*1024)
#define HANDLER_NAME(_c) _c?_c->handler_name:(char*)NULL

class EThread;
typedef EThread *EThreadPtr;
typedef volatile EThreadPtr VolatileEThreadPtr;

inkcoreapi extern void lock_waiting(const char *file, int line, const char *handler);
inkcoreapi extern void lock_holding(const char *file, int line, const char *handler);
extern void lock_taken(const char *file, int line, const char *handler);

/**
  Lock object used in continuations and threads.

  The ProxyMutex class is the main synchronization object used
  throughout the Event System. It is a reference counted object
  that provides mutually exclusive access to a resource. Since the
  Event System is multithreaded by design, the ProxyMutex is required
  to protect data structures and state information that could
  otherwise be affected by the action of concurrent threads.

  A ProxyMutex object has an ink_mutex member (defined in ink_mutex.h)
  which is a wrapper around the platform dependent mutex type. This
  member allows the ProxyMutex to provide the functionallity required
  by the users of the class without the burden of platform specific
  function calls.

  The ProxyMutex also has a reference to the current EThread holding
  the lock as a back pointer for verifying that it is released
  correctly.

  Acquiring/Releasing locks:

  Included with the ProxyMutex class, there are several macros that
  allow you to lock/unlock the underlying mutex object.

*/
class ProxyMutex:public RefCountObj
{
public:

  /**
    Underlying mutex object.

    The platform independent mutex for the ProxyMutex class. You
    must not modify or set it directly.

  */
  // coverity[uninit_member]
  ink_mutex the_mutex;

  /**
    Backpointer to owning thread.

    This is a pointer to the thread currently holding the mutex
    lock.  You must not modify or set this value directly.

  */
  volatile EThreadPtr thread_holding;

  int nthread_holding;

#ifdef DEBUG
  ink_hrtime hold_time;
  const char *file;
  int line;
  const char *handler;

#  ifdef MAX_LOCK_TAKEN
  int taken;
#  endif                        //MAX_LOCK_TAKEN

#  ifdef LOCK_CONTENTION_PROFILING
  int total_acquires, blocking_acquires,
    nonblocking_acquires, successful_nonblocking_acquires, unsuccessful_nonblocking_acquires;
  void print_lock_stats(int flag);
#  endif                        //LOCK_CONTENTION_PROFILING
#endif                          //DEBUG
  void free();

  /**
    Constructor - use new_ProxyMutex() instead.

    The constructor of a ProxyMutex object. Initializes the state
    of the object but leaves the initialization of the mutex member
    until it is needed (through init()). Do not use this constructor,
    the preferred mechanism for creating a ProxyMutex is via the
    new_ProxyMutex function, which provides a faster allocation.

  */
    ProxyMutex()
  {
    thread_holding = NULL;
    nthread_holding = 0;
#ifdef DEBUG
    hold_time = 0;
    file = NULL;
    line = 0;
    handler = NULL;
#  ifdef MAX_LOCK_TAKEN
    taken = 0;
#  endif                        //MAX_LOCK_TAKEN
#  ifdef LOCK_CONTENTION_PROFILING
    total_acquires = 0;
    blocking_acquires = 0;
    nonblocking_acquires = 0;
    successful_nonblocking_acquires = 0;
    unsuccessful_nonblocking_acquires = 0;
#  endif                        //LOCK_CONTENTION_PROFILING
#endif                          //DEBUG
    // coverity[uninit_member]
  }

  /**
    Initializes the underlying mutex object.

    After constructing your ProxyMutex object, use this function
    to initialize the underlying mutex object with an optional name.

    @param name Name to identify this ProxyMutex. Its use depends
      on the given platform.

  */
  void init(const char *name = "UnnamedMutex") {
    ink_mutex_init(&the_mutex, name);
  }

  bool is_thread()
  {
    return false;
  }
};

// The ClassAlocator for ProxyMutexes
extern inkcoreapi ClassAllocator<ProxyMutex> mutexAllocator;

/**
  Smart pointer to a ProxyMutex object.

  Given that the ProxyMutex object incorporates reference counting
  functionality, the ProxyMutexPtr provides a "smart pointer"
  interface for using references of the ProxyMutex class. Internally,
  it takes care of updating the object's reference count and to
  free it once that value drops to zero.

*/
class ProxyMutexPtr
{
public:

  /**
    Constructor of the ProxyMutexPtr class. You can provide a pointer
    to a ProxyMutex object or set it at a later time with the
    assignment operator.

  */
ProxyMutexPtr(ProxyMutex * ptr = 0):m_ptr(ptr) {
    if (m_ptr)
      REF_COUNT_OBJ_REFCOUNT_INC(m_ptr);
  }

  /**
    Copy constructor.

    Constructs a ProxyMutexPtr object from a constant reference to
    other. If the object to initialize from has a reference to a
    valid ProxyMutex object its reference count is incremented.

    @param src Constant ref to the ProxyMutexPtr to initialize from.

  */
  ProxyMutexPtr(const ProxyMutexPtr & src):m_ptr(src.m_ptr)
  {
    if (m_ptr)
      REF_COUNT_OBJ_REFCOUNT_INC(m_ptr);
  }

  /**
    The destructor of a ProxyMutexPtr. It will free the ProxyMutex
    object pointed at by this instance only if this is the last
    reference to the object (reference count equal to zero).

  */
  ~ProxyMutexPtr() {
    if (m_ptr && !m_ptr->refcount_dec())
      m_ptr->free();
  }

  /**
    Assignment operator using a ProxyMutex object.

    Sets the ProxyMutex object to which this ProxyMutexPtr must
    point at. Since the reference to the ProxyMutex is replaced,
    the reference count for the previous object (if any) is decremented
    and the object is freed if it drops to zero.

    @param p The reference to the ProxyMutex object.

  */
  ProxyMutexPtr & operator =(ProxyMutex * p) {
    ProxyMutex *temp_ptr = m_ptr;
    if (m_ptr == p)
      return (*this);
    m_ptr = p;
    if (m_ptr)
      m_ptr->refcount_inc();
    if (temp_ptr && REF_COUNT_OBJ_REFCOUNT_DEC(((RefCountObj *) temp_ptr)) == 0)
      ((RefCountObj *) temp_ptr)->free();
    return (*this);
  }

  /**
    Assignment operator using another ProxyMutexPtr object.

    Sets the ProxyMutex object to which this ProxyMutexPtr must
    point at to that of the 'src'. Since the reference to the
    ProxyMutex is replaced, the reference count for the previous
    object (if any) is decremented and the object is freed if it
    drops to zero.

    @param src The ProxyMutexPtr to get the ProxyMutex reference from.

  */
  ProxyMutexPtr & operator =(const ProxyMutexPtr & src) {
    return (operator =(src.m_ptr));
  }

  /**
    Disassociates this object's reference to the ProxyMutex object

    This method verifies that the ProxyMutexPtr object no longer
    references a ProxyMutex object. If it does, the ProxyMutex's
    reference count is decremented and the object is freed if it
    drops to zero.

  */
  void clear()
  {
    if (m_ptr) {
      if (!((RefCountObj *) m_ptr)->refcount_dec())
        ((RefCountObj *) m_ptr)->free();
      m_ptr = NULL;
    }
  }

  // TODO eval if cast op needs to be removed
  // The casting operators do not fair well together with comparison
  // operators in many cases, gcc-3.4.x isn't too happy about it. It'd
  // probably be safer to get rid of them entirely.
  /**
    Cast operator.

    When the cast (ProxyMutex*) is applied to a ProxyMutexPtr object,
    a pointer to the managed ProxyMutex object is returned.

  */
  operator  ProxyMutex *() const
  {
    return (m_ptr);
  }

  /**
    Pointer member selection operator.

    When applied to a ProxyMutexPtr object, it is applied to the
    underlying ProxyMutex object. Make sure this ProxyMutexPtr
    object has a valid reference to a ProxyMutex before using it.

  */
  ProxyMutex *operator ->() const
  {
    return (m_ptr);
  }

  /**
    Dereference operator.

    Provides access to the ProxyMutex object pointed at by this
    ProxyMutexPtr.

  */
  ProxyMutex & operator *() const
  {
    return (*m_ptr);
  }

  /**
    Equality operator.

    Returns true if the two pointers are equivalent (the rhs and
    the one stored inside the lhs ProxyMutexPtr object), otherwise
    false.

  */
  int operator ==(const ProxyMutex * p) const
  {
    return (m_ptr == p);
  }

  /**
    Equality operator.

    Returns true if the pointers managed by the rhs and lhs
    ProxyMutexPtr objects are equivalent.

  */
  int operator ==(const ProxyMutexPtr & p) const
  {
    return (m_ptr == p.m_ptr);
  }

  /**
    Inequality operator.

    Returns true if the two pointers are not equivalent (the rhs
    and the one stored inside the lhs ProxyMutexPtr object), otherwise
    false.

  */
  int operator !=(const ProxyMutex * p) const
  {
    return (m_ptr != p);
  }

  /**
    Inequality operator.

    Returns true if the pointers managed by the rhs and lhs
    ProxyMutexPtr objects are not equivalent.

  */
  int operator !=(const ProxyMutexPtr & p) const
  {
    return (m_ptr != p.m_ptr);
  }

  RefCountObj *_ptr()
  {
    return (RefCountObj *) m_ptr;
  }

  /**
    Pointer to the specified ProxyMutex.

    This is the pointer that stores the specified ProxyMutex. Do
    not set or modify its value directly.

  */
  ProxyMutex *m_ptr;
};

inline bool
Mutex_trylock(
#ifdef DEBUG
               const char *afile, int aline, const char *ahandler,
#endif
               ProxyMutex * m, EThread * t)
{

  ink_assert(t != 0);
  if (m->thread_holding != t) {
    if (!ink_mutex_try_acquire(&m->the_mutex)) {
#ifdef DEBUG
      lock_waiting(m->file, m->line, m->handler);
#ifdef LOCK_CONTENTION_PROFILING
      m->unsuccessful_nonblocking_acquires++;
      m->nonblocking_acquires++;
      m->total_acquires++;
      m->print_lock_stats(0);
#endif //LOCK_CONTENTION_PROFILING
#endif //DEBUG
      return false;
    }
    m->thread_holding = t;
    ink_assert(m->thread_holding);
#ifdef DEBUG
    m->file = afile;
    m->line = aline;
    m->handler = ahandler;
    m->hold_time = ink_get_hrtime();
#ifdef MAX_LOCK_TAKEN
    m->taken++;
#endif //MAX_LOCK_TAKEN
#endif //DEBUG
  }
#ifdef DEBUG
#ifdef LOCK_CONTENTION_PROFILING
  m->successful_nonblocking_acquires++;
  m->nonblocking_acquires++;
  m->total_acquires++;
  m->print_lock_stats(0);
#endif //LOCK_CONTENTION_PROFILING
#endif //DEBUG
  m->nthread_holding++;
  SPILL_ALL_VOLATILES;
  return true;
}

inline bool
Mutex_trylock_spin(
#ifdef DEBUG
                    const char *afile, int aline, const char *ahandler,
#endif
                    ProxyMutex * m, EThread * t, int spincnt = 1)
{

  ink_assert(t != 0);
  if (m->thread_holding != t) {
    int locked;
    do {
      if ((locked = ink_mutex_try_acquire(&m->the_mutex)))
        break;
    } while (--spincnt);
    if (!locked) {
#ifdef DEBUG
      lock_waiting(m->file, m->line, m->handler);
#ifdef LOCK_CONTENTION_PROFILING
      m->unsuccessful_nonblocking_acquires++;
      m->nonblocking_acquires++;
      m->total_acquires++;
      m->print_lock_stats(0);
#endif //LOCK_CONTENTION_PROFILING
#endif //DEBUG
      return false;
    }
    m->thread_holding = t;
    ink_debug_assert(m->thread_holding);
#ifdef DEBUG
    m->file = afile;
    m->line = aline;
    m->handler = ahandler;
    m->hold_time = ink_get_hrtime();
#ifdef MAX_LOCK_TAKEN
    m->taken++;
#endif //MAX_LOCK_TAKEN
#endif //DEBUG
  }
#ifdef DEBUG
#ifdef LOCK_CONTENTION_PROFILING
  m->successful_nonblocking_acquires++;
  m->nonblocking_acquires++;
  m->total_acquires++;
  m->print_lock_stats(0);
#endif //LOCK_CONTENTION_PROFILING
#endif //DEBUG
  m->nthread_holding++;
  SPILL_ALL_VOLATILES;
  return true;
}

inline int
Mutex_lock(
#ifdef DEBUG
            const char *afile, int aline, const char *ahandler,
#endif
            ProxyMutex * m, EThread * t)
{

  ink_assert(t != 0);
  if (m->thread_holding != t) {
    ink_mutex_acquire(&m->the_mutex);
    m->thread_holding = t;
    ink_debug_assert(m->thread_holding);
#ifdef DEBUG
    m->file = afile;
    m->line = aline;
    m->handler = ahandler;
    m->hold_time = ink_get_hrtime();
#ifdef MAX_LOCK_TAKEN
    m->taken++;
#endif //MAX_LOCK_TAKEN
#endif //DEBUG
  }
#ifdef DEBUG
#ifdef LOCK_CONTENTION_PROFILING
  m->blocking_acquires++;
  m->total_acquires++;
  m->print_lock_stats(0);
#endif // LOCK_CONTENTION_PROFILING
#endif //DEBUG
  m->nthread_holding++;
  SPILL_ALL_VOLATILES;
  return true;
}

inline void
Mutex_unlock(ProxyMutex * m, EThread * t)
{
  if (m->nthread_holding) {
    ink_assert(t == m->thread_holding);
    m->nthread_holding--;
    if (!m->nthread_holding) {
#ifdef DEBUG
      if (ink_get_hrtime() - m->hold_time > MAX_LOCK_TIME)
        lock_holding(m->file, m->line, m->handler);
#ifdef MAX_LOCK_TAKEN
      if (m->taken > MAX_LOCK_TAKEN)
        lock_taken(m->file, m->line, m->handler);
#endif //MAX_LOCK_TAKEN
      m->file = NULL;
      m->line = 0;
      m->handler = NULL;
#endif //DEBUG
      ink_debug_assert(m->thread_holding);
      m->thread_holding = 0;
      ink_mutex_release(&m->the_mutex);
    }
  }
  SPILL_ALL_VOLATILES;
}

struct MutexLock
{
  ProxyMutexPtr m;
    MutexLock():m(NULL)
  {
  };

  MutexLock(
#ifdef DEBUG
             const char *afile, int aline, const char *ahandler,
#endif                          //DEBUG
             ProxyMutex * am, EThread * t):m(am)
  {

    Mutex_lock(
#ifdef DEBUG
                afile, aline, ahandler,
#endif //DEBUG
                m, t);
  }

  void set_and_take(
#ifdef DEBUG
                     const char *afile, int aline, const char *ahandler,
#endif                          //DEBUG
                     ProxyMutex * am, EThread * t)
  {

    m = am;
    Mutex_lock(
#ifdef DEBUG
                afile, aline, ahandler,
#endif //DEBUG
                m, t);
  }

  void release()
  {
    if (m)
      Mutex_unlock(m, m->thread_holding);
    m.clear();
  }

  ~MutexLock() {
    if (m)
      Mutex_unlock(m, m->thread_holding);
  }

  int operator!()
  {
    return false;
  }
  operator  bool()
  {
    return true;
  }
};

struct MutexTryLock
{
  ProxyMutexPtr m;
  volatile bool lock_acquired;

    MutexTryLock(
#ifdef DEBUG
                  const char *afile, int aline, const char *ahandler,
#endif                          //DEBUG
                  ProxyMutex * am, EThread * t)
  {
      lock_acquired = Mutex_trylock(
#ifdef DEBUG
                                     afile, aline, ahandler,
#endif //DEBUG
                                     am, t);
      if (lock_acquired)
        m = am;
  }

  MutexTryLock(
#ifdef DEBUG
                const char *afile, int aline, const char *ahandler,
#endif                          //DEBUG
                ProxyMutex * am, EThread * t, int sp)
  {
      lock_acquired = Mutex_trylock_spin(
#ifdef DEBUG
                                          afile, aline, ahandler,
#endif //DEBUG
                                          am, t, sp);
      if (lock_acquired)
        m = am;
  }

  ~MutexTryLock() {
    if (m.m_ptr)
      Mutex_unlock(m.m_ptr, m.m_ptr->thread_holding);
  }

  void release()
  {
    if (m.m_ptr) {
      Mutex_unlock(m.m_ptr, m.m_ptr->thread_holding);
      m.clear();
    }
    lock_acquired = false;
  }

  int operator!()
  {
    return !lock_acquired;
  }
  operator  bool()
  {
    return lock_acquired;
  }
};

inline void
ProxyMutex::free()
{
#ifdef DEBUG
#ifdef LOCK_CONTENTION_PROFILING
  print_lock_stats(1);
#endif
#endif
  ink_mutex_destroy(&the_mutex);
  mutexAllocator.free(this);
}

// TODO should take optional mutex "name" identifier, to pass along to the init() fun
/**
  Creates a new ProxyMutex object.

  This is the preferred mechanism for constructing objects of the
  ProxyMutex class. It provides you with faster allocation than
  that of the normal constructor.

  @return A pointer to a ProxyMutex object appropriate for the build
    environment.

*/
inline ProxyMutex *
new_ProxyMutex()
{
  ProxyMutex *m = mutexAllocator.alloc();
  m->init();
  return m;
}

/*------------------------------------------------------*\
|  Macros                                                |
\*------------------------------------------------------*/

/**
  Blocks until the lock to the ProxyMutex is acquired.

  This macro performs a blocking call until the lock to the ProxyMutex
  is acquired. This call allocates a special object that holds the
  lock to the ProxyMutex only for the scope of the function or
  region. It is a good practice to delimit such scope explicitly
  with '&#123;' and '&#125;'.

  @param _l Arbitrary name for the lock to use in this call
  @param _m A pointer to (or address of) a ProxyMutex object
  @param _t The current EThread executing your code.

*/
#  ifdef DEBUG
#    define MUTEX_LOCK(_l,_m,_t) MutexLock _l(__FILE__,__LINE__,NULL,_m,_t)
#  else
#    define MUTEX_LOCK(_l,_m,_t) MutexLock _l(_m,_t)
#  endif //DEBUG

#  ifdef DEBUG
/**
  Attempts to acquire the lock to the ProxyMutex.

  This macro attempts to acquire the lock to the specified ProxyMutex
  object in a non-blocking manner. After using the macro you can
  see if it was successful by comparing the lock variable with true
  or false (the variable name passed in the _l parameter).

  @param _l Arbitrary name for the lock to use in this call (lock variable)
  @param _m A pointer to (or address of) a ProxyMutex object
  @param _t The current EThread executing your code.

*/
#    define MUTEX_TRY_LOCK(_l,_m,_t) \
MutexTryLock _l(__FILE__,__LINE__,(char*)NULL,_m,_t)

/**
  Attempts to acquire the lock to the ProxyMutex.

  This macro performs up to the specified number of attempts to
  acquire the lock on the ProxyMutex object. It does so by running
  a busy loop (busy wait) '_sc' times. You should use it with care
  since it blocks the thread during that time and wastes CPU time.

  @param _l Arbitrary name for the lock to use in this call (lock variable)
  @param _m A pointer to (or address of) a ProxyMutex object
  @param _t The current EThread executing your code.
  @param _sc The number of attempts or spin count. It must be a positive value.

*/
#    define MUTEX_TRY_LOCK_SPIN(_l,_m,_t,_sc) \
MutexTryLock _l(__FILE__,__LINE__,(char*)NULL,_m,_t,_sc)

#    ifdef PURIFY

/**
  Attempts to acquire the lock to the ProxyMutex.

  This macro attempts to acquire the lock to the specified ProxyMutex
  object in a non-blocking manner. After using the macro you can
  see if it was successful by comparing the lock variable with true
  or false (the variable name passed in the _l parameter).

  @param _l Arbitrary name for the lock to use in this call (lock variable)
  @param _m A pointer to (or address of) a ProxyMutex object
  @param _t The current EThread executing your code.
  @param _c Continuation whose mutex will be attempted to lock.

*/
#      define MUTEX_TRY_LOCK_FOR(_l,_m,_t,_c) \
MutexTryLock _l(__FILE__,__LINE__,(char *)NULL,_m,_t)
#    else //PURIFY
#      define MUTEX_TRY_LOCK_FOR(_l,_m,_t,_c) \
MutexTryLock _l(__FILE__,__LINE__,HANDLER_NAME(_c),_m,_t)
#    endif //PURIFY
#  else //DEBUG
#    define MUTEX_TRY_LOCK(_l,_m,_t) MutexTryLock _l(_m,_t)
#    define MUTEX_TRY_LOCK_SPIN(_l,_m,_t,_sc) MutexTryLock _l(_m,_t,_sc)
#    define MUTEX_TRY_LOCK_FOR(_l,_m,_t,_c) MutexTryLock _l(_m,_t)
#  endif //DEBUG

/**
  Releases the lock on a ProxyMutex.

  This macro releases the lock on the ProxyMutex, provided it is
  currently held. The lock must have been successfully acquired
  with one of the MUTEX macros.

  @param _l Arbitrary name for the lock to use in this call (lock
    variable) It must be the same name as the one used to acquire the
    lock.

*/
#  define MUTEX_RELEASE(_l) (_l).release()

/////////////////////////////////////
// DEPRECATED DEPRECATED DEPRECATED
#ifdef DEBUG
#  define MUTEX_TAKE_TRY_LOCK(_m,_t) \
Mutex_trylock(__FILE__,__LINE__,(char*)NULL,_m,_t)
#  ifdef PURIFY
#    define MUTEX_TAKE_TRY_LOCK_FOR(_m,_t,_c) \
Mutex_trylock(__FILE__,__LINE__,(char *)NULL,_m,_t)
#    define MUTEX_TAKE_TRY_LOCK_FOR_SPIN(_m,_t,_c,_sc) \
Mutex_trylock_spin(__FILE__,__LINE__,(char *)NULL,_m,_t,_sc)
#  else
#    define MUTEX_TAKE_TRY_LOCK_FOR(_m,_t,_c) \
Mutex_trylock(__FILE__,__LINE__,(char*)NULL,_m,_t)
#    define MUTEX_TAKE_TRY_LOCK_FOR_SPIN(_m,_t,_c,_sc) \
Mutex_trylock_spin(__FILE__,__LINE__,HANDLER_NAME(_c),_m,_t,_sc)
#  endif
#else
#  define MUTEX_TAKE_TRY_LOCK(_m,_t) Mutex_trylock(_m,_t)
#  define MUTEX_TAKE_TRY_LOCK_FOR(_m,_t,_c) Mutex_trylock(_m,_t)
#  define MUTEX_TAKE_TRY_LOCK_FOR_SPIN(_m,_t,_c,_sc) \
Mutex_trylock_spin(_m,_t,_sc)
#endif

#ifdef DEBUG
#  define MUTEX_TAKE_LOCK(_m,_t)\
Mutex_lock(__FILE__,__LINE__,(char*)NULL,_m,_t)
#  define MUTEX_SET_AND_TAKE_LOCK(_s,_m,_t)\
_s.set_and_take(__FILE__,__LINE__,(char*)NULL,_m,_t)
#  ifdef PURIFY
#    define MUTEX_TAKE_LOCK_FOR(_m,_t,_c) \
Mutex_lock(__FILE__,__LINE__,(char *)NULL,_m,_t)
#  else
#    define MUTEX_TAKE_LOCK_FOR(_m,_t,_c) \
Mutex_lock(__FILE__,__LINE__,HANDLER_NAME(_c),_m,_t)
#  endif //PURIFY
#else
#  define MUTEX_TAKE_LOCK(_m,_t) Mutex_lock(_m,_t)
#  define MUTEX_SET_AND_TAKE_LOCK(_s,_m,_t)_s.set_and_take(_m,_t)
#  define MUTEX_TAKE_LOCK_FOR(_m,_t,_c) Mutex_lock(_m,_t)
#endif //DEBUG

#define MUTEX_UNTAKE_LOCK(_m,_t) Mutex_unlock(_m,_t)
// DEPRECATED DEPRECATED DEPRECATED
/////////////////////////////////////

#endif // _Lock_h_
