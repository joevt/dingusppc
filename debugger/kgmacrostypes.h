/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef KGMACRO_TYPES_H_
#define KGMACRO_TYPES_H_

#ifdef __cplusplus
#define assert_size(typ, siz) static_assert(sizeof(typ) == siz)
#else
#define assert_size(typ, siz) _Static_assert(sizeof(typ) == siz)
#endif

#ifdef _MSC_VER
#define __TYPE_PACKED
typedef uint32_t __darwin_id_t;
typedef __darwin_id_t uid_t;
typedef __darwin_id_t pid_t;
#define __LITTLE_ENDIAN 1
#define __BIG_ENDIAN 0
#ifdef _M_PPCBE // xbox 360
#define __BYTE_ORDER __BIG_ENDIAN
#else
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif
#else
#define __TYPE_PACKED __attribute__((packed))
#endif

#define boolean_t kg_boolean_t
typedef uint32_t    kg_boolean_t;
typedef uint32_t    queue_entry_p;

typedef struct {
    queue_entry_p   next;
    queue_entry_p   prev;
} queue_chain_t;

typedef uint32_t    savearea_fpu_p;
typedef uint32_t    savearea_p;
typedef uint32_t    savearea_vec_p;
typedef uint32_t    thread_activation_p;

typedef struct {
    savearea_fpu_p      FPUsave;
    savearea_p          FPUlevel;
    uint32_t            FPUcpu;
    savearea_vec_p      VMXsave;
    savearea_p          VMXlevel;
    uint32_t            VMXcpu;
    thread_activation_p facAct;
} facility_context;

typedef uint32_t    facility_context_p;
typedef uint32_t    vmmCntrlEntry_p;
typedef uint32_t    vmmCntrlTable_p;

#ifdef _MSC_VER
#include <pshpack1.h>
#endif
typedef struct {
    savearea_p          pcb;
    facility_context_p  curctx;
    facility_context_p  deferctx;
    facility_context    facctx;
    vmmCntrlEntry_p     vmmCEntry;
    vmmCntrlTable_p     vmmControl;
    uint64_t            qactTimer __TYPE_PACKED;
    uint32_t            ksp;
    uint32_t            bbDescAddr;
    uint32_t            bbUserDA;
    uint32_t            bbTableStart;
    uint32_t            emPendRupts;
    uint32_t            bbTaskID;
    uint32_t            bbTaskEnv;
    uint32_t            specFlags;
    uint32_t            cthread_self;
} MachineThrAct;
#ifdef _MSC_VER
#include <poppack.h>
#endif
assert_size(MachineThrAct, 92);

typedef struct {
    int32_t lock_data;
} hw_lock_data_t;

typedef struct {
    hw_lock_data_t  interlock;
    hw_lock_data_t  locked;
    uint16_t        waiters;
    uint16_t        promoted_pri;
} mutex_t;

typedef uint32_t    void_function_p;
typedef uint32_t    ReturnHandler_p;

typedef struct {
    ReturnHandler_p next;
    void_function_p handler;
} ReturnHandler;

typedef uint32_t    ipc_port_p;
typedef uint32_t    vm_map_t;
typedef uint32_t    thread_state_flavor_t;
typedef uint32_t    exception_behavior_t;

typedef struct {
    ipc_port_p              port;
    thread_state_flavor_t   flavor;
    exception_behavior_t    behavior;
} exception_action;

typedef struct {
    queue_entry_p   next;
    queue_entry_p   prev;
} queue_head_t;

typedef uint32_t    task_p;
typedef uint32_t    thread_shuttle_p;
typedef uint32_t    ast_t;
typedef uint32_t    void_p;

typedef struct {
    // [ start of guest thread_activation
    queue_chain_t       thr_acts;
    boolean_t           kernel_loaded;
    boolean_t           kernel_loading;
    boolean_t           inited;
    MachineThrAct       mact;
    mutex_t             lock;
    hw_lock_data_t      sched_lock;
    int32_t             ref_count;
    task_p              task;
    vm_map_t            map;
    thread_shuttle_p    thread;
    thread_activation_p higher;
    thread_activation_p lower;
    uint32_t            alerts;
    uint32_t            alert_mask;
    int32_t             suspend_count;
    int32_t             user_stop_count;
    ast_t               ast;
    int32_t             active;
    ReturnHandler_p     handlers;
    ReturnHandler       special_handler;
    ipc_port_p          ith_self;
    ipc_port_p          ith_sself;
    exception_action    exc_actions[10];
    queue_head_t        held_ulocks;
    void_p              uthread;
    // ] end of guest thread_activation
    uint32_t thread_activation; // guest virtual address pointer to thread_activation
} thread_activation;
assert_size(thread_activation, 332); // 328 + 4 for the pointer that we added at the end.

typedef struct {
    uint32_t    low_bits;
    uint32_t    high_bits;
    uint32_t    high_bits_check;
    uint32_t    tstamp;
} timer_data_t;

typedef struct {
    uint32_t low;
    uint32_t high;
} timer_save_data_t;

typedef uint32_t    call_entry_func_t;
typedef uint32_t    call_entry_param_t;

#ifdef _MSC_VER
#include <pshpack1.h>
#endif
typedef struct {
    queue_chain_t       q_link;
    call_entry_func_t   func;
    call_entry_param_t  param0;
    call_entry_param_t  param1;
    uint64_t            deadline __TYPE_PACKED;
    uint32_t            state;
} timer_call_data_t;
#ifdef _MSC_VER
#include <poppack.h>
#endif
assert_size(timer_call_data_t, 32);

typedef uint32_t     ipc_kmsg_p;

typedef struct {
    ipc_kmsg_p ikmq_base;
} ipc_kmsg_queue;

typedef uint32_t    run_queue_t;
typedef uint32_t    wait_queue_t;
typedef uint64_t    event64_t;
typedef uint32_t    thread_act_t;
typedef uint32_t    wait_result_t;
typedef uint32_t    thread_roust_t;
typedef uint32_t    thread_continue_t;
typedef uint32_t    funnel_lock_p;
typedef uint32_t    vm_offset_t_32;
typedef int32_t     integer_t;
typedef uint32_t    mach_msg_return_t_32;
typedef uint32_t    ipc_object_t;
typedef uint32_t    mach_msg_header_t_32_p;
typedef uint32_t    mach_msg_size_t;
typedef uint32_t    mach_msg_option_t_32;
typedef uint32_t    mach_port_seqno_t;
typedef uint32_t    mach_msg_continue_t;
typedef uint32_t    mach_port_t;
typedef uint32_t    mach_msg_header_t_32_p;
typedef uint32_t    processor_set_t;
typedef uint32_t    processor_t;
typedef uint32_t    natural_t;
typedef uint32_t    semaphore_p;
typedef uint32_t    kern_return_t_32;
typedef uint32_t    mach_msg_return_t_32;

#ifdef _MSC_VER
#include <pshpack1.h>
#endif
typedef struct {
    queue_chain_t       links;
    run_queue_t         runq;
    wait_queue_t        wait_queue;
    event64_t           wait_event __TYPE_PACKED;
    thread_act_t        top_act;
    union {
        struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
            uint32_t                    : 28; // 4..31 -> 0..27
            uint32_t    active_callout  : 1; // 3 -> 28
            uint32_t    vm_privilege    : 1; // 2 -> 29
            uint32_t    interrupt_level : 2; // 0..1 -> 30..31
#elif __BYTE_ORDER == __BIG_ENDIAN
            uint32_t    interrupt_level : 2; // 0..1
            uint32_t    vm_privilege    : 1; // 2
            uint32_t    active_callout  : 1; // 3
#else
# error "Please fix <bits/endian.h>"
#endif
        };
        uint32_t        bits;
    };
    hw_lock_data_t      lock;
    hw_lock_data_t      wake_lock;
    boolean_t           wake_active;
    int32_t             at_safe_point;
    ast_t               reason;
    wait_result_t       wait_result;
    thread_roust_t      roust;
    thread_continue_t   continuation;
    funnel_lock_p       funnel_lock;
    int32_t             funnel_state;
    vm_offset_t_32      kernel_stack;
    vm_offset_t_32      stack_privilege;
    int32_t             state;
    integer_t           sched_mode;
    integer_t           sched_pri;
    integer_t           priority;
    integer_t           max_priority;
    integer_t           task_priority;
    integer_t           promotions;
    integer_t           pending_promoter_index;
    void_p              pending_promoter[2];
    integer_t           importance;
    struct {
        uint32_t        period;
        uint32_t        computation;
        uint32_t        constraint;
        boolean_t       preemptible;
    }                   realtime;
    uint32_t            current_quantum;
    timer_data_t        system_timer;
    processor_set_t     processor_set;
    processor_t         bound_processor;
    processor_t         last_processor;
    uint64_t            last_switch __TYPE_PACKED;
    uint64_t            computation_metered __TYPE_PACKED;
    uint64_t            computation_epoch __TYPE_PACKED;
    integer_t           safe_mode;
    natural_t           safe_release;
    natural_t           sched_stamp;
    natural_t           cpu_usage;
    natural_t           cpu_delta;
    natural_t           sched_usage;
    natural_t           sched_delta;
    natural_t           sleep_stamp;
    timer_data_t        user_timer;
    timer_save_data_t   system_timer_save;
    timer_save_data_t   user_timer_save;
    timer_call_data_t   wait_timer;
    integer_t           wait_timer_active;
    boolean_t           wait_timer_is_set;
    timer_call_data_t   depress_timer;
    integer_t           depress_timer_active;
    union {
        struct {
            mach_msg_return_t_32    state;
            ipc_object_t            object;
            mach_msg_header_t_32_p  msg;
            mach_msg_size_t         msize;
            mach_msg_option_t_32    option;
            mach_msg_size_t         slist_size;
            ipc_kmsg_p              kmsg;
            mach_port_seqno_t       seqno;
            mach_msg_continue_t     continuation;
        }               receive;
        struct {
            semaphore_p             waitsemaphore;
            semaphore_p             signalsemaphore;
            int32_t                 options;
            kern_return_t_32        result;
            mach_msg_continue_t     continuation;
        }               sema;
        struct {
            int32_t     option;
        }               swtch;
        int32_t         misc;
    }                   saved;
    ipc_kmsg_queue      ith_messages;
    mach_port_t         ith_mig_reply;
    mach_port_t         ith_rpc_reply;
    boolean_t           active;
    vm_offset_t_32      recover;
    int32_t             ref_count;
    queue_chain_t       pset_threads;
} thread_shuttle;
#ifdef _MSC_VER
#include <poppack.h>
#endif
assert_size(thread_shuttle, 404);

typedef struct {
    savearea_p save_prev;
    uint32_t sac_next;
    uint32_t sac_prev;
    uint32_t save_flags;
    uint32_t save_level;
    uint32_t save_time[2];
    thread_activation_p save_act;
    uint32_t sac_vrswap;
    uint32_t sac_alloc;
    uint32_t sac_flags;
    uint32_t save_misc0;
    uint32_t save_misc1;
    uint32_t save_misc2;
    uint32_t save_misc3;
    uint32_t save_misc4;
    uint32_t save_040[8];
} savearea_comm;

typedef struct {
    savearea_comm   save_hdr;
    uint32_t        save_060[8];
    uint32_t        save_r0;
    uint32_t        save_r1;
    uint32_t        save_r2;
    uint32_t        save_r3;
    uint32_t        save_r4;
    uint32_t        save_r5;
    uint32_t        save_r6;
    uint32_t        save_r7;
    uint32_t        save_r8;
    uint32_t        save_r9;
    uint32_t        save_r10;
    uint32_t        save_r11;
    uint32_t        save_r12;
    uint32_t        save_r13;
    uint32_t        save_r14;
    uint32_t        save_r15;
    uint32_t        save_r16;
    uint32_t        save_r17;
    uint32_t        save_r18;
    uint32_t        save_r19;
    uint32_t        save_r20;
    uint32_t        save_r21;
    uint32_t        save_r22;
    uint32_t        save_r23;
    uint32_t        save_r24;
    uint32_t        save_r25;
    uint32_t        save_r26;
    uint32_t        save_r27;
    uint32_t        save_r28;
    uint32_t        save_r29;
    uint32_t        save_r30;
    uint32_t        save_r31;
    uint32_t        save_srr0;
    uint32_t        save_srr1;
    uint32_t        save_cr;
    uint32_t        save_xer;
    uint32_t        save_lr;
    uint32_t        save_ctr;
    uint32_t        save_dar;
    uint32_t        save_dsisr;
    uint32_t        save_vscr[4];
    uint32_t        save_fpscrpad;
    uint32_t        save_fpscr;
    uint32_t        save_exception;
    uint32_t        save_vrsave;
    uint32_t        save_sr0;
    uint32_t        save_sr1;
    uint32_t        save_sr2;
    uint32_t        save_sr3;
    uint32_t        save_sr4;
    uint32_t        save_sr5;
    uint32_t        save_sr6;
    uint32_t        save_sr7;
    uint32_t        save_sr8;
    uint32_t        save_sr9;
    uint32_t        save_sr10;
    uint32_t        save_sr11;
    uint32_t        save_sr12;
    uint32_t        save_sr13;
    uint32_t        save_sr14;
    uint32_t        save_sr15;
    uint32_t        save_180[8];
    uint32_t        save_1A0[8];
    uint32_t        save_1C0[8];
    uint32_t        save_1E0[8];
    uint32_t        save_200[8];
    uint32_t        save_220[8];
    uint32_t        save_240[8];
    uint32_t        save_260[8];
}  savearea;

typedef struct {
    queue_head_t    queues[128];
    hw_lock_data_t  lock;
    int32_t         bitmap[4];
    int32_t         highq;
    int32_t         urgency;
    int32_t         count;
} run_queue;
assert_size(run_queue, 1056);

typedef struct {
    queue_head_t    idle_queue;
    int32_t         idle_count;
    queue_head_t    active_queue;
    hw_lock_data_t  sched_lock;
    queue_head_t    processors;
    int32_t         processor_count;
    hw_lock_data_t  processors_lock;
    run_queue       runq;
    queue_head_t    tasks;
    int32_t         task_count;
    queue_head_t    threads;
    int32_t         thread_count;
    int32_t         ref_count;
    boolean_t       active;
    mutex_t         lock;
    int32_t         set_quanta;
    int32_t         machine_quanta[3];
    ipc_port_p      pset_self;
    ipc_port_p      pset_name_self;
    uint32_t        run_count;
    integer_t       mach_factor;
    integer_t       load_average;
    uint32_t        sched_load;
} processor_set;
assert_size(processor_set, 1180);

typedef struct {
    uint32_t        val[2];
} security_token_t_32;
assert_size(security_token_t_32, 8);

typedef struct {
    integer_t       seconds;
    integer_t       microseconds;
} time_value_t;
assert_size(time_value_t, 8);

typedef uint32_t task_role_t;
typedef uint32_t ipc_space_p;
typedef uint32_t eml_dispatch_p;

typedef struct {
    mutex_t             lock;
    int32_t             ref_count;
    boolean_t           active;
    boolean_t           kernel_loaded;
    vm_map_t            map;
    queue_chain_t       pset_tasks;
    void_p              user_data;
    int32_t             suspend_count;
    queue_head_t        thr_acts;
    int32_t             thr_act_count;
    int32_t             res_act_count;
    int32_t             active_act_count;
    processor_set_t     processor_set;
    integer_t           user_stop_count;
    task_role_t         role;
    integer_t           priority;
    integer_t           max_priority;
    security_token_t_32 sec_token;
    time_value_t        total_user_time;
    time_value_t        total_system_time;
    mutex_t             itk_lock_data;
    ipc_port_p          itk_self;
    ipc_port_p          itk_sself;
    exception_action    exc_actions[10];
    ipc_port_p          itk_host;
    ipc_port_p          itk_bootstrap;
    ipc_port_p          itk_registered[3];
    ipc_space_p         itk_space;
    queue_head_t        semaphore_list;
    queue_head_t        lock_set_list;
    int32_t             semaphores_owned;
    int32_t             lock_sets_owned;
    eml_dispatch_p      eml_dispatch;
    ipc_port_p          wired_ledger_port;
    ipc_port_p          paged_ledger_port;
    integer_t           faults;
    integer_t           pageins;
    integer_t           cow_faults;
    integer_t           messages_sent;
    integer_t           messages_received;
    integer_t           syscalls_mach;
    integer_t           syscalls_unix;
    integer_t           csw;
    void_p              bsd_info;
    vm_offset_t_32      system_shared_region;
    vm_offset_t_32      dynamic_working_set;
} task;
assert_size(task, 352);

typedef struct {
    union {
        struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
            uint32_t        : 14; // 18..31 -> 0..13
            uint32_t        wq_isprepost : 1; // 17 -> 14
            uint32_t        wq_fifo : 1; // 16 -> 15
            uint32_t        wq_type : 16; // 0..15 -> 16..31
#elif __BYTE_ORDER == __BIG_ENDIAN
            uint32_t        wq_type : 16; // 0..15
            uint32_t        wq_fifo : 1; // 16
            uint32_t        wq_isprepost : 1; // 17
#else
# error "Please fix <bits/endian.h>"
#endif
        };
        uint32_t    bits;
    };
    hw_lock_data_t  wq_interlock;
    queue_head_t    wq_queue;
} wait_queue;
assert_size(wait_queue, 16);

typedef struct {
    queue_chain_t   wqe_links;
    void_p          wqe_type;
    wait_queue_t    wqe_queue;
} wait_queue_element;
assert_size(wait_queue_element, 16);

typedef struct {
    union {
        struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
            uint32_t        : 14; // 18..31 -> 0..13
            uint32_t        wq_isprepost : 1; // 17 -> 14
            uint32_t        wq_fifo : 1; // 16 -> 15
            uint32_t        wq_type : 16; // 0..15 -> 16..31
#elif __BYTE_ORDER == __BIG_ENDIAN
            uint32_t        wq_type : 16; // 0..15
            uint32_t        wq_fifo : 1; // 16
            uint32_t        wq_isprepost : 1; // 17
#else
# error "Please fix <bits/endian.h>"
#endif
        };
        uint32_t    bits;
    };
    hw_lock_data_t  wq_interlock;
    queue_head_t    wq_queue;
} WaitQueue;
assert_size(WaitQueue, 16);

typedef struct wait_queue_set {
    WaitQueue       wqs_wait_queue;
    queue_head_t    wqs_setlinks;
    uint32_t        wqs_refcount;
} wait_queue_set;
assert_size(wait_queue_set, 28);

typedef struct {
    queue_chain_t   wqe_links;
    void_p          wqe_type;
    wait_queue_t    wqe_queue;
} WaitQueueElement;
assert_size(WaitQueueElement, 16);

typedef uint32_t wait_queue_set_t;

typedef struct {
    WaitQueueElement    wql_element;
    queue_chain_t       wql_setlinks;
    wait_queue_set_t    wql_setqueue;
} wait_queue_link;
assert_size(wait_queue_link, 28);

typedef struct {
    hw_lock_data_t interlock;
    union {
        struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
            uint32_t : 12; // 20..31 -> 0..11
            uint32_t can_sleep : 1; // 19 -> 12
            uint32_t waiting : 1; // 18 -> 13
            uint32_t want_write : 1; // 17 -> 14
            uint32_t want_upgrade : 1; // 16 -> 15
            uint32_t read_count : 16; // 0..15 -> 16..31
#elif __BYTE_ORDER == __BIG_ENDIAN
            uint32_t read_count : 16; // 0..15
            uint32_t want_upgrade : 1; // 16
            uint32_t want_write : 1; // 17
            uint32_t waiting : 1; // 18
            uint32_t can_sleep : 1; // 19
#else
# error "Please fix <bits/endian.h>"
#endif
        };
        uint32_t bits;
    };
} lock_t;
assert_size(lock_t, 8);

typedef uint32_t vm_map_entry_p;

typedef struct vm_map_links {
    vm_map_entry_p  prev;
    vm_map_entry_p  next;
    vm_offset_t_32  start;
    vm_offset_t_32  end;
} vm_map_links;
assert_size(vm_map_links, 16);

typedef struct {
    vm_map_links    links;
    int32_t         nentries;
    boolean_t       entries_pageable;
} vm_map_header;
assert_size(vm_map_header, 24);

typedef uint32_t pmap_t;
typedef uint32_t vm_size_t_32;
typedef uint32_t vm_map_entry_t;

typedef struct {
    lock_t              lock;
    vm_map_header       hdr;
    pmap_t              pmap;
    vm_size_t_32        size;
    int32_t             ref_count;
    mutex_t             s_lock;
    vm_map_entry_t      hint;
    vm_map_entry_t      first_free;
    boolean_t           wait_for_space;
    boolean_t           wiring_required;
    boolean_t           no_zero_fill;
    boolean_t           mapped;
    uint32_t            timestamp;
} vm_map;
assert_size(vm_map, 84);

typedef struct {
    integer_t   resident_count;
    integer_t   wired_count;
} pmap_statistics;
assert_size(pmap_statistics, 8);

typedef uint32_t blokmap_p;
typedef uint32_t pmap_p;
typedef uint32_t space_t;

typedef struct {
    queue_head_t    pmap_link;
    uint32_t        pmapvr;
    space_t         space;
    blokmap_p       bmaps;
    int32_t         ref_count;
    uint32_t        vflags;
    uint32_t        spaceNum;
    uint32_t        pmapSegs[16];
    pmap_p          pmapPmaps[16];
    uint16_t        pmapUsage[128];
    pmap_statistics stats;
    hw_lock_data_t  lock;
} pmap;
assert_size(pmap, 428);

typedef uint32_t vm_object_p;
typedef uint32_t vm_map_p;

typedef union {
    vm_object_p vm_object;
    vm_map_p    sub_map;
} vm_map_object;
assert_size(vm_map_object, 4);

typedef uint64_t vm_object_offset_t;

#ifdef _MSC_VER
#include <pshpack1.h>
#endif
typedef struct {
    vm_map_links        links;
    vm_map_object       object;
    vm_object_offset_t  offset __TYPE_PACKED;
    union {
        struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
            uint32_t    : 8; // 24..31 -> 0..7
            uint32_t    alias : 8; // 16..23 -> 8..15
            uint32_t    use_pmap : 1; // 15 -> 16
            uint32_t    inheritance : 2; // 13..14 -> 17..18
            uint32_t    max_protection : 3; // 10..12 -> 19..21
            uint32_t    protection : 3; // 7..9 -> 22..24
            uint32_t    needs_copy : 1; // 6 -> 25
            uint32_t    behavior : 2; // 4..5 -> 26..27
            uint32_t    needs_wakeup : 1; // 3 -> 28
            uint32_t    in_transition : 1; // 2 -> 29
            uint32_t    is_sub_map : 1; // 1 -> 30
            uint32_t    is_shared : 1; // 0 -> 31
#elif __BYTE_ORDER == __BIG_ENDIAN
            uint32_t    is_shared : 1; // 0..0
            uint32_t    is_sub_map : 1; // 1..1
            uint32_t    in_transition : 1; // 2..2
            uint32_t    needs_wakeup : 1; // 3..3
            uint32_t    behavior : 2; // 4..5
            uint32_t    needs_copy : 1; // 6..6
            uint32_t    protection : 3; // 7..9
            uint32_t    max_protection : 3; // 10..12
            uint32_t    inheritance : 2; // 13..14
            uint32_t    use_pmap : 1; // 15..15
            uint32_t    alias : 8; // 16..23
#else
# error "Please fix <bits/endian.h>"
#endif
        };
        uint32_t        bits;
    };
    uint16_t            wired_count;
    uint16_t            user_wired_count;
} vm_map_entry;
#ifdef _MSC_VER
#include <poppack.h>
#endif
assert_size(vm_map_entry, 36);

typedef uint32_t ipc_object_p;
typedef uint32_t ipc_entry_bits_t;
typedef uint32_t ipc_table_index_t_p;
typedef uint32_t mach_port_index_t;
typedef uint32_t ipc_tree_entry_p;

typedef struct {
    ipc_object_p        ie_object;
    ipc_entry_bits_t    ie_bits;
    union index {
        mach_port_index_t   next;
        ipc_table_index_t_p request;
    } index;
    union  {
        mach_port_index_t   table;
        ipc_tree_entry_p    tree;
    } hash;
} ipc_entry;
assert_size(ipc_entry, 16);

typedef uint32_t mach_port_name_t;
typedef uint32_t ipc_tree_entry_t;
typedef uint32_t ipc_tree_entry_t_p;

typedef struct {
    mach_port_name_t        ist_name;
    ipc_tree_entry_t        ist_root;
    ipc_tree_entry_t        ist_ltree;
    ipc_tree_entry_t_p      ist_ltreep;
    ipc_tree_entry_t        ist_rtree;
    ipc_tree_entry_t_p      ist_rtreep;
} ipc_splay_tree;
assert_size(ipc_splay_tree, 24);

typedef uint32_t ipc_space_refs_t;
typedef uint32_t ipc_entry_t;
typedef uint32_t ipc_entry_num_t;
typedef uint32_t ipc_table_size_p;

typedef struct {
    mutex_t             is_ref_lock_data;
    ipc_space_refs_t    is_references;
    mutex_t             is_lock_data;
    boolean_t           is_active;
    boolean_t           is_growing;
    ipc_entry_t         is_table;
    ipc_entry_num_t     is_table_size;
    ipc_table_size_p    is_table_next;
    ipc_splay_tree      is_tree;
    ipc_entry_num_t     is_tree_total;
    ipc_entry_num_t     is_tree_small;
    ipc_entry_num_t     is_tree_hash;
    boolean_t           is_fast;
} ipc_space;
assert_size(ipc_space, 88);

typedef struct {
    void_p      lock_pc;
    void_p      lock_thread;
    uint32_t    duration[2];
    uint16_t    state;
    uint8_t     lock_cpu;
    void_p      unlock_thread;
    uint8_t     unlock_cpu;
    void_p      unlock_pc;
} uslock_debug;
assert_size(uslock_debug, 32);

typedef struct {
    int32_t     tv_sec;
    int32_t     tv_usec;
} timeval_32;
assert_size(timeval_32, 8);

typedef struct  {
    timeval_32      it_interval;
    timeval_32      it_value;
} itimerval;
assert_size(itimerval, 16);

typedef uint64_t    u_quad_t;
typedef uint8_t     u_char;
typedef uint32_t    pgrp_p;

typedef struct {
    hw_lock_data_t      interlock;
    uint16_t            lock_type;
    uslock_debug        debug;
} simple_lock_data_t;
assert_size(simple_lock_data_t, 40);

typedef uint32_t char_p;

typedef struct {
    simple_lock_data_t  lk_interlock;
    uint32_t            lk_flags;
    int32_t             lk_sharecount;
    int32_t             lk_waitcount;
    int16_t             lk_exclusivecount;
    int16_t             lk_prio;
    char_p              lk_wmesg;
    int32_t             lk_timo;
    pid_t               lk_lockholder;
    void_p              lk_lockthread;
} lock__bsd__;
assert_size(lock__bsd__, 72);

typedef uint32_t proc_p;
typedef uint32_t proc_pp;
typedef uint32_t pcred_p;
typedef uint32_t filedesc_p;
typedef uint32_t pstats_p;
typedef uint32_t plimit_p;
typedef uint32_t sigacts_p;
typedef uint32_t vnode_p;
typedef uint32_t eventqelt_p;
typedef uint32_t eventqelt_pp;
typedef uint32_t rusage_p;
typedef uint32_t uthread_p;
typedef uint32_t uthread_pp;
typedef uint32_t sigset_t_32;
typedef uint32_t fixpt_t;
typedef uint32_t caddr_t_32;

#ifdef _MSC_VER
#include <pshpack1.h>
#endif
typedef struct {
    struct {
        proc_p          le_next;
        proc_pp         le_prev;
    } p_list;
    pcred_p             p_cred;
    filedesc_p          p_fd;
    pstats_p            p_stats;
    plimit_p            p_limit;
    sigacts_p           p_sigacts;
    int32_t             p_flag;
    char                p_stat;
    char                p_pad1[3];
    pid_t               p_pid;
    struct {
        proc_p          le_next;
        proc_pp         le_prev;
    } p_pglist;
    proc_p              p_pptr;
    struct {
        proc_p          le_next;
        proc_pp         le_prev;
    } p_sibling;
    struct {
        proc_p          lh_first;
    } p_children;
    pid_t               p_oppid;
    int32_t             p_dupfd;
    uint32_t            p_estcpu;
    int32_t             p_cpticks;
    fixpt_t             p_pctcpu;
    void_p              p_wchan;
    char_p              p_wmesg;
    uint32_t            p_swtime;
    uint32_t            p_slptime;
    itimerval           p_realtimer;
    timeval_32          p_rtime;
    u_quad_t            p_uticks __TYPE_PACKED;
    u_quad_t            p_sticks __TYPE_PACKED;
    u_quad_t            p_iticks __TYPE_PACKED;
    int32_t             p_traceflag;
    vnode_p             p_tracep;
    sigset_t_32         p_siglist;
    vnode_p             p_textvp;
    struct {
        proc_p          le_next;
        proc_pp         le_prev;
    } p_hash;
    struct {
        eventqelt_p     tqh_first;
        eventqelt_pp    tqh_last;
    } p_evlist;
    sigset_t_32         p_sigmask;
    sigset_t_32         p_sigignore;
    sigset_t_32         p_sigcatch;
    u_char              p_priority;
    u_char              p_usrpri;
    char                p_nice;
    char                p_comm[17];
    pgrp_p              p_pgrp;
    uint16_t            p_xstat;
    uint16_t            p_acflag;
    rusage_p            p_ru;
    int32_t             p_debugger;
    void_p              task;
    void_p              sigwait_thread;
    lock__bsd__         signal_lock;
    boolean_t           sigwait;
    void_p              exit_thread;
    caddr_t_32          user_stack;
    void_p              exitarg;
    void_p              vm_shm;
    sigset_t_32         p_xxxsigpending;
    int32_t             p_vforkcnt;
    void_p              p_vforkact;
    struct {
        uthread_p       tqh_first;
        uthread_pp      tqh_last;
    } p_uthlist;
    pid_t               si_pid;
    uint16_t            si_status;
    uint16_t            si_code;
    uid_t               si_uid;
} proc;
#ifdef _MSC_VER
#include <poppack.h>
#endif
assert_size(proc, 360);

typedef uint32_t mach_msg_bits_t;
typedef uint32_t mach_msg_id_t_32;
typedef uint32_t ipc_port_t;

typedef struct {
    mach_msg_bits_t     msgh_bits;
    mach_msg_size_t     msgh_size;
    mach_port_t         msgh_remote_port;
    mach_port_t         msgh_local_port;
    mach_msg_size_t     msgh_reserved_32;
    mach_msg_id_t_32    msgh_id;
} mach_msg_header_t_32;
assert_size(mach_msg_header_t_32, 24);

typedef struct {
    ipc_kmsg_p              ikm_next;
    ipc_kmsg_p              ikm_prev;
    ipc_port_t              ikm_prealloc;
    mach_msg_size_t         ikm_size;
    mach_msg_header_t_32    ikm_header;
} ipc_kmsg;
assert_size(ipc_kmsg, 40);

typedef uint32_t ipc_object_refs_t;
typedef uint32_t ipc_object_bits_t;
typedef uint32_t port_name_t;

typedef struct {
    ipc_object_refs_t           io_references;
    ipc_object_bits_t           io_bits;
    port_name_t                 io_receiver_name;
    mutex_t                     io_lock_data;
} ipc_object;
assert_size(ipc_object, 24);

typedef uint32_t mach_port_msgcount_t;

typedef struct {
    union {
        struct {
            wait_queue              wait_queue;
            ipc_kmsg_queue          messages;
            mach_port_msgcount_t    msgcount;
            mach_port_msgcount_t    qlimit;
            mach_port_seqno_t       seqno;
            boolean_t               fullwaiters;
        } port;
        wait_queue_set              set_queue;
    } data;
} ipc_mqueue;
assert_size(ipc_mqueue, 36);

typedef uint32_t ipc_port_timestamp_t;
typedef uint32_t ipc_kobject_t;
typedef uint32_t ipc_port_request_p;
typedef uint32_t mach_port_mscount_t_32;
typedef uint32_t mach_port_rights_t_32;
typedef uint32_t mach_port_rights_t_32;

typedef struct {
    ipc_object                  ip_object;
    union {
        ipc_space_p             receiver;
        ipc_port_p              destination;
        ipc_port_timestamp_t    timestamp;
    } data;
    ipc_kobject_t               ip_kobject;
    mach_port_mscount_t_32      ip_mscount;
    mach_port_rights_t_32       ip_srights;
    mach_port_rights_t_32       ip_sorights;
    ipc_port_p                  ip_nsrequest;
    ipc_port_p                  ip_pdrequest;
    ipc_port_request_p          ip_dnrequests;
    uint32_t                    ip_pset_count;
    ipc_mqueue                  ip_messages;
    ipc_kmsg_p                  ip_premsg;
    int32_t                     alias;
} ipc_port;
assert_size(ipc_port, 104);

typedef struct {
    ipc_object  ips_object;
    ipc_mqueue  ips_messages;
} ipc_pset;
assert_size(ipc_pset, 60);

#ifdef _MSC_VER
#include <pshpack1.h>
#endif
typedef struct {
    queue_chain_t           q_link;
    call_entry_func_t       func;
    call_entry_param_t      param0;
    call_entry_param_t      param1;
    uint64_t                deadline __TYPE_PACKED;
    uint32_t                state;
} call_entry_data_t;
#ifdef _MSC_VER
#include <poppack.h>
#endif
assert_size(call_entry_data_t, 32);

typedef uint32_t zone_p;

typedef struct {
    int32_t             count;
    vm_offset_t_32      free_elements;
    vm_size_t_32        cur_size;
    vm_size_t_32        max_size;
    vm_size_t_32        elem_size;
    vm_size_t_32        alloc_size;
    char_p              zone_name;
    union {
        struct {
#if __BYTE_ORDER == __LITTLE_ENDIAN
            uint32_t            : 25; // 7..31 -> 0..24
            uint32_t            async_pending : 1; // 6 -> 25
            uint32_t            waiting : 1; // 5 -> 26
            uint32_t            doing_alloc : 1; // 4 -> 27
            uint32_t            allows_foreign : 1; // 3 -> 28
            uint32_t            expandable : 1; // 2 -> 29
            uint32_t            collectable : 1; // 1 -> 30
            uint32_t            exhaustible : 1; // 0 -> 31
#elif __BYTE_ORDER == __BIG_ENDIAN
            uint32_t            exhaustible : 1; // 0
            uint32_t            collectable : 1; // 1
            uint32_t            expandable : 1; // 2
            uint32_t            allows_foreign : 1; // 3
            uint32_t            doing_alloc : 1; // 4
            uint32_t            waiting : 1; // 5
            uint32_t            async_pending : 1; // 6
#else
# error "Please fix <bits/endian.h>"
#endif
        };
        uint32_t        bits;
    };
    zone_p              next_zone;
    call_entry_data_t   call_async_alloc;
    hw_lock_data_t      lock;
} zone;
assert_size(zone, 72);

typedef uint32_t cpu_type_t_32;
typedef uint32_t cpu_subtype_t_32;

typedef struct {
    integer_t           is_cpu;
    cpu_type_t_32       cpu_type;
    cpu_subtype_t_32    cpu_subtype;
    integer_t           running;
    integer_t           cpu_ticks[4];
    integer_t           clock_freq;
} machine_slot;
assert_size(machine_slot, 36);

typedef struct {
    uint16_t        reply_port;
    uint32_t        conn_seq;
    boolean_t       is_conn;
    void_p          saved_state;
    boolean_t       is_halted;
    uint16_t        exception_port;
    uint8_t         exception_seq;
    boolean_t       exception_ack_needed;
} kdp_glob_t;
assert_size(kdp_glob_t, 28);

typedef uint32_t queue_entry_t;

#endif // KGMACRO_TYPES_H_
