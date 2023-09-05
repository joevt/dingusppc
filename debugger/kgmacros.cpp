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

#include "symbols.h"
#include "memaccess.h"
#include <cpu/ppc/ppcmmu.h>
#include <loguru.hpp>
#include <fstream>
#include <vector>
#include <string>
#ifdef __APPLE__
#include <mach-o/loader.h>
#endif

#include "kgmacros.h"
#include "kgmacrostypes.h" // needs to be last

using namespace std;


/* ==================================================================================== */

static uint8_t* kernel_mem = nullptr;

static uint8_t* get_kernel_mem() {
    if (!kernel_mem) {
        AddressMapEntry* entry = mem_ctrl_instance->find_range(0);
        if (entry && entry->type & RT_RAM) {
            kernel_mem = entry->mem_ptr + (0 - entry->start);
        }
    }
    return kernel_mem;
}

uint64_t kernel_read(uint32_t addr, uint32_t size) {
    uint8_t* kmem = get_kernel_mem();
    if (kmem) {
        switch (size) {
            case 1: return  *((uint8_t  *)(kmem + addr));
            case 2: return READ_WORD_BE_A (kmem + addr);
            case 4: return READ_DWORD_BE_A(kmem + addr);
            case 8: return READ_QWORD_BE_A(kmem + addr);
            default: return 0;
        }
    }
    else {
        return 0;
    }
}

void kernel_write(uint32_t addr, uint64_t val, uint32_t size) {
    uint8_t* kmem = get_kernel_mem();
    if (kmem) {
        switch (size) {
            case 1: *((uint8_t  *)(kmem + addr)) = (uint8_t )val; break;
            case 2: WRITE_WORD_BE_A (kmem + addr, (uint16_t)val); break;
            case 4: WRITE_DWORD_BE_A(kmem + addr, (uint32_t)val); break;
            case 8: WRITE_QWORD_BE_A(kmem + addr, (uint64_t)val); break;
        }
    }
}


/* ==================================================================================== */

#define genericreadfield(dst, fld, typ, src) \
    do { \
        dst.fld = (typ)kernel_reaad(src + (uint32_t)((char*)&dst.fld - (char*)&dst), sizeof(typ)); \
    } while (0)

uint32_t get_32(uint32_t p) {
    return (uint32_t)kernel_read(p, sizeof(uint32_t));
}

uint32_t get_32vm(uint32_t p) {
    return (uint32_t)mem_read_dbg(p, sizeof(uint32_t));
}

void set_32(uint32_t p, uint32_t v) {
    kernel_write(p, v, sizeof(uint32_t));
}

#define get_addr(typ, p, fld) \
    ((p) + offsetof(typ, fld))

#define get_field(typ, p, fld) \
    (std::remove_reference_t<decltype(((typ*)0)->fld)>)kernel_read(get_addr(typ, p, fld), sizeof(((typ*)0)->fld))

#define get_fieldvm(typ, p, fld) \
    (std::remove_reference_t<decltype(((typ*)0)->fld)>)mem_read_dbg(get_addr(typ, p, fld), sizeof(((typ*)0)->fld))

#define readfield(fld) \
    do { \
        try { \
            s.fld = (std::remove_reference_t<decltype((s.fld))>)kernel_read( \
                p + (uint32_t)((char*)&s.fld - (char*)&s), sizeof(s.fld) \
            ); \
        } catch (...) { \
            s.fld = 0; \
        } \
    } while (0)

#define readfieldvm(fld) \
    do { \
        try { \
            s.fld = (std::remove_reference_t<decltype((s.fld))>)mem_read_dbg( \
                p + (uint32_t)((char*)&s.fld - (char*)&s), sizeof(s.fld) \
            ); \
        } catch (...) { \
            s.fld = 0; \
        } \
    } while (0)

/* ==================================================================================== */

static void get_kmod_info(uint32_t p, kmod_info_t &s) {
    s.kmod = p;
    uint64_t val;
    readfieldvm(next           );
    readfieldvm(info_version   );
    readfieldvm(id             );
    readfieldvm(reference_count);
    readfieldvm(reference_list );
    readfieldvm(address        );
    readfieldvm(size           );
    readfieldvm(hdr_size       );
    readfieldvm(start          );
    readfieldvm(stop           );
    for (int i = 0; i < 8; i++) {
        val = mem_read_dbg(p + offsetof(kmod_info_t, name   ) + i * 8, 8);
        WRITE_QWORD_BE_A(&(((uint64_t*)(&s.name   ))[i]), val);
        if (!val) break;
    }
    for (int i = 0; i < 8; i++) {
        val = mem_read_dbg(p + offsetof(kmod_info_t, version) + i * 8, 8);
        WRITE_QWORD_BE_A(&(((uint64_t*)(&s.version))[i]), val);
        if (!val) break;
    }
}

static void get_thread_activation(uint32_t p, thread_activation &s) {
    s.thread_activation = p;
    readfieldvm(thr_acts.next             );
    readfieldvm(thr_acts.prev             );
    readfieldvm(kernel_loaded             );
    readfieldvm(kernel_loading            );
    readfieldvm(inited                    );
    readfieldvm(mact.pcb                  );
    readfieldvm(mact.curctx               );
    readfieldvm(mact.deferctx             );
    readfieldvm(mact.facctx.FPUsave       );
    readfieldvm(mact.facctx.FPUlevel      );
    readfieldvm(mact.facctx.FPUcpu        );
    readfieldvm(mact.facctx.VMXsave       );
    readfieldvm(mact.facctx.VMXlevel      );
    readfieldvm(mact.facctx.VMXcpu        );
    readfieldvm(mact.facctx.facAct        );
    readfieldvm(mact.vmmCEntry            );
    readfieldvm(mact.vmmControl           );
    readfieldvm(mact.qactTimer            );
    readfieldvm(mact.ksp                  );
    readfieldvm(mact.bbDescAddr           );
    readfieldvm(mact.bbUserDA             );
    readfieldvm(mact.bbTableStart         );
    readfieldvm(mact.emPendRupts          );
    readfieldvm(mact.bbTaskID             );
    readfieldvm(mact.bbTaskEnv            );
    readfieldvm(mact.specFlags            );
    readfieldvm(mact.cthread_self         );
    readfieldvm(lock.interlock.lock_data  );
    readfieldvm(lock.locked.lock_data     );
    readfieldvm(lock.waiters              );
    readfieldvm(lock.promoted_pri         );
    readfieldvm(sched_lock.lock_data      );
    readfieldvm(ref_count                 );
    readfieldvm(task                      );
    readfieldvm(map                       );
    readfieldvm(thread                    );
    readfieldvm(higher                    );
    readfieldvm(lower                     );
    readfieldvm(alerts                    );
    readfieldvm(alert_mask                );
    readfieldvm(suspend_count             );
    readfieldvm(user_stop_count           );
    readfieldvm(ast                       );
    readfieldvm(active                    );
    readfieldvm(handlers                  );
    readfieldvm(special_handler.next      );
    readfieldvm(special_handler.handler   );
    readfieldvm(ith_self                  );
    readfieldvm(ith_sself                 );
    for (int i = 0; i < 10; i++) {
    readfieldvm(exc_actions[i].port       );
    readfieldvm(exc_actions[i].flavor     );
    readfieldvm(exc_actions[i].behavior   );
    }
    readfieldvm(held_ulocks.next          );
    readfieldvm(held_ulocks.prev          );
    readfieldvm(uthread                   );
}

static void get_thread_shuttle(uint32_t p, thread_shuttle &s) {
    readfieldvm(links.next                        );
    readfieldvm(links.prev                        );
    readfieldvm(runq                              );
    readfieldvm(wait_queue                        );
    readfieldvm(wait_event                        );
    readfieldvm(top_act                           );
    readfieldvm(bits                              );
    readfieldvm(lock.lock_data                    );
    readfieldvm(wake_lock.lock_data               );
    readfieldvm(wake_active                       );
    readfieldvm(at_safe_point                     );
    readfieldvm(reason                            );
    readfieldvm(wait_result                       );
    readfieldvm(roust                             );
    readfieldvm(continuation                      );
    readfieldvm(funnel_lock                       );
    readfieldvm(funnel_state                      );
    readfieldvm(kernel_stack                      );
    readfieldvm(stack_privilege                   );
    readfieldvm(state                             );
    readfieldvm(sched_mode                        );
    readfieldvm(sched_pri                         );
    readfieldvm(priority                          );
    readfieldvm(max_priority                      );
    readfieldvm(task_priority                     );
    readfieldvm(promotions                        );
    readfieldvm(pending_promoter_index            );
    readfieldvm(pending_promoter[0]               );
    readfieldvm(pending_promoter[1]               );
    readfieldvm(importance                        );
    readfieldvm(realtime.period                   );
    readfieldvm(realtime.computation              );
    readfieldvm(realtime.constraint               );
    readfieldvm(realtime.preemptible              );
    readfieldvm(current_quantum                   );
    readfieldvm(system_timer.low_bits             );
    readfieldvm(system_timer.high_bits            );
    readfieldvm(system_timer.high_bits_check      );
    readfieldvm(system_timer.tstamp               );
    readfieldvm(processor_set                     );
    readfieldvm(bound_processor                   );
    readfieldvm(last_processor                    );
    readfieldvm(last_switch                       );
    readfieldvm(computation_metered               );
    readfieldvm(computation_epoch                 );
    readfieldvm(safe_mode                         );
    readfieldvm(safe_release                      );
    readfieldvm(sched_stamp                       );
    readfieldvm(cpu_usage                         );
    readfieldvm(cpu_delta                         );
    readfieldvm(sched_usage                       );
    readfieldvm(sched_delta                       );
    readfieldvm(sleep_stamp                       );
    readfieldvm(user_timer.low_bits               );
    readfieldvm(user_timer.high_bits              );
    readfieldvm(user_timer.high_bits_check        );
    readfieldvm(user_timer.tstamp                 );
    readfieldvm(system_timer_save.low             );
    readfieldvm(system_timer_save.high            );
    readfieldvm(user_timer_save.low               );
    readfieldvm(user_timer_save.high              );
    readfieldvm(wait_timer.q_link.next            );
    readfieldvm(wait_timer.q_link.prev            );
    readfieldvm(wait_timer.func                   );
    readfieldvm(wait_timer.param0                 );
    readfieldvm(wait_timer.param1                 );
    readfieldvm(wait_timer.deadline               );
    readfieldvm(wait_timer.state                  );
    readfieldvm(wait_timer_active                 );
    readfieldvm(wait_timer_is_set                 );
    readfieldvm(depress_timer.q_link.next         );
    readfieldvm(depress_timer.q_link.prev         );
    readfieldvm(depress_timer.func                );
    readfieldvm(depress_timer.param0              );
    readfieldvm(depress_timer.param1              );
    readfieldvm(depress_timer.deadline            );
    readfieldvm(depress_timer.state               );
    readfieldvm(depress_timer_active              );
    readfieldvm(saved.receive.state               );
    readfieldvm(saved.receive.object              );
    readfieldvm(saved.receive.msg                 );
    readfieldvm(saved.receive.msize               );
    readfieldvm(saved.receive.option              );
    readfieldvm(saved.receive.slist_size          );
    readfieldvm(saved.receive.kmsg                );
    readfieldvm(saved.receive.seqno               );
    readfieldvm(saved.receive.continuation        );
    readfieldvm(ith_messages.ikmq_base            );
    readfieldvm(ith_mig_reply                     );
    readfieldvm(ith_rpc_reply                     );
    readfieldvm(active                            );
    readfieldvm(recover                           );
    readfieldvm(ref_count                         );
    readfieldvm(pset_threads.next                 );
    readfieldvm(pset_threads.prev                 );
}

static void get_savearea_comm(uint32_t p, savearea_comm &s) {
    readfieldvm(save_prev      );
    readfieldvm(sac_next       );
    readfieldvm(sac_prev       );
    readfieldvm(save_flags     );
    readfieldvm(save_level     );
    readfieldvm(save_time[0]   );
    readfieldvm(save_time[1]   );
    readfieldvm(save_act       );
    readfieldvm(sac_vrswap     );
    readfieldvm(sac_alloc      );
    readfieldvm(sac_flags      );
    readfieldvm(save_misc0     );
    readfieldvm(save_misc1     );
    readfieldvm(save_misc2     );
    readfieldvm(save_misc3     );
    readfieldvm(save_misc4     );
    readfieldvm(save_040[0]    );
    readfieldvm(save_040[1]    );
    readfieldvm(save_040[2]    );
    readfieldvm(save_040[3]    );
    readfieldvm(save_040[4]    );
    readfieldvm(save_040[5]    );
    readfieldvm(save_040[6]    );
    readfieldvm(save_040[7]    );
}

static void get_savearea(uint32_t p, savearea &s) {
    get_savearea_comm(p, s.save_hdr);
    for (int i = 0; i < 8; i++) readfieldvm(save_060[i]);
    readfieldvm(save_r0             );
    readfieldvm(save_r1             );
    readfieldvm(save_r2             );
    readfieldvm(save_r3             );
    readfieldvm(save_r4             );
    readfieldvm(save_r5             );
    readfieldvm(save_r6             );
    readfieldvm(save_r7             );
    readfieldvm(save_r8             );
    readfieldvm(save_r9             );
    readfieldvm(save_r10            );
    readfieldvm(save_r11            );
    readfieldvm(save_r12            );
    readfieldvm(save_r13            );
    readfieldvm(save_r14            );
    readfieldvm(save_r15            );
    readfieldvm(save_r16            );
    readfieldvm(save_r17            );
    readfieldvm(save_r18            );
    readfieldvm(save_r19            );
    readfieldvm(save_r20            );
    readfieldvm(save_r21            );
    readfieldvm(save_r22            );
    readfieldvm(save_r23            );
    readfieldvm(save_r24            );
    readfieldvm(save_r25            );
    readfieldvm(save_r26            );
    readfieldvm(save_r27            );
    readfieldvm(save_r28            );
    readfieldvm(save_r29            );
    readfieldvm(save_r30            );
    readfieldvm(save_r31            );
    readfieldvm(save_srr0           );
    readfieldvm(save_srr1           );
    readfieldvm(save_cr             );
    readfieldvm(save_xer            );
    readfieldvm(save_lr             );
    readfieldvm(save_ctr            );
    readfieldvm(save_dar            );
    readfieldvm(save_dsisr          );
    for (int i = 0; i < 4; i++) readfieldvm(save_vscr[i]);
    readfieldvm(save_fpscrpad       );
    readfieldvm(save_fpscr          );
    readfieldvm(save_exception      );
    readfieldvm(save_vrsave         );
    readfieldvm(save_sr0            );
    readfieldvm(save_sr1            );
    readfieldvm(save_sr2            );
    readfieldvm(save_sr3            );
    readfieldvm(save_sr4            );
    readfieldvm(save_sr5            );
    readfieldvm(save_sr6            );
    readfieldvm(save_sr7            );
    readfieldvm(save_sr8            );
    readfieldvm(save_sr9            );
    readfieldvm(save_sr10           );
    readfieldvm(save_sr11           );
    readfieldvm(save_sr12           );
    readfieldvm(save_sr13           );
    readfieldvm(save_sr14           );
    readfieldvm(save_sr15           );
    for (int i = 0; i < 8; i++) readfieldvm(save_180[i]);
    for (int i = 0; i < 8; i++) readfieldvm(save_1A0[i]);
    for (int i = 0; i < 8; i++) readfieldvm(save_1C0[i]);
    for (int i = 0; i < 8; i++) readfieldvm(save_1E0[i]);
    for (int i = 0; i < 8; i++) readfieldvm(save_200[i]);
    for (int i = 0; i < 8; i++) readfieldvm(save_220[i]);
    for (int i = 0; i < 8; i++) readfieldvm(save_240[i]);
    for (int i = 0; i < 8; i++) readfieldvm(save_260[i]);
};

static void get_wait_queue(uint32_t p, wait_queue &s) {
    readfieldvm(bits                   );
    readfieldvm(wq_interlock.lock_data );
    readfieldvm(wq_queue.next          );
    readfieldvm(wq_queue.prev          );
}

static void get_vm_map(uint32_t p, vm_map &s) {
    readfieldvm(lock.interlock.lock_data      );
    readfieldvm(lock.bits                     );
    readfieldvm(hdr.links.prev                );
    readfieldvm(hdr.links.next                );
    readfieldvm(hdr.links.start               );
    readfieldvm(hdr.links.end                 );
    readfieldvm(hdr.nentries                  );
    readfieldvm(hdr.entries_pageable          );
    readfieldvm(pmap                          );
    readfieldvm(size                          );
    readfieldvm(ref_count                     );
    readfieldvm(s_lock.interlock.lock_data    );
    readfieldvm(s_lock.locked.lock_data       );
    readfieldvm(s_lock.waiters                );
    readfieldvm(s_lock.promoted_pri           );
    readfieldvm(hint                          );
    readfieldvm(first_free                    );
    readfieldvm(wait_for_space                );
    readfieldvm(wiring_required               );
    readfieldvm(no_zero_fill                  );
    readfieldvm(mapped                        );
    readfieldvm(timestamp                     );
}

static void get_vm_map_entry(uint32_t p, vm_map_entry &s) {
    readfieldvm(links.prev        );
    readfieldvm(links.next        );
    readfieldvm(links.start       );
    readfieldvm(links.end         );
    readfieldvm(object.vm_object  );
    readfieldvm(offset            );
    readfieldvm(bits              );
    readfieldvm(wired_count       );
    readfieldvm(user_wired_count  );
}

static void get_ipc_entry(uint32_t p, ipc_entry &s) {
    readfieldvm(ie_object     );
    readfieldvm(ie_bits       );
    readfieldvm(index.next    );
    readfieldvm(hash.table    );
}

static void get_ipc_space(uint32_t p, ipc_space &s) {
    readfieldvm(is_ref_lock_data.interlock.lock_data );
    readfieldvm(is_ref_lock_data.locked.lock_data    );
    readfieldvm(is_ref_lock_data.waiters             );
    readfieldvm(is_ref_lock_data.promoted_pri        );
    readfieldvm(is_references                        );
    readfieldvm(is_lock_data.interlock.lock_data     );
    readfieldvm(is_lock_data.locked.lock_data        );
    readfieldvm(is_lock_data.waiters                 );
    readfieldvm(is_lock_data.promoted_pri            );
    readfieldvm(is_active                            );
    readfieldvm(is_growing                           );
    readfieldvm(is_table                             );
    readfieldvm(is_table_size                        );
    readfieldvm(is_table_next                        );
    readfieldvm(is_tree.ist_name                     );
    readfieldvm(is_tree.ist_root                     );
    readfieldvm(is_tree.ist_ltree                    );
    readfieldvm(is_tree.ist_ltreep                   );
    readfieldvm(is_tree.ist_rtree                    );
    readfieldvm(is_tree.ist_rtreep                   );
    readfieldvm(is_tree_total                        );
    readfieldvm(is_tree_small                        );
    readfieldvm(is_tree_hash                         );
    readfieldvm(is_fast                              );
}

static void get_proc(uint32_t p, proc &s) {
    readfieldvm(p_list.le_next                    );
    readfieldvm(p_list.le_prev                    );
    readfieldvm(p_cred                            );
    readfieldvm(p_fd                              );
    readfieldvm(p_stats                           );
    readfieldvm(p_limit                           );
    readfieldvm(p_sigacts                         );
    readfieldvm(p_flag                            );
    readfieldvm(p_stat                            );
    for (int i = 0; i < 3; i++) readfieldvm(p_pad1[i]);
    readfieldvm(p_pid                             );
    readfieldvm(p_pglist.le_next                  );
    readfieldvm(p_pglist.le_prev                  );
    readfieldvm(p_pptr                            );
    readfieldvm(p_sibling.le_next                 );
    readfieldvm(p_sibling.le_prev                 );
    readfieldvm(p_children.lh_first               );
    readfieldvm(p_oppid                           );
    readfieldvm(p_dupfd                           );
    readfieldvm(p_estcpu                          );
    readfieldvm(p_cpticks                         );
    readfieldvm(p_pctcpu                          );
    readfieldvm(p_wchan                           );
    readfieldvm(p_wmesg                           );
    readfieldvm(p_swtime                          );
    readfieldvm(p_slptime                         );
    readfieldvm(p_realtimer.it_interval.tv_sec    );
    readfieldvm(p_realtimer.it_interval.tv_usec   );
    readfieldvm(p_realtimer.it_value.tv_sec       );
    readfieldvm(p_realtimer.it_value.tv_usec      );
    readfieldvm(p_rtime.tv_sec                    );
    readfieldvm(p_rtime.tv_usec                   );
    readfieldvm(p_uticks                          );
    readfieldvm(p_sticks                          );
    readfieldvm(p_iticks                          );
    readfieldvm(p_traceflag                       );
    readfieldvm(p_tracep                          );
    readfieldvm(p_siglist                         );
    readfieldvm(p_textvp                          );
    readfieldvm(p_hash.le_next                    );
    readfieldvm(p_hash.le_prev                    );
    readfieldvm(p_evlist.tqh_first                );
    readfieldvm(p_evlist.tqh_last                 );
    readfieldvm(p_sigmask                         );
    readfieldvm(p_sigignore                       );
    readfieldvm(p_sigcatch                        );
    readfieldvm(p_priority                        );
    readfieldvm(p_usrpri                          );
    readfieldvm(p_nice                            );
    for (int i = 0; i < 17; i++) readfieldvm(p_comm[i]);
    readfieldvm(p_pgrp                            );
    readfieldvm(p_xstat                           );
    readfieldvm(p_acflag                          );
    readfieldvm(p_ru                              );
    readfieldvm(p_debugger                        );
    readfieldvm(task                              );
    readfieldvm(sigwait_thread                    );
    readfieldvm(signal_lock.lk_interlock.interlock.lock_data  );
    readfieldvm(signal_lock.lk_interlock.lock_type            );
    readfieldvm(signal_lock.lk_interlock.debug.lock_pc        );
    readfieldvm(signal_lock.lk_interlock.debug.lock_thread    );
    readfieldvm(signal_lock.lk_interlock.debug.duration[0]    );
    readfieldvm(signal_lock.lk_interlock.debug.duration[1]    );
    readfieldvm(signal_lock.lk_interlock.debug.state          );
    readfieldvm(signal_lock.lk_interlock.debug.lock_cpu       );
    readfieldvm(signal_lock.lk_interlock.debug.unlock_thread  );
    readfieldvm(signal_lock.lk_interlock.debug.unlock_cpu     );
    readfieldvm(signal_lock.lk_interlock.debug.unlock_pc      );
    readfieldvm(signal_lock.lk_flags              );
    readfieldvm(signal_lock.lk_sharecount         );
    readfieldvm(signal_lock.lk_waitcount          );
    readfieldvm(signal_lock.lk_exclusivecount     );
    readfieldvm(signal_lock.lk_prio               );
    readfieldvm(signal_lock.lk_wmesg              );
    readfieldvm(signal_lock.lk_timo               );
    readfieldvm(signal_lock.lk_lockholder         );
    readfieldvm(signal_lock.lk_lockthread         );
    readfieldvm(sigwait                           );
    readfieldvm(exit_thread                       );
    readfieldvm(user_stack                        );
    readfieldvm(exitarg                           );
    readfieldvm(vm_shm                            );
    readfieldvm(p_xxxsigpending                   );
    readfieldvm(p_vforkcnt                        );
    readfieldvm(p_vforkact                        );
    readfieldvm(p_uthlist.tqh_first               );
    readfieldvm(p_uthlist.tqh_last                );
    readfieldvm(si_pid                            );
    readfieldvm(si_status                         );
    readfieldvm(si_code                           );
    readfieldvm(si_uid                            );
}

static void get_mach_msg_header(uint32_t p, mach_msg_header_t_32 &s) {
    readfieldvm(msgh_bits         );
    readfieldvm(msgh_size         );
    readfieldvm(msgh_remote_port  );
    readfieldvm(msgh_local_port   );
    readfieldvm(msgh_reserved_32  );
    readfieldvm(msgh_id           );
}

static void get_zone(uint32_t p, zone &s) {
    readfieldvm(count                         );
    readfieldvm(free_elements                 );
    readfieldvm(cur_size                      );
    readfieldvm(max_size                      );
    readfieldvm(elem_size                     );
    readfieldvm(alloc_size                    );
    readfieldvm(zone_name                     );
    readfieldvm(bits                          );
    readfieldvm(next_zone                     );
    readfieldvm(call_async_alloc.q_link.next  );
    readfieldvm(call_async_alloc.q_link.prev  );
    readfieldvm(call_async_alloc.func         );
    readfieldvm(call_async_alloc.param0       );
    readfieldvm(call_async_alloc.param1       );
    readfieldvm(call_async_alloc.deadline     );
    readfieldvm(call_async_alloc.state        );
    readfieldvm(lock.lock_data                );
}

static void get_ipc_mqueue(uint32_t p, ipc_mqueue &s) {
    readfieldvm(data.set_queue.wqs_wait_queue.bits                    );
    readfieldvm(data.set_queue.wqs_wait_queue.wq_interlock.lock_data  );
    readfieldvm(data.set_queue.wqs_wait_queue.wq_queue.next           );
    readfieldvm(data.set_queue.wqs_wait_queue.wq_queue.prev           );
    readfieldvm(data.set_queue.wqs_setlinks.next                      );
    readfieldvm(data.set_queue.wqs_setlinks.prev                      );
    readfieldvm(data.set_queue.wqs_refcount                           );
}


/* ==================================================================================== */

vector<kmod_info_t> get_kmod_infos() {
    static uint32_t _kmod = 0;
    kmod_info_t info;
    vector<kmod_info_t> kmod_infos;
    if (!_kmod)
        lookup_name_kernel("_kmod", _kmod);
    if (_kmod) {
        try {
            uint32_t kmod = (uint32_t)kernel_read(_kmod, 4);
            while ((!(kmod & 3)) && kmod) {
                get_kmod_info(kmod, info);
                kmod = info.next;
                kmod_infos.push_back(info);
            }
        } catch (invalid_argument& exc) {
        }
    }
    return kmod_infos;
}

/* ==================================================================================== */

vector<kmod_info_t> gkmod_infos;

static uint32_t _default_pset;
static uint32_t _wait_queue_link;
static uint32_t _ipc_space_kernel;
static uint32_t _first_zone;
static uint32_t _machine_slot;
static uint32_t _kdp;
static uint32_t _debug_buf;
static uint32_t _debug_buf_size;

static void showtaskheader();
static void showtaskint(uint32_t arg0);
static void showportdest(uint32_t arg0);
static void showprocheader();
static void showprocint(uint32_t arg0);

/* ==================================================================================== */

/*
# Kernel gdb macros
#
#  These gdb macros should be useful during kernel development in
#  determining what's going on in the kernel.
#
#  All the convenience variables used by these macros begin with kgm_
*/

//set print asm-demangle on
//set cp-abi gnu-v2

//printf("Loading Kernel GDB Macros package.  Type "help kgm" for more info.\n");

void kgm() {
    printf("");
    printf("These are the gdb macros for kernel debugging.  Type \"help kgm\" for more info.\n");
}

/*
    |  = 10.2 to 10.5.8
    || = 10.5.8
*/

/* kgm
    |        (gdb) target remote-kdp
    |        (gdb) attach <name-of-remote-host>
    |
    | The following macros are available in this package:
    ||    showversion           Displays a string describing the remote kernel version
    |
    |     showalltasks          Display a summary listing of all tasks
    |10.2 showallacts           Display a summary listing of all activations
    ||    showallthreads        Display info about all threads in the system
    |10.2 showallstacks         Display the kernel stacks for all activations
    ||    showallstacks         Display the stack for each thread in the system
    ||    showcurrentthreads    Display info about the thread running on each cpu
    ||    showcurrentstacks     Display the stack for the thread running on each cpu
    |     showallvm             Display a summary listing of all the vm maps
    |     showallvme            Display a summary listing of all the vm map entries
    |     showallipc            Display a summary listing of all the ipc spaces
    |     showallrights         Display a summary listing of all the ipc rights
    |     showallkmods          Display a summary listing of all the kernel modules
    |
    ||    showallclasses        Display info about all OSObject subclasses in the system
    ||    showobject            Show info about an OSObject - its vtable ptr and retain count,
                                & more info for simple container classes.
    ||    showregistry          Show info about all registry entries in the current plane
    ||    showregistryprops     Show info about all registry entries in the current plane, and their properties
    ||    showregistryentry     Show info about a registry entry; its properties and descendants in the current plane
    ||    setregistryplane      Set the plane to be used for the iokit registry macros (pass zero for list)
    |
    |     showtask              Display info about the specified task
    |10.2 showtaskacts          Display the status of all activations in the task
    ||    showtaskthreads       Display info about the threads in the task
    |     showtaskstacks        Display the stack for each thread in the task
    |     showtaskvm            Display info about the specified task's vm_map
    |     showtaskvme           Display info about the task's vm_map entries
    |     showtaskipc           Display info about the specified task's ipc space
    |     showtaskrights        Display info about the task's ipc space entries
    |
    |     showact               Display info about a thread specified by activation
    |     showactstack          Display the stack for a thread specified by activation
    |
    |     showmap               Display info about the specified vm_map
    |     showmapvme            Display a summary list of the specified vm_map's entries
    |
    |     showipc               Display info about the specified ipc space
    |     showrights            Display a summary list of all the rights in an ipc space
    |
    |     showpid               Display info about the process identified by pid
    |     showproc              Display info about the process identified by proc struct
    ||    showprocinfo          Display detailed info about the process identified by proc struct
    ||    showprocfiles         Given a proc_t pointer, display the list of open file descriptors
    ||    showproclocks         Given a proc_t pointer, display the list of advisory file locks
    ||    zombproc              Print out all procs in the zombie list
    ||    allproc               Print out all process in the system not in the zombie list
    ||    zombstacks            Print out all stacks of tasks that are exiting
    |
    ||    showinitchild         Print out all processes in the system which are children of init process
    |
    |     showkmod              Display info about a kernel module
    |     showkmodaddr          Given an address, display the kernel module and offset
    |
    ||    dumpcallqueue         Dump out all the entries given a queue head
    |
    ||    showallmtx            Display info about mutexes usage
    ||    showallrwlck          Display info about reader/writer locks usage
    |
    |     zprint                Display info about the memory zones
    ||    showioalloc           Display info about iokit allocations
    |     paniclog              Display the panic log info
    |
    |     switchtoact           Switch to different context specified by activation
    |     switchtoctx           Switch to different context
    ||    showuserstack         Display numeric backtrace of the user stack for an
    |                           activation
    |
    ||    switchtouserthread    Switch to the user context of the specified thread
    ||    resetstacks           Return to the original kernel context
    |
    ||    resetctx              Reset context
    ||    resume_on             Resume when detaching from gdb
    ||    resume_off            Don't resume when detaching from gdb
    |
    ||    sendcore              Configure kernel to send a coredump to the specified IP
    ||    disablecore           Configure the kernel to disable coredump transmission
    ||    switchtocorethread    Corefile version of "switchtoact"
    ||    resetcorectx          Corefile version of "resetctx"
    |
    ||    readphys              Reads the specified untranslated address
    ||    readphys64            Reads the specified untranslated 64-bit address
    |
    ||    rtentry_showdbg       Print the debug information of a route entry
    ||    rtentry_trash         Walk the list of trash route entries
    |
    ||    mbuf_walkpkt          Walk the mbuf packet chain (m_nextpkt)
    ||    mbuf_walk             Walk the mbuf chain (m_next)
    ||    mbuf_buf2slab         Find the slab structure of the corresponding buffer
    ||    mbuf_buf2mca          Find the mcache audit structure of the corresponding mbuf
    ||    mbuf_showmca          Print the contents of an mbuf mcache audit structure
    ||    mbuf_showactive       Print all active/in-use mbuf objects
    ||    mbuf_showinactive     Print all freed/in-cache mbuf objects
    ||    mbuf_showall          Print all mbuf objects
    ||    mbuf_slabs            Print all slabs in the group
    ||    mbuf_slabstbl         Print slabs table
    ||    mbuf_stat             Print extended mbuf allocator statistics
    |
    ||    mcache_walkobj        Walk the mcache object chain (obj_next)
    ||    mcache_stat           Print all mcaches in the system
    ||    mcache_showcache      Display the number of objects in the cache
    |
    ||    showbootermemorymap   Dump phys memory map from EFI
    |
    ||    systemlog             Display the kernel's printf ring buffer
    |
    ||    showvnodepath         Print the path for a vnode
    ||    showvnodelocks        Display list of advisory locks held/blocked on a vnode
    ||    showallvols           Display a summary of mounted volumes
    ||    showvnode             Display info about one vnode
    ||    showvolvnodes         Display info about all vnodes of a given volume
    ||    showvolbusyvnodes     Display info about busy (iocount!=0) vnodes of a given volume
    ||    showallbusyvnodes     Display info about all busy (iocount!=0) vnodes
    ||    showallvnodes         Display info about all vnodes
    ||    print_vnode           Print out the fields of a vnode struct
    ||    showprocvnodes        Print out all the open fds which are vnodes in a process
    ||    showallprocvnodes     Print out all the open fds which are vnodes in any process
    ||    showmountvnodes       Print the vnode list
    ||    showmountallvnodes    Print the vnode inactive list
    ||    showworkqvnodes       Print the vnode worker list
    ||    shownewvnodes         Print the new vnode list
    |
    ||    ifconfig              display ifconfig-like output
    ||    showifaddrs           show the list of addresses for the given ifp
    ||    showifmultiaddrs      show the list of multicast addresses for the given ifp
    |
    ||    showallpmworkqueues   Display info about all IOPMWorkQueue objects
    ||    showregistrypmstate   Display power management state for all IOPower registry entries
    ||    showioservicepm       Display the IOServicePM object
    ||    showstacksaftertask   showallstacks starting after a given task
    ||    showstacksafterthread showallstacks starting after a given thread
    |
    ||    showMCAstate          Print machine-check register state after MC exception.
    |
    ||    showallgdbstacks      Cause GDB to trace all thread stacks
    ||    showallgdbcorestacks  Corefile equivalent of "showallgdbstacks"
    ||    kdp-reenter           Schedule reentry into the debugger and continue.
    ||    kdp-reboot            Restart remote target
    |
    ||    zstack                Print zalloc caller stack (zone leak debugging)
    ||    findoldest            Find oldest zone leak debugging record
    ||    countpcs              Print how often a pc occurs in the zone leak log
    |
    |
    | Type "help <macro>" for more specific help on a particular macro.
    | Type "show user <macro>" to see what the macro is really doing.
*/

/*
// 10.4.1
// This macro should appear before any symbol references, to facilitate
// a gdb "source" without a loaded symbol file.
static void showversion() {
    printf("%s\n", *(char **)0x501C);
}
*/
/* showversion
Syntax: showversion
| Read the kernel version string from a fixed address in low
| memory. Useful if you don't know which kernel is on the other end,
| and need to find the appropriate symbols. Beware that if you've
| loaded a symbol file, but aren't connected to a remote target,
| the version string from the symbol file will be displayed instead.
| This macro expects to be connected to the remote kernel to function
| correctly.
*/

/*
uint32_t kgm_mtype = ((struct mach_header)_mh_execute_header).cputype;
*/

// This option tells gdb to relax its stack tracing heuristics
// Useful for debugging across stack switches
// (to the interrupt stack, for instance). Requires gdb-675 or greater.
// Don't do this for arm as a workaround to 5486905
/*
if (kgm_mtype != 12) {
    set backtrace sanity-checks off
}
*/

/*
kgm_dummy = &proc0
kgm_dummy = &kmod
*/

#if 0
static uint32_t kgm_reg_depth = 0;
static uint32_t kgm_reg_plane = 0; // (void **) gIOServicePlane;
static uint32_t kgm_namekey = 0; // (OSSymbol *)
static uint32_t kgm_childkey = 0; // (OSSymbol *)

static uint32_t kgm_show_object_addrs = 0;
static uint32_t kgm_show_object_retain = 0;
static uint32_t kgm_show_props = 0;
#endif

static uint32_t kgm_show_kmod_syms = 0;

static void showkmodheader() {
    printf("kmod        address     hdr_size    size        id    refs     version  name\n");
}

static void showkmodint(kmod_info_t &info) {
    printf("0x%08x  ", info.kmod);
    printf("0x%08x  ", info.address);
    printf("0x%08x  ", info.hdr_size);
    printf("0x%08x  ", info.size);
    printf("%3d  ", info.id);
    printf("%5d  ", info.reference_count);
    printf("%10s  ", info.version);
    printf("%s\n", info.name);
}

static uint32_t kgm_kmodmin = 0xffffffff;
static uint32_t kgm_fkmodmin = 0x00000000;
static uint32_t kgm_kmodmax = 0x00000000;
static uint32_t kgm_fkmodmax = 0xffffffff;
static kmod_info_t *kgm_pkmod = 0;
static uint32_t kgm_pkmodst = 0;
static uint32_t kgm_pkmoden = 0;

static void showkmodaddrint(uint32_t arg0) {
    printf("0x%x" , arg0);
    if (((unsigned int)arg0 >= (unsigned int)kgm_pkmodst) && ((unsigned int)arg0 < (unsigned int)kgm_pkmoden)) {
        int kgm_off = ((unsigned int)arg0 - (unsigned int)kgm_pkmodst);
        printf(" <%s + 0x%x>", kgm_pkmod->name, kgm_off);
    }
    else
    {
        if ( ((unsigned int)arg0 <= (unsigned int)kgm_fkmodmax) && ((unsigned int)arg0 >= (unsigned int)kgm_fkmodmin)) {
            gkmod_infos = get_kmod_infos();
            for (auto &kgm_kmod : gkmod_infos) {
                kmod_info_t *kgm_kmodp = &kgm_kmod;
                if (kgm_kmod.address && (kgm_kmod.address < kgm_kmodmin)) {
                    kgm_kmodmin = kgm_kmod.address;
                }
                if ((kgm_kmod.address + kgm_kmod.size) > kgm_kmodmax) {
                    kgm_kmodmax = kgm_kmod.address + kgm_kmod.size;
                }
                int kgm_off = ((unsigned int)arg0 - (unsigned int)kgm_kmod.address);
                if ((kgm_kmod.address <= arg0) && (kgm_off <= kgm_kmod.size)) {
                    printf(" <%s + 0x%x>", kgm_kmodp->name, kgm_off);
                    kgm_pkmod = kgm_kmodp;
                    kgm_pkmodst = kgm_kmod.address;
                    kgm_pkmoden = kgm_pkmodst + kgm_kmod.size;
                    break;
                }
            }
            if (!kgm_pkmod) {
                kgm_fkmodmin = kgm_kmodmin;
                kgm_fkmodmax = kgm_kmodmax;
            }
        }
    }
}

void showkmodaddr(uint32_t arg0) {
    showkmodaddrint(arg0);
}
/* showkmodaddr
Syntax: (gdb) showkmodaddr <addr>
| Given an address, print the offset and name for the kmod containing it
*/

void showkmod(uint32_t arg0) {
    kmod_info_t info;
    get_kmod_info(arg0, info);
    showkmodheader();
    showkmodint(info);
}
/* showkmod
Syntax: (gdb) showkmod <kmod>
| Routine to print info about a kernel module
*/

void showallkmods() {
    gkmod_infos = get_kmod_infos();
    showkmodheader();
    for (auto &info : gkmod_infos) {
        showkmodint(info);
    }
}
/* showallkmods
Syntax: (gdb) showallkmods
| Routine to print a summary listing of all the kernel modules
*/

static void showactheader() {
    printf("            thread      ");
    printf("processor   pri  state  wait_queue  wait_event\n");
}

#if 0
// 10.5.8
static void showactint(uint32_t arg0, int arg1) {
    printf("            0x%08x  ", arg0);
    thread_shuttle kgm_thread;
    get_thread_shuttle(arg0, kgm_thread);
    printf("0x%08x  ", kgm_thread.last_processor);
    printf("%3d  ", kgm_thread.sched_pri);
    int32_t kgm_state = kgm_thread.state;
    if (kgm_state & 0x80) {
        printf("I");
    }
    if (kgm_state & 0x40) {
        printf("P");
    }
    if (kgm_state & 0x20) {
        printf("A");
    }
    if (kgm_state & 0x10) {
        printf("H");
    }
    if (kgm_state & 0x08) {
        printf("U");
    }
    if (kgm_state & 0x04) {
        printf("R");
    }
    if (kgm_state & 0x02) {
        printf("S");
    }
    if (kgm_state & 0x01) {
        printf("W\t");
        printf("0x%08x  ", kgm_thread.wait_queue);
            if (kgm_thread.wait_event > sectPRELINKB) {
                && (arg1 != 2) && (kgm_show_kmod_syms == 0))
                    showkmodaddr(kgm_thread.wait_event);
            }
            else
            {
                    output /a (unsigned) kgm_thread.wait_event
            }
            if ((kgm_thread.uthread != 0)) {
                kgm_uthread = (struct uthread *)kgm_thread.uthread
                if ((kgm_uthread->uu_wmesg != 0)) {
                            printf(" \"%s\"", kgm_uthread->uu_wmesg);
                }
            }
    }
    if (arg1 != 0) {
        if ((kgm_thread.kernel_stack != 0)) {
            if ((kgm_thread.reserved_stack != 0)) {
                    printf("\n\t\treserved_stack=0x%08x", kgm_thread.reserved_stack);
            }
            printf("\n\t\tkernel_stack=0x%08x", kgm_thread.kernel_stack);
            if ((kgm_mtype == 18)) {
                    mysp = kgm_thread.machine.pcb->save_r1
            }
            if ((kgm_mtype == 7)) {
                    kgm_statep = (struct x86_kernel_state32 *) \
                            (kgm_thread->kernel_stack + 0x4000 \
                             - sizeof(struct x86_kernel_state32))
                    mysp = kgm_statep->k_ebp
            }
            if ((kgm_mtype == 12)) {
                    if ((arg0 == r9)) {
                            mysp = r7
                    }
                    else
                    {
                            kgm_statep = (struct arm_saved_state *)kgm_thread.machine.kstackptr
                            mysp = kgm_statep->r[7]
                    }
            }
            prevsp = mysp - 16
            printf("\n\t\tstacktop=0x%08x", mysp);
            if ((kgm_mtype == 18)) {
                    stkmask = 0xf
            }
            else
            {
                    stkmask = 0x3
            }
            kgm_return = 0
            while (mysp != 0) && ((mysp & stkmask) == 0) \
                  && (mysp != prevsp) \
                  && ((((unsigned) mysp ^ (unsigned) prevsp) < 0x2000) \
                  || (((unsigned)mysp < ((unsigned) (kgm_thread->kernel_stack+0x4000))) \
                  && ((unsigned)mysp > (unsigned) (kgm_thread->kernel_stack))))
                    printf("\n\t\t0x%08x  ", mysp);
                    if ((kgm_mtype == 18)) {
                            kgm_return = *(mysp + 8)
                    }
                    if ((kgm_mtype == 7)) {
                            kgm_return = *(mysp + 4)
                    }
                    if ((kgm_mtype == 12)) {
                            kgm_return = *(mysp + 4)
                    }
                    if ((((unsigned) kgm_return > (unsigned) sectPRELINKB) \) {
                        && (kgm_show_kmod_syms == 0))
                            showkmodaddr(kgm_return);
                    }
                    else
                    {
                            output /a (unsigned) kgm_return
                    }
                    prevsp = mysp
                    mysp = * mysp
            }
            kgm_return = 0
            printf("\n\t\tstackbottom=0x%08x", prevsp);
        }
        else
        {
            std::string name = get_name(kgm_thread.continuation);
            printf("\n\t\t\tcontinuation=");
            printf("0x%08x %s", kgm_thread.continuation, name.c_str());
        }
        printf("\n");
    }
    else
    {
        printf("\n");
    }
}
#endif

// 10.2
static void showactint(thread_activation &kgm_actp, int arg1) {
   printf("            0x%08x  ", kgm_actp.thread_activation);
   if (kgm_actp.thread) {
        thread_shuttle kgm_thread;
        get_thread_shuttle(kgm_actp.thread, kgm_thread);
        printf("0x%08x  ", kgm_actp.thread);
        printf("%3d  ", kgm_thread.sched_pri);
        int kgm_state = kgm_thread.state;
        if (kgm_state & 0x80) {
            printf("I" );
        }
        if (kgm_state & 0x40) {
            printf("P" );
        }
        if (kgm_state & 0x20) {
            printf("A" );
        }
        if (kgm_state & 0x10) {
            printf("H" );
        }
        if (kgm_state & 0x08) {
            printf("U" );
        }
        if (kgm_state & 0x04) {
            printf("R" );
        }
        if (kgm_state & 0x02) {
            printf("S" );
        }
        if (kgm_state & 0x01) {
            printf("W\t" );
            printf("0x%08x  ", kgm_thread.wait_queue);
            printf("0x%llx", kgm_thread.wait_event);
        }
        if (arg1 != 0) {
            if ((kgm_thread.kernel_stack != 0)) {
                if ((kgm_thread.stack_privilege != 0)) {
                    printf("\n\t\tstack_privilege=0x%08x", kgm_thread.stack_privilege);
                }
                printf("\n\t\tkernel_stack=0x%08x", kgm_thread.kernel_stack);
                savearea pcb;
                get_savearea(kgm_actp.mact.pcb, pcb);
                uint32_t mysp = pcb.save_r1;
                uint32_t prevsp = 0;
                printf("\n\t\tstacktop=0x%08x", mysp);
                while ((mysp != 0) && ((mysp & 0xf) == 0) && (mysp < 0xb0000000) && (mysp > prevsp)) {
                    printf("\n\t\t0x%08x  ", mysp);
                    uint32_t kgm_return = get_32vm(mysp + 8);
                    if (((kgm_return > 0) && (kgm_return < 0x40000000))) {
                        std::string name = get_name(kgm_return);
                        showkmodaddr(kgm_return);
                        printf(" %s", name.c_str());
                    }
                    else
                    {
                        printf(" 0x%08x", kgm_return);
                    }
                    prevsp = mysp;
                    mysp = get_32vm(mysp);
                }
                printf("\n\t\tstackbottom=0x%08x", prevsp);
            }
            else
            {
                std::string name = get_name(kgm_thread.continuation);
                printf("\n\t\t\tcontinuation=");
                printf("0x%08x %s", kgm_thread.continuation, name.c_str());
            }
            printf("\n");
        }
        else
        {
            printf("\n");
        }
    }
}

void showact(uint32_t arg0) {
    showactheader();
    thread_activation kgm_actp;
    get_thread_activation(arg0, kgm_actp);
    showactint(kgm_actp, 0);
}
/* showact
| Routine to print out the state of a specific thread activation.
| The following is the syntax:
|     (gdb) showact <activation>
*/


void showactstack(uint32_t arg0) {
    showactheader();
    thread_activation kgm_act;
    get_thread_activation(arg0, kgm_act);
    showactint(kgm_act, 1);
}
/* showactstack
| Routine to print out the stack of a specific thread activation.
| The following is the syntax:
|     (gdb) showactstack <activation>
*/

void showallacts() {
    if (!_default_pset)
        lookup_name_kernel("_default_pset", _default_pset);
    if (!_default_pset)
        return;
    uint32_t kgm_head_taskp = get_addr(processor_set, _default_pset, tasks);
    uint32_t kgm_taskp = get_fieldvm(queue_head_t, kgm_head_taskp, next); // (struct task *)
    while (kgm_taskp != kgm_head_taskp) {
        showtaskheader();
        showtaskint(kgm_taskp);
        showactheader();
        uint32_t kgm_head_actp = get_addr(task, kgm_taskp, thr_acts);
        uint32_t kgm_actp = get_fieldvm(task, kgm_taskp, thr_acts.next);
        while (kgm_actp != kgm_head_actp) {
            thread_activation kgm_act;
            get_thread_activation(kgm_actp, kgm_act);
            showactint(kgm_act, 0);
            kgm_actp = kgm_act.thr_acts.next;
        }
        printf("\n");
        kgm_taskp = get_fieldvm(task, kgm_taskp, pset_tasks.next); // (struct task *)
    }
}
/* showallacts
| Routine to print out a summary listing of all the thread activations.
| The following is the syntax:
|     (gdb) showallacts
*/


void showallstacks() {
    if (!_default_pset)
        lookup_name_kernel("_default_pset", _default_pset);
    if (!_default_pset)
        return;
    uint32_t kgm_head_taskp = get_addr(processor_set, _default_pset, tasks);
    uint32_t kgm_taskp = get_fieldvm(queue_head_t, kgm_head_taskp, next); // (struct task *)
    while (kgm_taskp != kgm_head_taskp) {
        showtaskheader();
        showtaskint(kgm_taskp);
        uint32_t kgm_head_actp = get_addr(task, kgm_taskp, thr_acts);
        uint32_t kgm_actp = get_fieldvm(task, kgm_taskp, thr_acts.next);
        while (kgm_actp != kgm_head_actp) {
            showactheader();
            thread_activation kgm_act;
            get_thread_activation(kgm_actp, kgm_act);
            showactint(kgm_act, 1);
            kgm_actp = kgm_act.thr_acts.next;
        }
        printf("\n");
        kgm_taskp = get_fieldvm(task, kgm_taskp, pset_tasks.next); // (struct task *)
    }
}
/* showallstacks
| Routine to print out a summary listing of all the thread kernel stacks.
| The following is the syntax:
|     (gdb) showallstacks
*/

static void showwaiterheader() {
    printf("waiters     activation  ");
    printf("thread      pri  state  wait_queue  wait_event\n");
}

static void showwaitqwaiters(uint32_t arg0) {
    if (!_wait_queue_link)
        lookup_name_kernel("_wait_queue_link", _wait_queue_link);
    if (!_wait_queue_link)
        return;

    uint32_t kgm_w_waitqp = arg0; // (struct wait_queue *)
    uint32_t kgm_w_linksp = get_addr(wait_queue, kgm_w_waitqp, wq_queue);
    uint32_t kgm_w_wqe = (uint32_t)get_fieldvm(queue_head_t, kgm_w_linksp, next); // (struct wait_queue_element *)
    uint32_t kgm_w_found = 0;
    while (kgm_w_wqe != kgm_w_linksp) {
        if (get_fieldvm(wait_queue_element, kgm_w_wqe, wqe_type) != _wait_queue_link) {
                if (!kgm_w_found) {
                        kgm_w_found = 1;
                        showwaiterheader();
                }
                uint32_t kgm_w_shuttle = kgm_w_wqe; // (struct thread_shuttle *)
                uint32_t kgm_actp = (uint32_t)get_fieldvm(thread_shuttle, kgm_w_shuttle, top_act);
                thread_activation kgm_act;
                get_thread_activation(kgm_actp, kgm_act);
                showactint(kgm_act, 0);
        }
        kgm_w_wqe = (uint32_t)get_fieldvm(wait_queue_element, kgm_w_wqe, wqe_links.next); // (struct wait_queue_element *)
    }
}

static void showwaitqwaitercount(uint32_t arg0) {
    if (!_wait_queue_link)
        lookup_name_kernel("_wait_queue_link", _wait_queue_link);
    if (!_wait_queue_link)
        return;

    uint32_t kgm_wc_waitqp = arg0; // (struct wait_queue *)
    uint32_t kgm_wc_linksp = get_addr(wait_queue, kgm_wc_waitqp, wq_queue);
    uint32_t kgm_wc_wqe = (uint32_t)get_fieldvm(queue_head_t, kgm_wc_linksp, next); // (struct wait_queue_element *)
    uint32_t kgm_wc_count = 0;
    while (kgm_wc_wqe != kgm_wc_linksp) {
        if (get_fieldvm(wait_queue_element, kgm_wc_wqe, wqe_type) != _wait_queue_link) {
            kgm_wc_count = kgm_wc_count + 1;
        }
        kgm_wc_wqe = (uint32_t)get_fieldvm(wait_queue_element, kgm_wc_wqe, wqe_links.next); // (struct wait_queue_element *)
    }
    printf("0x%08x  ", kgm_wc_count);
}

static void showwaitqmembercount(uint32_t arg0) {
    uint32_t kgm_mc_waitqsetp = arg0; // (struct wait_queue_set *)
    uint32_t kgm_mc_setlinksp = get_addr(wait_queue_set, kgm_mc_waitqsetp, wqs_setlinks);
    uint32_t kgm_mc_wql = (uint32_t)get_fieldvm(queue_head_t, kgm_mc_setlinksp, next); // (struct wait_queue_link *)
    uint32_t kgm_mc_count = 0;
    while ((queue_entry_t)kgm_mc_wql != (queue_entry_t)kgm_mc_setlinksp) {
        kgm_mc_count = kgm_mc_count + 1;
        kgm_mc_wql = (uint32_t)get_fieldvm(wait_queue_link, kgm_mc_wql, wql_setlinks.next); // (struct wait_queue_link *)
    }
    printf("0x%08x  ", kgm_mc_count);
}


static void showwaitqmemberheader() {
    printf("set-members wait_queue  interlock   ");
    printf("pol  type   member_cnt  waiter_cnt\n");
}

static void showwaitqmemberint(uint32_t arg0) {
    uint32_t kgm_m_waitqp = arg0; // (struct wait_queue *)
    printf("            0x%08x  ", kgm_m_waitqp);
    printf("0x%08x  ", get_fieldvm(wait_queue, kgm_m_waitqp, wq_interlock.lock_data));
    wait_queue kgm_m_waitq;
    get_wait_queue(kgm_m_waitqp, kgm_m_waitq);
    if (kgm_m_waitq.wq_fifo) {
        printf("Fifo ");
    }
    else
    {
        printf("Prio ");
    }
    if (kgm_m_waitq.wq_type == 0xf1d1) {
        printf("Set    ");
        showwaitqmembercount(kgm_m_waitqp);
    }
    else
    {
        printf("Que    0x00000000  ");
    }
    showwaitqwaitercount(kgm_m_waitqp);
    printf("\n");
}


static void showwaitqmemberofheader() {
    printf("member-of   wait_queue  interlock   ");
    printf("pol  type   member_cnt  waiter_cnt\n");
}

static void showwaitqmemberof(uint32_t arg0) {
    if (!_wait_queue_link)
        lookup_name_kernel("_wait_queue_link", _wait_queue_link);
    if (!_wait_queue_link)
        return;

    uint32_t kgm_mo_waitqp = arg0; // (struct wait_queue *)
    uint32_t kgm_mo_linksp = get_addr(wait_queue, kgm_mo_waitqp, wq_queue);
    uint32_t kgm_mo_wqe = (uint32_t)get_fieldvm(queue_head_t, kgm_mo_linksp, next); // (struct wait_queue_element *)
    uint32_t kgm_mo_found = 0;
    while ((queue_entry_t)kgm_mo_wqe != (queue_entry_t)kgm_mo_linksp) {
        if (get_fieldvm(wait_queue_element, kgm_mo_wqe, wqe_type) == _wait_queue_link) {
                if (!kgm_mo_found) {
                    kgm_mo_found = 1;
                    showwaitqmemberofheader();
                }
                uint32_t kgm_mo_wqlp = kgm_mo_wqe; // (struct wait_queue_link *)
                uint32_t kgm_mo_wqsetp =
                    (uint32_t)get_fieldvm(wait_queue_link, kgm_mo_wqlp, wql_setqueue); // (struct wait_queue *)
                showwaitqmemberint(kgm_mo_wqsetp);
        }
        kgm_mo_wqe = (uint32_t)get_fieldvm(wait_queue_element, kgm_mo_wqe, wqe_links.next); // (struct wait_queue_element *)
    }
}

static void showwaitqmembers(uint32_t arg0) {
    uint32_t kgm_ms_waitqsetp = arg0; // (struct wait_queue_set *)
    uint32_t kgm_ms_setlinksp = get_addr(wait_queue_set, kgm_ms_waitqsetp, wqs_setlinks);
    uint32_t kgm_ms_wql = (uint32_t)get_fieldvm(queue_head_t, kgm_ms_setlinksp, next); // (struct wait_queue_link *)
    uint32_t kgm_ms_found = 0;
    while ((queue_entry_t)kgm_ms_wql != (queue_entry_t)kgm_ms_setlinksp) {
        uint32_t kgm_ms_waitqp = (uint32_t)get_fieldvm(wait_queue_link, kgm_ms_wql, wql_element.wqe_queue);
        if (!kgm_ms_found) {
            showwaitqmemberheader();
            kgm_ms_found = 1;
        }
        showwaitqmemberint(kgm_ms_waitqp);
        kgm_ms_wql = (uint32_t)get_fieldvm(wait_queue_link, kgm_ms_wql, wql_setlinks.next); // (struct wait_queue_link *)
    }
}

static void showwaitqheader() {
    printf("wait_queue  ref_count   interlock   ");
    printf("pol  type   member_cnt  waiter_cnt\n");
}

static void showwaitqint(uint32_t arg0) {
    uint32_t kgm_waitqp = arg0; // (struct wait_queue *)
    printf("0x%08x  ", kgm_waitqp);
    wait_queue kgm_waitq;
    get_wait_queue(kgm_waitqp, kgm_waitq);
    if (kgm_waitq.wq_type == 0xf1d1) {
        printf("0x%08x  ", get_fieldvm(wait_queue_set, kgm_waitqp, wqs_refcount)); // (struct wait_queue_set *)
    }
    else
    {
        printf("0x00000000  ");
    }
    printf("0x%08x  ", kgm_waitq.wq_interlock.lock_data);
    if (kgm_waitq.wq_fifo) {
        printf("Fifo ");
    }
    else
    {
        printf("Prio ");
    }
    if (kgm_waitq.wq_type == 0xf1d1) {
        printf("Set    ");
        showwaitqmembercount(kgm_waitqp);
    }
    else
    {
        printf("Que    0x00000000  ");
    }
    showwaitqwaitercount(kgm_waitqp);
    printf("\n");
}

static void showwaitq(uint32_t arg0) {
    uint32_t kgm_waitq1p = (wait_queue_t)arg0;
    showwaitqheader();
    showwaitqint(kgm_waitq1p);
    wait_queue kgm_waitq1;
    get_wait_queue(kgm_waitq1p, kgm_waitq1);
    if (kgm_waitq1.wq_type == 0xf1d1) {
        showwaitqmembers(kgm_waitq1p);
    }
    else
    {
        showwaitqmemberof(kgm_waitq1p);
    }
    showwaitqwaiters(kgm_waitq1p);
}

static void showmapheader() {
    printf("vm_map      pmap        vm_size    ");
    printf("#ents rpage  hint        first_free\n");
}

static void showvmeheader() {
    printf("            entry       start       ");
    printf("prot #page  object      offset\n");
}

static void showvmint(uint32_t arg0, uint32_t arg1) {
    uint32_t kgm_mapp = (vm_map_t)arg0;
    vm_map kgm_map;
    get_vm_map(kgm_mapp, kgm_map);
    printf("0x%08x  ", arg0);
    printf("0x%08x  ", kgm_map.pmap);
    printf("0x%08x  ", kgm_map.size);
    printf("%3d  ", kgm_map.hdr.nentries);
    printf("%5d  ", get_fieldvm(pmap, kgm_map.pmap, stats.resident_count));
    printf("0x%08x  ", kgm_map.hint);
    printf("0x%08x\n", kgm_map.first_free);
    if (arg1 != 0) {
        showvmeheader();
        uint32_t kgm_head_vmep = get_addr(vm_map, kgm_mapp, hdr.links);
        uint32_t kgm_vmep = kgm_map.hdr.links.next; // (vm_map_entry *)
        while ((kgm_vmep != 0) && (kgm_vmep != kgm_head_vmep)) {
            vm_map_entry kgm_vme;
            get_vm_map_entry(kgm_vmep, kgm_vme);
            printf("            0x%08x  ", kgm_vmep);
            printf("0x%08x  ", kgm_vme.links.start);
            printf("%1x", kgm_vme.protection);
            printf("%1x", kgm_vme.max_protection);
            if (kgm_vme.inheritance == 0x0) {
                printf("S");
            }
            if (kgm_vme.inheritance == 0x1) {
                printf("C");
            }
            if (kgm_vme.inheritance == 0x2) {
                printf("-");
            }
            if (kgm_vme.inheritance == 0x3) {
                printf("D");
            }
            if (kgm_vme.is_sub_map) {
                printf("s ");
            }
            else
            {
                if (kgm_vme.needs_copy) {
                    printf("n ");
                }
                else
                {
                    printf("  ");
                }
            }
            printf("%5d  ",(kgm_vme.links.end - kgm_vme.links.start) >> 12);
            printf("0x%08x  ", kgm_vme.object.vm_object);
            printf("0x%08llx\n", kgm_vme.offset);
            kgm_vmep = kgm_vme.links.next;
        }
    }
    printf("\n");
}


void showmapvme(uint32_t arg0) {
    showmapheader();
    showvmint(arg0, 1);
}
/* showmapvme
| Routine to print out a summary listing of all the entries in a vm_map
| The following is the syntax:
|     (gdb) showmapvme <vm_map>
*/


void showmap(uint32_t arg0) {
    showmapheader();
    showvmint(arg0, 0);
}
/* showmap
| Routine to print out a summary description of a vm_map
| The following is the syntax:
|     (gdb) showmap <vm_map>
*/

void showallvm() {
    if (!_default_pset)
        lookup_name_kernel("_default_pset", _default_pset);
    if (!_default_pset)
        return;

    uint32_t kgm_head_taskp = get_addr(processor_set, _default_pset, tasks);
    uint32_t kgm_taskp = (uint32_t)get_fieldvm(queue_head_t, kgm_head_taskp, next); // (struct task *)
    while (kgm_taskp != kgm_head_taskp) {
        showtaskheader();
        showmapheader();
        showtaskint(kgm_taskp);
        showvmint((uint32_t)get_fieldvm(task, kgm_taskp, map), 0);
        kgm_taskp = (uint32_t)get_fieldvm(task, kgm_taskp, pset_tasks.next); // (struct task *)
    }
}
/* showallvm
| Routine to print a summary listing of all the vm maps
| The following is the syntax:
|     (gdb) showallvm
*/


void showallvme() {
    if (!_default_pset)
        lookup_name_kernel("_default_pset", _default_pset);
    if (!_default_pset)
        return;

    uint32_t kgm_head_taskp = get_addr(processor_set, _default_pset, tasks);
    uint32_t kgm_taskp = (uint32_t)get_fieldvm(queue_head_t, kgm_head_taskp, next); // (struct task *)
    while (kgm_taskp != kgm_head_taskp) {
        showtaskheader();
        showmapheader();
        showtaskint(kgm_taskp);
        showvmint((uint32_t)get_fieldvm(task, kgm_taskp, map), 1);
        kgm_taskp = (uint32_t)get_fieldvm(task, kgm_taskp, pset_tasks.next); // (struct task *)
    }
}
/* showallvme
| Routine to print a summary listing of all the vm map entries
| The following is the syntax:
|     (gdb) showallvme
*/


static void showipcheader() {
    printf("ipc_space   is_table    table_next ");
    printf("flags tsize  splaytree   splaybase\n");
}

static void showipceheader() {
    printf("            name        object      ");
    printf("rite urefs  destname    destination\n");
}

static void showipceint(uint32_t arg0, uint32_t arg1) {
    ipc_entry kgm_ie;
    get_ipc_entry(arg0, kgm_ie);

    printf("            0x%08x  ", arg1);
    printf("0x%08x  ", kgm_ie.ie_object);
    if (kgm_ie.ie_bits & 0x00100000) {
        printf("Dead ");
        printf("%5d\n", kgm_ie.ie_bits & 0xffff);
    }
    else
    {
        if (kgm_ie.ie_bits & 0x00080000) {
            printf("SET  ");
            printf("%5d\n", kgm_ie.ie_bits & 0xffff);
        }
        else
        {
            if (kgm_ie.ie_bits & 0x00010000) {
                if (kgm_ie.ie_bits & 0x00020000) {
                    printf(" SR");
                }
                else
                {
                    printf("  S");
                }
            }
            else
            {
                if (kgm_ie.ie_bits & 0x00020000) {
                   printf("  R");
                }
            }
            if (kgm_ie.ie_bits & 0x00040000) {
                printf("  O");
            }
            if (kgm_ie.index.request) {
                printf("n");
            }
            else
            {
                printf(" ");
            }
            if (kgm_ie.ie_bits & 0x00800000) {
                printf("c");
            }
            else
            {
                printf(" ");
            }
            printf("%5d  ", kgm_ie.ie_bits & 0xffff);
            showportdest(kgm_ie.ie_object);
        }
    }
}

static uint32_t kgm_destspacep = 0;

static void showipcint(uint32_t arg0, uint32_t arg1) {
    uint32_t kgm_isp = arg0; // (ipc_space_t)
    ipc_space kgm_is;
    get_ipc_space(kgm_isp, kgm_is);
    printf("0x%08x  ", arg0);
    printf("0x%08x  ", kgm_is.is_table);
    printf("0x%08x  ", kgm_is.is_table_next);
    if (kgm_is.is_growing != 0) {
        printf("G");
    }
    else
    {
        printf(" ");
    }
    if (kgm_is.is_fast != 0) {
        printf("F");
    }
    else
    {
        printf(" ");
    }
    if (kgm_is.is_active != 0) {
        printf("A  ");
    }
    else
    {
        printf("   ");
    }
    printf("%5d  ", kgm_is.is_table_size);
    printf("0x%08x  ", kgm_is.is_tree_total);
    printf("0x%08x\n", (uint32_t)get_addr(ipc_space, kgm_isp, is_tree));
    if (arg1 != 0) {
        showipceheader();
        uint32_t kgm_iindex = 0;
        uint32_t kgm_iep = kgm_is.is_table;
        kgm_destspacep = 0; // (ipc_space_t)
        while (kgm_iindex < kgm_is.is_table_size) {
            ipc_entry kgm_ie;
            get_ipc_entry(kgm_iep, kgm_ie);
            if (kgm_ie.ie_bits & 0x001f0000) {
                uint32_t kgm_name = ((kgm_iindex << 8)|(kgm_ie.ie_bits >> 24));
                showipceint(kgm_iep, kgm_name);
            }
            kgm_iindex = kgm_iindex + 1;
            kgm_iep = kgm_iep + sizeof(ipc_entry);
        }
        if (kgm_is.is_tree_total) {
            printf("Still need to write tree traversal\n");
        }
    }
    printf("\n");
}


void showipc(uint32_t arg0) {
    uint32_t kgm_isp = arg0; // (ipc_space_t)
    showipcheader();
    showipcint(kgm_isp, 0);
}
/* showipc
| Routine to print the status of the specified ipc space
| The following is the syntax:
|     (gdb) showipc <ipc_space>
*/

void showrights(uint32_t arg0) {
    uint32_t kgm_isp = arg0; // (ipc_space_t)
    showipcheader();
    showipcint(kgm_isp, 1);
}
/* showrights
| Routine to print a summary list of all the rights in a specified ipc space
| The following is the syntax:
|     (gdb) showrights <ipc_space>
*/


void showtaskipc(uint32_t arg0) {
    uint32_t kgm_taskp = arg0; // (task_t)
    showtaskheader();
    showipcheader();
    showtaskint(kgm_taskp);
    showipcint((uint32_t)get_fieldvm(task, kgm_taskp, itk_space), 0);
}
/* showtaskipc
| Routine to print the status of the ipc space for a task
| The following is the syntax:
|     (gdb) showtaskipc <task>
*/


void showtaskrights(uint32_t arg0) {
    uint32_t kgm_taskp = arg0; // (task_t)
    showtaskheader();
    showipcheader();
    showtaskint(kgm_taskp);
    showipcint((uint32_t)get_fieldvm(task, kgm_taskp, itk_space), 1);
}
/* showtaskrights
| Routine to print a summary listing of all the ipc rights for a task
| The following is the syntax:
|     (gdb) showtaskrights <task>
*/

void showallipc() {
    if (!_default_pset)
        lookup_name_kernel("_default_pset", _default_pset);
    if (!_default_pset)
        return;

    uint32_t kgm_head_taskp = get_addr(processor_set, _default_pset, tasks);
    uint32_t kgm_taskp = (uint32_t)get_fieldvm(queue_head_t, kgm_head_taskp, next); // (struct task *)
    while (kgm_taskp != kgm_head_taskp) {
        showtaskheader();
        showipcheader();
        showtaskint(kgm_taskp);
        showipcint((uint32_t)get_fieldvm(task, kgm_taskp, itk_space), 0);
        kgm_taskp = (uint32_t)get_fieldvm(task, kgm_taskp, pset_tasks.next); // (struct task *)
    }
}
/* showallipc
| Routine to print a summary listing of all the ipc spaces
| The following is the syntax:
|     (gdb) showallipc
*/


void showallrights() {
    if (!_default_pset)
        lookup_name_kernel("_default_pset", _default_pset);
    if (!_default_pset)
        return;

    uint32_t kgm_head_taskp = get_addr(processor_set, _default_pset, tasks);
    uint32_t kgm_taskp = (uint32_t)get_fieldvm(queue_head_t, kgm_head_taskp, next); // (struct task *)
    while (kgm_taskp != kgm_head_taskp) {
        showtaskheader();
        showipcheader();
        showtaskint(kgm_taskp);
        showipcint((uint32_t)get_fieldvm(task, kgm_taskp, itk_space), 1);
        kgm_taskp = (uint32_t)get_fieldvm(task, kgm_taskp, pset_tasks.next); // (struct task *)
    }
}
/* showallrights
| Routine to print a summary listing of all the ipc rights
| The following is the syntax:
|     (gdb) showallrights
*/


void showtaskvm(uint32_t arg0) {
    uint32_t kgm_taskp = arg0; // (task_t)
    showtaskheader();
    showmapheader();
    showtaskint(kgm_taskp);
    showvmint((uint32_t)get_fieldvm(task, kgm_taskp, map), 0);
}
/* showtaskvm
| Routine to print out a summary description of a task's vm_map
| The following is the syntax:
|     (gdb) showtaskvm <task>
*/

void showtaskvme(uint32_t arg0) {
    uint32_t kgm_taskp = arg0; // (task_t)
    showtaskheader();
    showmapheader();
    showtaskint(kgm_taskp);
    showvmint((uint32_t)get_fieldvm(task, kgm_taskp, map), 1);
}
/* showtaskvme
| Routine to print out a summary listing of a task's vm_map_entries
| The following is the syntax:
|     (gdb) showtaskvme <task>
*/


static void showtaskheader() {
    printf("task        vm_map      ipc_space  #acts  ");
    showprocheader();
}


static void showtaskint(uint32_t arg0) {
    uint32_t kgm_task = arg0; // *(struct task *)
    printf("0x%08x  ", arg0);
    printf("0x%08x  ", (uint32_t)get_fieldvm(task, kgm_task, map));
    printf("0x%08x  ", (uint32_t)get_fieldvm(task, kgm_task, itk_space));
    printf("%3d  ", (uint32_t)get_fieldvm(task, kgm_task, thr_act_count));
    showprocint((uint32_t)get_fieldvm(task, kgm_task, bsd_info));
}

void showtask(uint32_t arg0) {
    showtaskheader();
    showtaskint(arg0);
}
/* showtask
| Routine to print out info about a task.
| The following is the syntax:
|     (gdb) showtask <task>
*/


void showtaskacts(uint32_t arg0) {
    showtaskheader();
    uint32_t kgm_taskp = arg0; // (struct task *)
    showtaskint(kgm_taskp);
    showactheader();
    uint32_t kgm_head_actp = get_addr(task, kgm_taskp, thr_acts);
    uint32_t kgm_actp = (uint32_t)get_fieldvm(task, kgm_taskp, thr_acts.next); // (struct thread_activation *)
    while (kgm_actp != kgm_head_actp) {
        thread_activation kgm_act;
        get_thread_activation(kgm_actp, kgm_act);
        showactint(kgm_act, 0);
        kgm_actp = kgm_act.thr_acts.next; // (struct thread_activation *)
    }
}
/* showtaskacts
| Routine to print a summary listing of the activations in a task
| The following is the syntax:
|     (gdb) showtaskacts <task>
*/


void showtaskstacks(uint32_t arg0) {
    showtaskheader();
    uint32_t kgm_taskp = arg0; // (struct task *)
    showtaskint(kgm_taskp);
    uint32_t kgm_head_actp = get_addr(task, kgm_taskp, thr_acts);
    uint32_t kgm_actp = (uint32_t)get_fieldvm(task, kgm_taskp, thr_acts.next); // (struct thread_activation *)
    while (kgm_actp != kgm_head_actp) {
        showactheader();
        thread_activation kgm_act;
        get_thread_activation(kgm_actp, kgm_act);
        showactint(kgm_act, 1);
        kgm_actp = kgm_act.thr_acts.next; // (struct thread_activation *)
    }
}
/* showtaskstacks
| Routine to print a summary listing of the activations in a task and their stacks
| The following is the syntax:
|     (gdb) showtaskstacks <task>
*/


void showalltasks() {
    showtaskheader();
    if (!_default_pset)
        lookup_name_kernel("_default_pset", _default_pset);
    if (!_default_pset)
        return;

    uint32_t kgm_head_taskp = get_addr(processor_set, _default_pset, tasks);
    uint32_t kgm_taskp = (uint32_t)get_fieldvm(queue_head_t, kgm_head_taskp, next); // (struct task *)
    while (kgm_taskp != kgm_head_taskp) {
        showtaskint(kgm_taskp);
        kgm_taskp = (uint32_t)get_fieldvm(task, kgm_taskp, pset_tasks.next); // (struct task *)
    }
}
/* showalltasks
| Routine to print a summary listing of all the tasks
| The following is the syntax:
|     (gdb) showalltasks
*/


static void showprocheader() {
    printf(" pid  proc        command\n");
}

static void showprocint(uint32_t arg0) {
    uint32_t kgm_procp = arg0; // (struct proc *)
    if (kgm_procp != 0) {
        proc kgm_proc;
        get_proc(kgm_procp, kgm_proc);
        printf("%5d  ", kgm_proc.p_pid);
        printf("0x%08x  ", kgm_procp);
        printf("%s\n", kgm_proc.p_comm);
    }
    else
    {
        printf("  *0*  0x00000000  --\n");
    }
}

void showpid(uint32_t arg0) {
    showtaskheader();
    if (!_default_pset)
        lookup_name_kernel("_default_pset", _default_pset);
    if (!_default_pset)
        return;

    uint32_t kgm_head_taskp = get_addr(processor_set, _default_pset, tasks);
    uint32_t kgm_taskp = (uint32_t)get_fieldvm(queue_head_t, kgm_head_taskp, next); // (struct task *)
    while (kgm_taskp != kgm_head_taskp) {
        uint32_t kgm_procp = (uint32_t)get_fieldvm(task, kgm_taskp, bsd_info); // (struct proc *)
        if ((kgm_procp != 0) && (get_fieldvm(proc, kgm_procp, p_pid) == arg0)) {
            showtaskint(kgm_taskp);
            kgm_taskp = kgm_head_taskp;
        }
        else
        {
            kgm_taskp = (uint32_t)get_fieldvm(task, kgm_taskp, pset_tasks.next); // (struct task *)
        }
    }
}
/* showpid
| Routine to print a single process by pid
| The following is the syntax:
|     (gdb) showpid <pid>
*/

void showproc(uint32_t arg0) {
    showtaskheader();
    uint32_t kgm_procp = arg0; // (struct proc *)
    showtaskint((uint32_t)get_fieldvm(proc, kgm_procp, task));
}


/*
static void kdb() {
    switch_debugger=1;
    continue;
}
*/
/* kdb
| kdb - Switch to the inline kernel debugger
|
| usage: kdb
|
| The kdb macro allows you to invoke the inline kernel debugger.
*/

static void showpsetheader() {
    printf("portset     waitqueue   recvname    ");
    printf("flags refs  recvname    process\n");
}

static void showportheader() {
    printf("port        mqueue      recvname    ");
    printf("flags refs  recvname    process\n");
}

static void showportmemberheader() {
    printf("members     port        recvname    ");
    printf("flags refs  mqueue      msgcount\n");
}

static void showkmsgheader() {
    printf("messages    kmsg        size        ");
    printf("disp msgid  remote-port local-port\n");
}

static void showkmsgint(uint32_t arg0) {
    printf("            0x%08x  ", arg0);
    mach_msg_header_t_32 kgm_kmsgh;
    get_mach_msg_header(get_addr(ipc_kmsg, arg0, ikm_header), kgm_kmsgh);
    printf("0x%08x  ", kgm_kmsgh.msgh_size);
    if (((kgm_kmsgh.msgh_bits & 0xff) == 19)) {
        printf("rC");
    }
    else
    {
        printf("rM");
    }
    if (((kgm_kmsgh.msgh_bits & 0xff00) == (19 < 8))) {
        printf("lC");
    }
    else
    {
        printf("lM");
    }
    if ((kgm_kmsgh.msgh_bits & 0xf0000000)) {
        printf("c");
    }
    else
    {
        printf("s");
    }
    printf("%5d  ", kgm_kmsgh.msgh_id);
    printf("0x%08x  ", kgm_kmsgh.msgh_remote_port);
    printf("0x%08x\n", kgm_kmsgh.msgh_local_port);
}



static void showkobject(uint32_t arg0) {
    uint32_t kgm_portp = arg0; // (struct ipc_port *)
    printf("0x%08x  kobject(", (uint32_t)get_fieldvm(ipc_port, kgm_portp, ip_kobject));
    uint32_t kgm_kotype = ((uint32_t)get_fieldvm(ipc_port, kgm_portp, ip_object.io_bits) & 0x00000fff);
    if (kgm_kotype == 1) {
        printf("THREAD");
    }
    if (kgm_kotype == 2) {
        printf("TASK");
    }
    if (kgm_kotype == 3) {
        printf("HOST");
    }
    if (kgm_kotype == 4) {
        printf("HOST_PRIV");
    }
    if (kgm_kotype == 5) {
        printf("PROCESSOR");
    }
    if (kgm_kotype == 6) {
        printf("PSET");
    }
    if (kgm_kotype == 7) {
        printf("PSET_NAME");
    }
    if (kgm_kotype == 8) {
        printf("TIMER");
    }
    if (kgm_kotype == 9) {
        printf("PAGER_REQ");
    }
    if (kgm_kotype == 10) {
        printf("DEVICE");
    }
    if (kgm_kotype == 11) {
        printf("XMM_OBJECT");
    }
    if (kgm_kotype == 12) {
        printf("XMM_PAGER");
    }
    if (kgm_kotype == 13) {
        printf("XMM_KERNEL");
    }
    if (kgm_kotype == 14) {
        printf("XMM_REPLY");
    }
    if (kgm_kotype == 15) {
        printf("NOTDEF 15");
    }
    if (kgm_kotype == 16) {
        printf("NOTDEF 16");
    }
    if (kgm_kotype == 17) {
        printf("HOST_SEC");
    }
    if (kgm_kotype == 18) {
        printf("LEDGER");
    }
    if (kgm_kotype == 19) {
        printf("MASTER_DEV");
    }
    if (kgm_kotype == 20) {
        printf("ACTIVATION");
    }
    if (kgm_kotype == 21) {
        printf("SUBSYSTEM");
    }
    if (kgm_kotype == 22) {
        printf("IO_DONE_QUE");
    }
    if (kgm_kotype == 23) {
        printf("SEMAPHORE");
    }
    if (kgm_kotype == 24) {
        printf("LOCK_SET");
    }
    if (kgm_kotype == 25) {
        printf("CLOCK");
    }
    if (kgm_kotype == 26) {
        printf("CLOCK_CTRL");
    }
    if (kgm_kotype == 27) {
        printf("IOKIT_SPARE");
    }
    if (kgm_kotype == 28) {
        printf("NAMED_MEM");
    }
    if (kgm_kotype == 29) {
        printf("IOKIT_CON");
    }
    if (kgm_kotype == 30) {
        printf("IOKIT_OBJ");
    }
    if (kgm_kotype == 31) {
        printf("UPL");
    }
    printf(")\n");
}

static void showportdestproc(uint32_t arg0) {
    if (!_default_pset)
        lookup_name_kernel("_default_pset", _default_pset);
    if (!_default_pset)
        return;

    uint32_t kgm_portp = arg0; // (struct ipc_port *)
    uint32_t kgm_spacep = (uint32_t)get_fieldvm(ipc_port, kgm_portp, data.receiver);
    uint32_t kgm_destprocp = 0;
    uint32_t kgm_taskp = 0;
//  check against the previous cached value - this is slow
    if (kgm_spacep != kgm_destspacep) {
        kgm_destprocp = 0; // (struct proc *)

        uint32_t kgm_head_taskp = get_addr(processor_set, _default_pset, tasks);
        kgm_taskp = (uint32_t)get_fieldvm(queue_head_t, kgm_head_taskp, next); // (struct task *)
        while ((kgm_destprocp == 0) && (kgm_taskp != kgm_head_taskp)) {
            kgm_destspacep = (uint32_t)get_fieldvm(task, kgm_taskp, itk_space);
            if (kgm_destspacep == kgm_spacep) {
                kgm_destprocp = (uint32_t)get_fieldvm(task, kgm_taskp, bsd_info); // (struct proc *)
                break; // joevt - stop infinite loop
            }
            else
            {
                kgm_taskp = (uint32_t)get_fieldvm(task, kgm_taskp, pset_tasks.next); // (struct task *)
            }
        }
    }
    if (kgm_destprocp != 0) {
        proc kgm_destproc;
        get_proc(kgm_destprocp, kgm_destproc);
        printf("%s(%d)\n", kgm_destproc.p_comm, kgm_destproc.p_pid);
    }
    else
    {
        printf("task 0x%08x\n", kgm_taskp);
    }
}

static void showportdest(uint32_t arg0) {
    if (!_ipc_space_kernel)
        lookup_name_kernel("_ipc_space_kernel", _ipc_space_kernel);
    if (!_ipc_space_kernel)
        return;

    uint32_t kgm_portp = arg0; // (struct ipc_port *)
    uint32_t kgm_spacep = (uint32_t)get_fieldvm(ipc_port, kgm_portp, data.receiver);
    if (kgm_spacep == get_32vm(_ipc_space_kernel)) {
        showkobject(kgm_portp);
    }
    else
    {
        if (get_fieldvm(ipc_port, kgm_portp, ip_object.io_bits) & 0x80000000) {
            printf("0x%08x  ", (uint32_t)get_fieldvm(ipc_port, kgm_portp, ip_object.io_receiver_name));
            showportdestproc(kgm_portp);
        }
        else
        {
            printf("0x%08x  inactive-port\n", kgm_portp);
        }
    }
}

static void showportmember(uint32_t arg0) {
    printf("            0x%08x  ", arg0);
    uint32_t kgm_portp = arg0; // (struct ipc_port *)
    printf("0x%08x  ", (uint32_t)get_fieldvm(ipc_port, kgm_portp, ip_object.io_receiver_name));
    if (get_fieldvm(ipc_port, kgm_portp, ip_object.io_bits) & 0x80000000) {
        printf("A");
    }
    else
    {
        printf(" ");
    }
    if (get_fieldvm(ipc_port, kgm_portp, ip_object.io_bits) & 0x7fff0000) {
        printf("Set ");
    }
    else
    {
        printf("Port");
    }
    printf("%5d  ", (uint32_t)get_fieldvm(ipc_port, kgm_portp, ip_object.io_references));
    printf("0x%08x  ", (uint32_t)get_addr(ipc_port, kgm_portp, ip_messages));
    printf("0x%08x\n", (uint32_t)get_fieldvm(ipc_port, kgm_portp, ip_messages.data.port.msgcount));
}

static void showportint(uint32_t arg0, uint32_t arg1) {
    printf("0x%08x  ", arg0);
    uint32_t kgm_portp = arg0; // (struct ipc_port *)
    printf("0x%08x  ", (uint32_t)get_addr(ipc_port, kgm_portp, ip_messages));
    printf("0x%08x  ", (uint32_t)get_fieldvm(ipc_port, kgm_portp, ip_object.io_receiver_name));
    if (((uint32_t)get_fieldvm(ipc_port, kgm_portp, ip_object.io_bits) & 0x80000000)) {
        printf("A");
    }
    else
    {
        printf("D");
    }
    printf("Port");
    printf("%5d  ", (uint32_t)get_fieldvm(ipc_port, kgm_portp, ip_object.io_references));
    kgm_destspacep = 0; // (struct ipc_space *)
    showportdest(kgm_portp);
    uint32_t kgm_kmsgp = (uint32_t)get_fieldvm(ipc_port, kgm_portp, ip_messages.data.port.messages.ikmq_base); // (ipc_kmsg_t)
    if (arg1 && kgm_kmsgp) {
        showkmsgheader();
        showkmsgint(kgm_kmsgp);
        uint32_t kgm_kmsgheadp = kgm_kmsgp;
        kgm_kmsgp = (uint32_t)get_fieldvm(ipc_kmsg, kgm_kmsgp, ikm_next);
        while (kgm_kmsgp != kgm_kmsgheadp) {
            showkmsgint(kgm_kmsgp);
            kgm_kmsgp = (uint32_t)get_fieldvm(ipc_kmsg, kgm_kmsgp, ikm_next);
        }
    }
}

int kgm_portoff;

static void showpsetint(uint32_t arg0) {
    printf("0x%08x  ", arg0);
    uint32_t kgm_psetp = arg0; // (struct ipc_pset *)
    printf("0x%08x  ", (uint32_t)get_addr(ipc_pset, kgm_psetp, ips_messages));
    printf("0x%08x  ", (uint32_t)get_fieldvm(ipc_pset, kgm_psetp, ips_object.io_receiver_name));
    if ((uint32_t)get_fieldvm(ipc_pset, kgm_psetp, ips_object.io_bits) & 0x80000000) {
        printf("A");
    }
    else
    {
        printf("D");
    }
    printf("Set ");
    printf("%5d  ", (uint32_t)get_fieldvm(ipc_pset, kgm_psetp, ips_object.io_references));
    printf("0x%08x  ", (uint32_t)get_fieldvm(ipc_pset, kgm_psetp, ips_object.io_receiver_name));
    uint32_t kgm_setlinksp = (uint32_t)get_addr(ipc_pset, kgm_psetp, ips_messages.data.set_queue.wqs_setlinks);
    uint32_t kgm_wql = (uint32_t)get_addr(queue_head_t, kgm_setlinksp, next); // (struct wait_queue_link *)
    uint32_t kgm_found = 0;
    while ((queue_entry_t)kgm_wql != (queue_entry_t)kgm_setlinksp) {
        uint32_t kgm_portp =
            ((int)(get_fieldvm(wait_queue_link, kgm_wql, wql_element.wqe_queue)) - ((int)kgm_portoff)); // (struct ipc_port *)
        if (!kgm_found) {
            kgm_destspacep = 0; // (struct ipc_space *)
            showportdestproc(kgm_portp);
            showportmemberheader();
            kgm_found = 1;
        }
        showportmember(kgm_portp);
        kgm_wql = (uint32_t)get_fieldvm(wait_queue_link, kgm_wql, wql_setlinks.next); // (struct wait_queue_link *)
    }
    if (!kgm_found) {
        printf("--n/e--\n");
    }
}

static void showpset(uint32_t arg0) {
    showpsetheader();
    showpsetint(arg0);
}

static void showport(uint32_t arg0) {
    showportheader();
    showportint(arg0, 1);
}

static void showipcobject(uint32_t arg0) {
    uint32_t kgm_objectp = arg0; // (ipc_object_t)
    if (get_fieldvm(ipc_object, kgm_objectp, io_bits) & 0x7fff0000) {
        showpset(kgm_objectp);
    }
    else
    {
        showport(kgm_objectp);
    }
}

static void showmqueue(uint32_t arg0) {
    ipc_mqueue kgm_mqueue;
    get_ipc_mqueue(arg0, kgm_mqueue);
    uint32_t kgm_psetoff = offsetof(ipc_pset, ips_messages); // &(((struct ipc_pset *)0)->ips_messages);
    kgm_portoff = offsetof(ipc_port, ip_messages); // &(((struct ipc_port *)0)->ip_messages)

    if (kgm_mqueue.data.set_queue.wqs_wait_queue.wq_type == 0xf1d1) {
        uint32_t kgm_pset = arg0 - kgm_psetoff;
        showpsetheader();
        showpsetint(kgm_pset);
    }
    if (kgm_mqueue.data.set_queue.wqs_wait_queue.wq_type == 0xf1d0) {
        showportheader();
        uint32_t kgm_port = arg0 - kgm_portoff;
        showportint(kgm_port, 1);
    }
}

static void zprint_one(uint32_t arg0) {
    zone kgm_zone;
    get_zone(arg0, kgm_zone);

    char zone_name[256];
    for (int i = 0; i < sizeof(zone_name) - 1; i++) {
        char c = kernel_read(kgm_zone.zone_name + i, 1);
        zone_name[i] = c;
        if (!c)
            break;
    }

    printf("0x%08x ", arg0);
    printf("%8d ", kgm_zone.count);
    printf("%8x ", kgm_zone.cur_size);
    printf("%8x ", kgm_zone.max_size);
    printf("%6d ", kgm_zone.elem_size);
    printf("%8x ", kgm_zone.alloc_size);
    printf("%s ", zone_name);

    if (kgm_zone.exhaustible) {
        printf("H");
    }
    if (kgm_zone.collectable) {
        printf("C");
    }
    if (kgm_zone.expandable) {
        printf("X");
    }
    printf("\n");
}


void zprint() {
    if (!_first_zone)
        lookup_name_kernel("_first_zone", _first_zone);
    if (!_first_zone)
        return;

    printf("ZONE          COUNT   TOT_SZ   MAX_SZ ELT_SZ ALLOC_SZ NAME\n");

    uint32_t kgm_zone_ptr = get_32vm(_first_zone); // (struct zone *)
    while (kgm_zone_ptr != 0) {
        zprint_one(kgm_zone_ptr);
        kgm_zone_ptr = get_fieldvm(zone, kgm_zone_ptr, next_zone);
    }
    printf("\n");
}
/* zprint
| Routine to print a summary listing of all the kernel zones
| The following is the syntax:
|     (gdb) zprint
*/

static uint32_t kdp_act_counter = 0;
static uint32_t kdpstate = 0;

static void flush() {
}

static void update() {
}

void switchtoact(uint32_t arg0) {
    if (!_machine_slot)
        lookup_name_kernel("_machine_slot", _machine_slot);
    if (!_machine_slot)
        return;
    if (!_kdp)
        lookup_name_kernel("_kdp", _kdp);
    if (!_kdp)
        return;

    if (get_fieldvm(machine_slot, _machine_slot, cpu_type) == 18) {
        if (kdp_act_counter == 0) {
            kdpstate = get_fieldvm(kdp_glob_t, _kdp, saved_state); // (struct savearea *)
        }
        kdp_act_counter = kdp_act_counter + 1;
        uint32_t newact = arg0; // (struct thread_activation *)
        if (get_fieldvm(thread_shuttle, get_fieldvm(thread_activation, newact, thread), kernel_stack) == 0) {
            printf("This activation does not have a stack.\n");
            printf("continuation:");
            printf("0x%x", get_fieldvm(thread_shuttle, get_fieldvm(thread_activation, newact, thread), continuation));
            printf("\n");
        }
        set_32(get_addr(kdp_glob_t, _kdp, saved_state), get_fieldvm(thread_activation, newact, mact.pcb)); // (struct savearea *)
        flush();
        ppc_state.pc = get_fieldvm(savearea, get_fieldvm(thread_activation, newact, mact.pcb), save_srr0);
        update();
    }
    else
    {
        printf("switchtoact not implemented for this architecture.\n");
    }
}
/* switchtoact
Syntax: switchtoact <address of activation>
| This command allows gdb to examine the execution context and call
| stack for the specified activation. For example, to view the backtrace
| for an activation issue "switchtoact <address>", followed by "bt".
| Before resuming execution, issue a "resetctx" command, to
| return to the original execution context.
*/

void switchtoctx(uint32_t arg0) {
    if (!_machine_slot)
        lookup_name_kernel("_machine_slot", _machine_slot);
    if (!_machine_slot)
        return;
    if (!_kdp)
        lookup_name_kernel("_kdp", _kdp);
    if (!_kdp)
        return;

    if (get_fieldvm(machine_slot, _machine_slot, cpu_type) == 18) {
        if (kdp_act_counter == 0) {
            kdpstate = get_fieldvm(kdp_glob_t, _kdp, saved_state); // (struct savearea *)
        }
        kdp_act_counter = kdp_act_counter + 1;
        set_32(get_addr(kdp_glob_t, _kdp, saved_state), arg0); // (struct savearea *)
        flush();
        ppc_state.pc = get_fieldvm(savearea, arg0, save_srr0);
        update();
    }
    else
    {
        printf("switchtoctx not implemented for this architecture.\n");
    }
}
/* switchtoctx
Syntax: switchtoctx <address of pcb>
| This command allows gdb to examine an execution context and dump the
| backtrace for this execution context.
| Before resuming execution, issue a "resetctx" command, to
| return to the original execution context.
*/

void resetctx() {
    if (!_machine_slot)
        lookup_name_kernel("_machine_slot", _machine_slot);
    if (!_machine_slot)
        return;
    if (!_kdp)
        lookup_name_kernel("_kdp", _kdp);
    if (!_kdp)
        return;

    if (get_fieldvm(machine_slot, _machine_slot, cpu_type) == 18) {
        set_32(get_addr(kdp_glob_t, _kdp, saved_state), kdpstate); // (struct savearea *)
        flush();
        ppc_state.pc = get_fieldvm(savearea, get_addr(kdp_glob_t, _kdp, saved_state), save_srr0);
        update();
        kdp_act_counter = 0;
    }
    else
    {
        printf("resetctx not implemented for this architecture.\n");
    }
}
/* resetctx
| Syntax: resetctx
| Returns to the original execution context. This command should be
| issued if (you wish to resume execution after using the "switchtoact") {
| or "switchtoctx" commands.
*/

void paniclog() {
    if (!_debug_buf)
        lookup_name_kernel("_debug_buf", _debug_buf);
    if (!_debug_buf)
        return;
    if (!_debug_buf_size)
        lookup_name_kernel("_debug_buf_size", _debug_buf_size);
    if (!_debug_buf_size)
        return;

    uint32_t kgm_panic_bufptr = get_32(_debug_buf);
    uint32_t kgm_panic_bufptr_max = get_32(_debug_buf) + get_32(_debug_buf_size);
    while (kgm_panic_bufptr < kgm_panic_bufptr_max) {
        char c = kernel_read(kgm_panic_bufptr, 1);
        if (!c)
            break;
        if (c == 10) {
            printf("\n");
        }
        else
        {
            printf("%c", c);
        }
        kgm_panic_bufptr++;
    }
}
/* paniclog
| Syntax: paniclog
| Display the panic log information
|
*/
