#include "enclave.h"

#include <arch/memory.h>
#include <arch/page_tables.h>
#include "cpu_core_inl.h"
#include "dram_regions_inl.h"
#include "enclave_inl.h"
#include "metadata_inl.h"

size_t* g_os_region_bitmap = 0;

api_result_t delete_enclave(enclave_id_t enclave_id) {
  size_t dram_region = clamped_dram_region_for(enclave_id);
  if (test_and_set_dram_region_lock(dram_region))
    return monitor_concurrent_call;

  // NOTE: null_enclave_id is accepted by is_valid_enclave_id, but does not
  //       have a useful meaning here
  if (enclave_id == null_enclave_id || !is_valid_enclave_id(enclave_id)) {
    clear_dram_region_lock(dram_region);
    return monitor_invalid_value;
  }

  enclave_info_t* enclave_info = enclave_id;
  if (enclave_info->thread_count != 0) {
    clear_dram_region_lock(dram_region);
    return monitor_invalid_state;
  }

  size_t* region_bitmap = enclave_region_bitmap(enclave_id);
  size_t region_iterator = 0;
  for (; region_iterator < g_dram_region_count; ++region_iterator) {
    if (region_iterator == dram_region)
      continue;  // We've already locked the enclave's main DRAM region.
    if (!read_bitmap_bit(region_bitmap, region_iterator))
      continue;  // This region does not belong to the enclave.
    if (test_and_set_dram_region_lock(region_iterator))
      break;  // Failed to acquire lock on region.
  }
  if (region_iterator < g_dram_region_count) {
    // We failed to acquire a DRAM region lock. Unlock everything we touched.
    for (size_t i = 0; i < region_iterator; ++i) {
      if (i == dram_region)
        continue;  // We've already locked the enclave's main DRAM region.
      if (!read_bitmap_bit(region_bitmap, i))
        continue;  // This region does not belong to the enclave.
      clear_dram_region_lock(i);
    }
    clear_dram_region_lock(dram_region);
    return monitor_concurrent_call;
  }

  // NOTE: we know that no enclave thread is running, so we can free the
  //       enclave's DRAM regions directly, without going through the blocking
  //       state
  for (size_t i = 0; i < g_dram_region_count; ++i) {
    if (!read_bitmap_bit(region_bitmap, region_iterator))
      continue;  // This region does not belong to the enclave.

    dram_region_info_t* region = &g_dram_region[i];
    region->owner = free_enclave_id;

    // NOTE: The enclave's DRAM regions have pages and pinned pages, due to
    //       threads. The rest of the system assumes that pinned_pages is zero
    //       for a DRAM-region once the region is blocked, which is a
    //       prerequisite to the DRAM region being freed. So, we must make sure
    //       that pinned_pages is zero for the DRAM regions that we free.
    region->pinned_pages = 0;

    bzero_dram_region(i);
  }

  for (size_t i = 0; i < g_dram_region_count; ++i) {
    if (i == dram_region)
      continue;  // We've already locked the enclave's main DRAM region.
    if (!read_bitmap_bit(region_bitmap, i))
      continue;  // This region does not belong to the enclave.
    clear_dram_region_lock(i);
  }
  clear_dram_region_lock(dram_region);
  return monitor_ok;
}

api_result_t enter_enclave(enclave_id_t enclave_id,
    thread_id_t thread_id) {
  size_t dram_region = clamped_dram_region_for(enclave_id);
  if (test_and_set_dram_region_lock(dram_region))
    return monitor_concurrent_call;

  // NOTE: null_enclave_id is accepted by is_valid_enclave_id, but does not
  //       have a useful meaning here
  if (enclave_id == null_enclave_id || !is_valid_enclave_id(enclave_id)) {
    clear_dram_region_lock(dram_region);
    return monitor_invalid_value;
  }

  enclave_info_t* enclave_info = enclave_id;
  // TODO(pwnall): new thread_id validity check

  if (enclave_info->is_initialized == 0) {
    clear_dram_region_lock(dram_region);
    return monitor_invalid_state;
  }

  /*
  thread_slot_t* slot{enclave_thread_slot(enclave_id, thread_id)};
  if (atomic_flag_test_and_set(&(slot->*(&thread_slot_t::lock)))) {
    clear_dram_region_lock(dram_region);
    return monitor_concurrent_call;
  }

  thread_info_t* private_thread =
      slot->*(&thread_slot_t::thread_public_info);
  if (private_thread == thread_info_t*::null()) {
    atomic_flag_clear(&(slot->*(&thread_slot_t::lock)));
    clear_dram_region_lock(dram_region);
    return monitor_invalid_state;
  }

  atomic_fetch_add(&(enclave_info->*(&enclave_info_t::running_threads)),
      static_cast<size_t>(1));
  clear_dram_region_lock(dram_region);

  core_info_t* core{current_core_info()};
  core->*(&core_info_t::enclave_id) = enclave_id;
  core->*(&core_info_t::thread_id) = thread_id;
  core->*(&core_info_t::thread) = private_thread;
  thread_public_info_t* thread{
      &(private_thread->*(&thread_private_info_t::public_info))};
  set_ev_base(enclave_info->*(&enclave_info_t::ev_base));
  set_ev_mask(enclave_info->*(&enclave_info_t::ev_mask));
  set_epar_base(uintptr_t(enclave_info));
  set_epar_mask(enclave_info->*(&enclave_info_t::epar_mask));
  set_edrb_map(uintptr_t(enclave_region_bitmap(enclave_id)));
  set_eptbr(thread->*(&thread_init_info_t::eptbr));
  */

  // TODO: set the hypervisor and OS handler addresses to monitor functions
  //       that fault if the enclave attempts to perform syscalls or hypercalls

  // TODO: modify return state to perform an enclave jump

  return monitor_ok;
}

api_result_t copy_debug_enclave_page(enclave_id_t enclave_id,
    uintptr_t enclave_addr, uintptr_t os_addr, bool read_from_enclave) {
  if (!is_page_aligned(enclave_addr) || !is_page_aligned(os_addr))
    return monitor_invalid_value;
  if (!is_dram_address(enclave_addr) || !is_dram_address(os_addr))
    return monitor_invalid_value;

  size_t enclave_dram_region = clamped_dram_region_for(enclave_id);
  size_t enclave_addr_dram_region = dram_region_for(enclave_addr);
  size_t os_addr_dram_region = dram_region_for(os_addr);

  if (test_and_set_dram_region_lock(enclave_dram_region))
    return monitor_concurrent_call;

  // NOTE: We don't need to check if os_dram_region is the same as
  //       enclave_dram_region. If that's the case, we'll simply fail to
  //       acquire the lock and return concurrent_call. This is acceptable.
  //       Ideally, we'd return invalid_value, but that'd increase code size.
  if (test_and_set_dram_region_lock(os_addr_dram_region)) {
    clear_dram_region_lock(enclave_dram_region);
    return monitor_concurrent_call;
  }

  api_result_t result = monitor_ok;
  if (read_dram_region_owner(enclave_dram_region) != enclave_id)
    result = monitor_invalid_value;
  if (read_dram_region_owner(os_addr_dram_region) != null_enclave_id)
    result = monitor_invalid_value;

  // NOTE: We don't need to lock enclave_addr's DRAM region because we have the
  //       enclave's DRAM region lock, so the DRAM region cannot be added to or
  //       removed (blocked) from the enclave while this call is happening.
  if (!read_enclave_region_bitmap_bit(enclave_id, enclave_addr_dram_region))
    result = monitor_invalid_value;

  enclave_info_t* enclave_info = enclave_id;
  if (enclave_info->is_debug == 0)
    result = monitor_invalid_state;

  if (result == monitor_ok) {
    size_t* enclave_ptr = enclave_addr;
    size_t* enclave_end = (enclave_addr + page_size());
    size_t* os_ptr = os_addr;
    if (read_from_enclave) {
      for (; enclave_ptr != enclave_end; enclave_ptr += 1, os_ptr += 1) {
        *os_ptr = *enclave_ptr;
      }
    } else {
      for (; enclave_ptr != enclave_end; enclave_ptr += 1, os_ptr += 1) {
        *enclave_ptr = *os_ptr;
      }
    }
  }

  clear_dram_region_lock(os_addr_dram_region);
  clear_dram_region_lock(enclave_dram_region);
  return result;
}

api_result_t create_thread(thread_id_t thread_id,
    uintptr_t phys_addr) {
  if (!is_page_aligned(phys_addr))
    return monitor_invalid_value;
  if (!is_dram_address(phys_addr))
    return monitor_invalid_value;

  uintptr_t phys_end = phys_addr + thread_metadata_size();
  // NOTE: The thread_info_t occupies contiguous space in physical
  //       memory, so we only need to check the end for DRAM inclusion. The
  //       intermediate pages are guaranteed to be in DRAM.
  if (!is_dram_address(phys_end - 1))
    return monitor_invalid_value;

  enclave_id_t enclave_id = current_enclave();
  size_t dram_region = dram_region_for(enclave_id);
  if (test_and_set_dram_region_lock(dram_region))
    return monitor_concurrent_call;

  size_t thread_dram_region = dram_region_for(phys_addr);
  if (!read_bitmap_bit(enclave_region_bitmap(enclave_id),
      thread_dram_region)) {
    clear_dram_region_lock(dram_region);
    return monitor_invalid_state;
  }
  for (uintptr_t page_addr = phys_addr; page_addr < phys_end;
       page_addr += page_size()) {
    if (dram_region_for(page_addr) != thread_dram_region) {
      // See load_thread() for the reasons why we don't support thread
      // metadata spanning multiple DRAM regions.
      clear_dram_region_lock(dram_region);
      return monitor_unsupported;
    }
  }

  /*
  // NOTE: We're performing the thread slot checks towards the end to minimize
  //       the number of times we have two release two/three locks when bailing
  //       out due to errors.

  thread_slot_t* slot{enclave_thread_slot(enclave_id, thread_id)};
  if (atomic_flag_test_and_set(&(slot->*(&thread_slot_t::lock)))) {
    clear_dram_region_lock(dram_region);
    return monitor_concurrent_call;
  }

  thread_info_t* old_thread =
      slot->*(&thread_slot_t::thread_public_info);
  if (old_thread != thread_info_t*::null()) {
    atomic_flag_clear(&(slot->*(&thread_slot_t::lock)));
    clear_dram_region_lock(dram_region);
    return monitor_invalid_state;
  }

  // NOTE: We're locking the thread info's DRAM region last, to minimize the
  //       number of times we have to release three locks when bailing out due
  //       to errors.

  // NOTE: The thread metadata's DRAM region may be the same as the main
  //       enclave's DRAM region, so we have to code around that.
  if (thread_dram_region != dram_region &&
      test_and_set_dram_region_lock(thread_dram_region))  {
    atomic_flag_clear(&(slot->*(&thread_slot_t::lock)));
    clear_dram_region_lock(dram_region);
    return monitor_concurrent_call;
  }

  dram_region_info_t* thread_region{
      dram_region_start(thread_dram_region)};
  thread_region->*(&dram_region_info_t::pinned_pages) +=
      thread_metadata_pages();

  thread_info_t* private_thread{phys_addr};
  slot->*(&thread_slot_t::thread_public_info) = private_thread;

  if (thread_dram_region != dram_region)
    clear_dram_region_lock(thread_dram_region);
  atomic_flag_clear(&(slot->*(&thread_slot_t::lock)));
  clear_dram_region_lock(dram_region);
  */
  return monitor_ok;
}

api_result_t delete_thread(thread_id_t thread_id) {
  enclave_id_t enclave_id = current_enclave();
  size_t dram_region = dram_region_for(enclave_id);
  if (test_and_set_dram_region_lock(dram_region))
    return monitor_concurrent_call;

  /*
  thread_slot_t* slot{enclave_thread_slot(enclave_id, thread_id)};
  if (atomic_flag_test_and_set(&(slot->*(&thread_slot_t::lock)))) {
    clear_dram_region_lock(dram_region);
    return monitor_concurrent_call;
  }

  thread_info_t* thread =
      slot->*(&thread_slot_t::thread_public_info);
  if (thread == thread_info_t*::null()) {
    atomic_flag_clear(&(slot->*(&thread_slot_t::lock)));
    clear_dram_region_lock(dram_region);
    return monitor_invalid_state;
  }

  uintptr_t thread_addr = uintptr_t(thread);
  size_t thread_dram_region = dram_region_for(thread_addr);

  // NOTE: The thread metadata's DRAM region may be the same as the main
  //       enclave's DRAM region, so we have to code around that.
  if (thread_dram_region != dram_region &&
      test_and_set_dram_region_lock(thread_dram_region))  {
    atomic_flag_clear(&(slot->*(&thread_slot_t::lock)));
    clear_dram_region_lock(dram_region);
    return monitor_concurrent_call;
  }

  dram_region_info_t* thread_region{
      dram_region_start(thread_dram_region)};
  thread_region->*(&dram_region_info_t::pinned_pages) -=
      thread_metadata_pages();

  slot->*(&thread_slot_t::thread_public_info) =
      thread_info_t*::null();

  if (thread_dram_region != dram_region)
    clear_dram_region_lock(thread_dram_region);
  atomic_flag_clear(&(slot->*(&thread_slot_t::lock)));
  clear_dram_region_lock(dram_region);
  */
  return monitor_ok;
}

api_result_t exit_enclave() {
  core_info_t* core = current_core_info();

  enclave_id_t enclave_id = core->enclave_id;
  thread_id_t thread_id = core->thread_id;
  thread_info_t* private_thread = core->thread;

  enclave_info_t* enclave_info = enclave_id;
  /*
  thread_slot_t* slot{enclave_thread_slot(enclave_id, thread_id)};

  core->*(&core_info_t::enclave_id) = null_enclave_id;
  set_eptbr(0);

  // NOTE: The values below make sure that the enclave registers will never
  //       be selected by the page walker input's MUXes. The address AND mask
  //       is 0, so the AND result will always be 0, and it will be compared
  //       with a non-zero number.
  set_ev_base(page_size());
  set_ev_mask(0);
  // NOTE: we don't need to reset eptbr, edrb_map and epar_{base, mask},
  //       because they'll never make it out of the page walker input MUXes.

  atomic_fetch_sub(&(enclave_info->*(&enclave_info_t::running_threads)),
      static_cast<size_t>(1));
  atomic_flag_clear(&(slot->*(&thread_slot_t::lock)));

  // TODO: restore the hypervisor and OS handler addresses changed in
  //       enter_enclave

  // TODO: modify return state to return to the run_enclave_thread() caller
  */
  return monitor_ok;
}
