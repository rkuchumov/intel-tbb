/*
    Copyright 2005-2012 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

// TODO: rework as non-CPF or make two tests out of it
#define TBB_PREVIEW_LOCAL_OBSERVER 1
#if __TBB_CPF_BUILD
    #define TBB_PREVIEW_TASK_ARENA 1
#endif

#include "tbb/tbb_config.h"
#include "harness.h"

#if __TBB_SCHEDULER_OBSERVER
#include "tbb/task_scheduler_observer.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/atomic.h"
#include "tbb/task.h"
#include "../tbb/tls.h"
#include "tbb/tick_count.h"
#include "harness_barrier.h"

#if _MSC_VER && __TBB_NO_IMPLICIT_LINKAGE
// plays around __TBB_NO_IMPLICIT_LINKAGE. __TBB_LIB_NAME should be defined (in makefiles)
    #pragma comment(lib, __TBB_STRING(__TBB_LIB_NAME))
#endif

const int MaxFlagIndex = sizeof(uintptr_t)*8-1;

struct ObserverStats {
    tbb::atomic<int> m_entries;
    tbb::atomic<int> m_exits;
    tbb::atomic<int> m_workerEntries;
    tbb::atomic<int> m_workerExits;

    void Reset () {
        m_entries = m_exits = m_workerEntries = m_workerExits = 0;
    }

    void operator += ( const ObserverStats& s ) {
        m_entries += s.m_entries;
        m_exits += s.m_exits;
        m_workerEntries += s.m_workerEntries;
        m_workerExits += s.m_workerExits;
    }
};

struct ThreadState {
    uintptr_t m_flags;
    tbb::task_scheduler_observer *m_dyingObserver;
    bool m_isMaster;
    ThreadState() : m_flags(0), m_dyingObserver(NULL), m_isMaster(false) {}
};

tbb::internal::tls<ThreadState*> theLocalState;
static ObserverStats theStats;
static tbb::atomic<int> theNumObservers;

const int P = min( tbb::task_scheduler_init::default_num_threads(), (int)sizeof(int) * CHAR_BIT );

enum TestMode {
    //! Ensure timely workers destruction in order to guarantee all exit notification are fired.
    tmSynchronized = 1,
    //! Use local observer.
    tmLocalObservation = 2,
    //! Observer causes autoinitialization of the scheduler
    tmAutoinitialization = 4,
    //! test on_scheduler_leaving
    tmLeavingControl = 8
};

uintptr_t theTestMode,
          thePrevMode = 0;

class MyObserver : public tbb::task_scheduler_observer, public ObserverStats {
    uintptr_t m_flag;
    bool m_dying;
    tbb::atomic<int> m_leave_ticket;

    /*override*/
    void on_scheduler_entry( bool is_worker ) {
        ThreadState& state = *theLocalState;
        ASSERT( is_worker==!state.m_isMaster, NULL );
        if ( thePrevMode & tmSynchronized ) {
            ASSERT( !(state.m_flags & m_flag), "Observer repeatedly invoked for the same thread" );
            if ( theTestMode & tmLocalObservation )
                ASSERT( !state.m_flags, "Observer locality breached" );
        }
        if ( m_dying && theTestMode & tmLocalObservation ) {
            // In case of local observation a worker may enter the arena after
            // the wait for lagging on_entry calls in the MyObserver destructor
            // succeeds but before its base class tbb::task_scheduler_observer
            // destructor removes it from the internal list maintained by the
            // task scheduler. This will result in on_entry notification without,
            // subsequent on_exit as the observer is likely to be destroyed before
            // the worker discovers that the arena is empty and leaves it.
            //
            // To prevent statistics distortion, ignore the notifications for
            // observers about to be destroyed.
            ASSERT( !state.m_dyingObserver || state.m_dyingObserver != this || thePrevMode & tmSynchronized, NULL );
            state.m_dyingObserver = this;
            return;
        }
        state.m_dyingObserver = NULL;
        ++m_entries;
        state.m_flags |= m_flag;
        if ( is_worker )
            ++m_workerEntries;
    }
    /*override*/
    void on_scheduler_exit( bool is_worker ) {
        ThreadState& state = *theLocalState;
        ASSERT( is_worker==!state.m_isMaster, NULL );
        if ( m_dying && state.m_dyingObserver ) {
            ASSERT( state.m_dyingObserver == this, "Exit without entry (for a dying observer)" );
            state.m_dyingObserver = NULL;
            return;
        }
        ASSERT( state.m_flags & m_flag, "Exit without entry" );
        // TODO: if(!is_leaving_test()) // workers are not supposed to return back for leaving test
        state.m_flags &= ~m_flag;
        ++m_exits;
        if ( is_worker )
            ++m_workerExits;
    }
    /*override*/
    bool on_scheduler_leaving() {
        if( m_leave_ticket == 0 ) return true;
        return m_leave_ticket.fetch_and_store(-1) > 0;
    }
public:
    void test_leaving() { m_leave_ticket.store<tbb::relaxed>(-1); }
    void dismiss_one() { m_leave_ticket = 1; }
    bool is_leaving_test() { return m_leave_ticket.load<tbb::relaxed>() != 0; }

    MyObserver( uintptr_t flag )
        : tbb::task_scheduler_observer(theTestMode & tmLocalObservation ? true : false)
        , m_flag(flag)
        , m_dying(false)
    {
        m_leave_ticket.store<tbb::relaxed>(0);
        ++theNumObservers;
        Reset();
        // Local observer causes automatic scheduler initialization
        // in the current thread, so here, we must postpone the activation.
        if ( !(theTestMode & tmLocalObservation) )
            observe(true);
    }

    ~MyObserver () {
        m_dying = true;
        --theNumObservers;
        ASSERT( m_exits <= m_entries, NULL );
        if ( theTestMode & tmSynchronized ) {
            tbb::tick_count t0 = tbb::tick_count::now();
            while ( m_exits < m_entries && (tbb::tick_count::now() - t0).seconds() < 5 )
                Harness::Sleep(10);
            if ( m_exits < m_entries )
                REPORT( "Warning: Entry/exit count mismatch (%d, %d). Observer is broken or machine is overloaded.\n", (int)m_entries, (int)m_exits );
        }
        theStats += *this;
        // it is recommended to disable observation before destructor of the base class starts,
        // otherwise it can lead to concurrent notification callback on partly destroyed object,
        // which in turn can harm (in addition) if derived class has new virtual methods.
        // This class has no, and for test purposes we rely on implementation failsafe mechanism.
        //observe(false);
    }
}; // class MyObserver

Harness::SpinBarrier theGlobalBarrier;
bool theGlobalBarrierActive = true;

class FibTask : public tbb::task {
    const int N;
    uintptr_t m_flag;
    MyObserver &m_observer;
public:
    FibTask( int n, uintptr_t flags, MyObserver &obs ) : N(n), m_flag(flags), m_observer(obs) {}

    /*override*/ tbb::task* execute() {
        ASSERT( !(~theLocalState->m_flags & m_flag), NULL );
        if( N < 2 )
            return NULL;
        bool globalBarrierActive = false;
        if ( theLocalState->m_isMaster ) {
            if ( theGlobalBarrierActive ) {
                // This is the root task. Its N is equal to the number of threads.
                // Spawn a task for each worker.
                set_ref_count(N);
                for ( int i = 1; i < N; ++i )
                    spawn( *new( allocate_child() ) FibTask(20, m_flag, m_observer) );
                if ( theTestMode & tmSynchronized ) {
                    theGlobalBarrier.wait();
                    ASSERT( m_observer.m_entries >= N, "Wrong number of on_entry calls after the first barrier" );
                    // All the spawned tasks have been stolen by workers.
                    // Now wait for workers to spawn some more tasks for this thread to steal back.
                    theGlobalBarrier.wait();
                    ASSERT( !theGlobalBarrierActive, "Workers are expected to have reset this flag" );
                }
                else
                    theGlobalBarrierActive = false;
                wait_for_all();
                return NULL;
            }
        }
        else {
            if ( theGlobalBarrierActive ) {
                if ( theTestMode & tmSynchronized ) {
                    theGlobalBarrier.wait();
                    globalBarrierActive = true;
                }
                theGlobalBarrierActive = false;
            }
        }
        set_ref_count(3);
        spawn( *new( allocate_child() ) FibTask(N-1, m_flag, m_observer) );
        spawn( *new( allocate_child() ) FibTask(N-2, m_flag, m_observer) );
        if ( globalBarrierActive ) {
            // It's the first task executed by a worker. Release the master thread.
            theGlobalBarrier.wait();
        }
        wait_for_all();
        return NULL;
    }
}; // class FibTask

Harness::SpinBarrier theMasterBarrier;

class TestBody {
    int m_numThreads;
public:
    TestBody( int numThreads ) : m_numThreads(numThreads) {}

    void operator()( int i ) const {
        theLocalState->m_isMaster = true;
        uintptr_t f = i <= MaxFlagIndex ? 1<<i : 0;
        MyObserver o(f);
        if ( theTestMode & tmSynchronized )
            theMasterBarrier.wait();
        // when mode is local observation but not synchronized and when num threads == default
        if ( theTestMode & tmAutoinitialization )
            o.observe(true); // test autoinitialization can be done by observer
        // when mode is local synchronized observation and when num threads == default
        if ( theTestMode & tmLeavingControl )
            o.test_leaving();
        // Observer in enabled state must outlive the scheduler to ensure that
        // all exit notifications are called.
        tbb::task_scheduler_init init(m_numThreads);
        // when local & non-autoinitialized observation mode
        if ( theTestMode & tmLocalObservation )
            o.observe(true);
        for ( int j = 0; j < 2; ++j ) {
            tbb::task &t = *new( tbb::task::allocate_root() ) FibTask(m_numThreads, f, o);
            tbb::task::spawn_root_and_wait(t);
            thePrevMode = theTestMode;
        }
        if( o.is_leaving_test() ) {
            REMARK( "Testing on_scheduler_leaving()\n");
            ASSERT(o.m_workerEntries > 0, "Unbelievable");
            // TODO: start from 0?
            for ( int j = o.m_workerExits; j < o.m_workerEntries; j++ ) {
                REMARK( "Round %d: entries %d, exits %d\n", j, (int)o.m_workerEntries, (int)o.m_workerExits );
                ASSERT_WARNING(o.m_workerExits == j, "Workers unexpectedly leaved arena");
                o.dismiss_one();
                double n_seconds = 5;
                (Harness::TimedWaitWhileEq(n_seconds))(o.m_workerExits, j);
                ASSERT( n_seconds >= 0, "Time out while waiting for a worker to leave arena");
                __TBB_Yield();
            }
        }
    }
}; // class TestBody

Harness::SpinBarrier theWorkersBarrier;

class CleanerTask : public tbb::task {
public:
    tbb::task* execute () {
        theLocalState->m_flags = 0;
        theWorkersBarrier.wait();
        return NULL;
    }
};

void CleanLocalState () {
    tbb::task_scheduler_init init;
    tbb::task &r = *new( tbb::task::allocate_root() ) tbb::empty_task;
    r.set_ref_count(P + 1);
    for ( int i = 1; i < P; ++i )
        tbb::task::spawn( *new(r.allocate_child()) CleanerTask );
    r.spawn_and_wait_for_all( *new(r.allocate_child()) CleanerTask );
    tbb::task::destroy( r );
}

void TestObserver( int M, int T, uintptr_t testMode ) {
    theStats.Reset();
    theGlobalBarrierActive = true;
    theTestMode = testMode;
    NativeParallelFor( M, TestBody(T) );
    // When T (number of threads in arena, i.e. master + workers) is less than P
    // (hardware concurrency), more than T-1 workers can visit the same arena. This 
    // is possible in case of imbalance or when other arenas are activated/deactivated
    // concurrently).
    ASSERT( !theNumObservers, "Unexpected alive observer(s)" );
    REMARK( "Entries %d / %d, exits %d\n", (int)theStats.m_entries, (int)theStats.m_workerEntries, (int)theStats.m_exits );
    if ( testMode & tmSynchronized ) {
        if ( testMode & tmLocalObservation ) {
            ASSERT( theStats.m_entries >= M * T, "Too few on_entry calls" );
            ASSERT( theStats.m_workerEntries >= M * (T - 1), "Too few worker entries" );
        }
        else {
            ASSERT( theStats.m_entries >= M * M * T, "Too few on_entry calls" );
            ASSERT( theStats.m_entries <= M * (P + 1), "Too many on_entry calls" );
            ASSERT( theStats.m_workerEntries >= M * M * (T - 1), "Too few worker entries" );
            ASSERT( theStats.m_workerEntries <= M * (P - 1), "Too many worker entries" );
        }
        ASSERT( theStats.m_entries == theStats.m_exits, "Entries/exits mismatch" );
    }
    else {
        ASSERT( theStats.m_entries >= M, "Too few on_entry calls" );
        ASSERT( theStats.m_exits >= M || (testMode & tmAutoinitialization), "Too few on_exit calls" );
        if ( !(testMode & tmLocalObservation) ) {
            ASSERT( theStats.m_entries <= M * M * P, "Too many on_entry calls" );
            ASSERT( theStats.m_exits <= M * M * T, "Too many on_exit calls" );
        }
        ASSERT( theStats.m_entries >= theStats.m_exits, "More exits than entries" );
    }
}

int TestMain () {
    if ( P < 2 )
        return Harness::Skipped;
    theNumObservers = 0;
    theWorkersBarrier.initialize(P);
    // Fully- and under-utilized mode
    for ( int M = 1; M < P; M <<= 1 ) {
        if ( M > P/2 ) {
            ASSERT( P & (P-1), "Can get here only in case of non power of two cores" );
            M = P/2;
            if ( M & (M-1) )
                break; // Already tested this configuration
        }
        int T = P / M;
        ASSERT( T > 1, NULL );
        REMARK( "Masters: %d; Arena size: %d\n", M, T );
        theMasterBarrier.initialize(M);
        theGlobalBarrier.initialize(M * T);
        TestObserver(M, T, 0);
        TestObserver(M, T, tmLocalObservation | ( T==P? tmAutoinitialization : 0) );
        CleanLocalState();
        TestObserver(M, T, tmSynchronized);
        TestObserver(M, T, tmSynchronized | tmLocalObservation
#if __TBB_TASK_ARENA
                     | ( T==P? tmLeavingControl : 0)
#endif
                     );
    }
    // Oversubscribed mode
    for ( int i = 0; i < 5; ++i ) {
        REMARK( "Masters: %d; Arena size: %d\n", P-1, P );
        TestObserver(P-1, P, 0);
        TestObserver(P-1, P, tmLocalObservation);
    }
    Harness::Sleep(20);
    return Harness::Done;
}

#else /* !__TBB_SCHEDULER_OBSERVER */

int TestMain () {
    return Harness::Skipped;
}
#endif /* !__TBB_SCHEDULER_OBSERVER */
