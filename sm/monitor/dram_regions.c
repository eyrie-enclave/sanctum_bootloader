#include "dram_regions.h"

#include "cpu_core_inl.h"
#include "dram_regions_inl.h"
#include "enclave_inl.h"
#include "metadata_inl.h"

dram_region_info_t* g_dram_region = 0;
dram_regions_info_t* g_dram_regions = 0;

size_t g_dram_base;
size_t g_dram_size;
size_t g_dram_region_shift;
size_t g_dram_stripe_shift;
size_t g_dram_stripe_page_mask;
size_t g_dram_region_mask;
size_t g_dram_stripe_mask;
size_t g_dram_region_count;
size_t g_dram_stripe_size;
size_t g_dram_stripe_pages;
size_t g_dram_region_bitmap_words;
size_t g_dma_range_start;
size_t g_dma_range_end;

size_t dram_size() {
  return g_dram_size;
}
size_t dram_region_mask() {
  return g_dram_region_mask;
}
api_result_t block_dram_region(size_t dram_region) {
  if (!is_dynamic_dram_region(dram_region))
    return monitor_invalid_value;
  if (test_and_set_dram_region_lock(dram_region))
    return monitor_concurrent_call;

  enclave_id_t owner = read_dram_region_owner(dram_region);
  if (owner != current_enclave()) {
    clear_dram_region_lock(dram_region);
    return monitor_access_denied;
  }

  dram_region_info_t* region = &g_dram_region[dram_region];
  if (owner != null_enclave_id &&
      region->pinned_pages != 0) {
    clear_dram_region_lock(dram_region);
    return monitor_invalid_state;
  }

  // NOTE: The owner DRAM region is guaranteed to be different from the current
  //       DRAM region. For OS-owned regions, region 0 can never be blocked due
  //       to the is_dynamic_dram_region check. For enclave-owned regions, the
  //       enclave's main region will always have pinned_pages != 0. Therefore,
  //       we can grab the owner region lock without worrying that it's
  //       identical to a lock that we've already grabbed
  size_t owner_dram_region = dram_region_for(owner);
  if (test_and_set_dram_region_lock(owner_dram_region)) {
    clear_dram_region_lock(dram_region);
    return monitor_concurrent_call;
  }

  if (owner_dram_region == 0) {
    bool dma_range_crossed = false;
    uintptr_t region_start = dram_region_start(dram_region);
    uintptr_t region_end = dram_region_start(dram_region + 1);
    if (g_dma_range_start >= region_start && g_dma_range_end <= region_end)
      dma_range_crossed = true;
    if (g_dma_range_end >= region_start && g_dma_range_end <= region_end)
      dma_range_crossed = true;
    if (region_start >= g_dma_range_start && region_start <= g_dma_range_end)
      dma_range_crossed = true;
    if (region_end >= g_dma_range_start && region_end <= g_dma_range_end)
      dma_range_crossed = true;

    if (dma_range_crossed) {
      clear_dram_region_lock(owner_dram_region);
      clear_dram_region_lock(dram_region);
      return monitor_invalid_state;
    }
  }

  region->previous_owner = owner;
  region->owner = blocked_enclave_id;
  size_t block_clock = atomic_fetch_add(
      &(g_dram_regions->block_clock),
      1);
  region->blocked_at = block_clock;
  // TODO: panic if block_clock is max_size_t

  set_enclave_region_bitmap_bit(owner, dram_region, false);
  if (owner == 0)
    set_drb_map(g_os_region_bitmap);
  else
    set_edrb_map(enclave_region_bitmap(owner));

  clear_dram_region_lock(owner_dram_region);
  clear_dram_region_lock(dram_region);
  return monitor_ok;
}

api_result_t dram_region_check_ownership(size_t dram_region) {
  if (!is_dynamic_dram_region(dram_region))
    return monitor_invalid_value;
  if (test_and_set_dram_region_lock(dram_region))
    return monitor_concurrent_call;

  api_result_t result;
  dram_region_info_t* region = &g_dram_region[dram_region];

  // NOTE: we don't need to read the state, because owner has special values
  //       for non-owned states
  if (read_dram_region_owner(dram_region) == current_enclave()) {
    result = monitor_ok;
  } else {
    result = monitor_invalid_state;
  }

  clear_dram_region_lock(dram_region);
  return result;
}

dram_region_state_t dram_region_state(size_t dram_region) {
  if (!is_valid_dram_region(dram_region))
    return dram_region_invalid;

  if (test_and_set_dram_region_lock(dram_region))
    return dram_region_locked;

  dram_region_state_t state;
  switch (read_dram_region_owner(dram_region)) {
  case null_enclave_id:
    state = dram_region_owned;
    break;
  case blocked_enclave_id:
    state = dram_region_blocked;
    break;
  case free_enclave_id:
    state = dram_region_free;
    break;
  default:
    state = dram_region_owned;
  }

  clear_dram_region_lock(dram_region);
  return state;
}

enclave_id_t dram_region_owner(size_t dram_region) {
  if (!is_valid_dram_region(dram_region))
    return null_enclave_id;

  if (test_and_set_dram_region_lock(dram_region))
    return null_enclave_id;

  enclave_id_t owner = read_dram_region_owner(dram_region);
  if (owner == blocked_enclave_id || owner == free_enclave_id)
    owner = null_enclave_id;

  clear_dram_region_lock(dram_region);
  return owner;
}

api_result_t set_dma_range(uintptr_t base, uintptr_t mask) {
  if (!is_valid_range(base, mask))
    return monitor_invalid_value;
  // NOTE: the base is aligned to mask, so (base | mask) == base + mask
  if (!is_dram_address(base | mask))
    return monitor_invalid_value;
  // NOTE: We acquire the lock for region 0 because that's required to block
  //       the DRAM regions owned by the OS.
  if (test_and_set_dram_region_lock(0))
    return monitor_concurrent_call;

  bool os_owns_regions = true;
  uintptr_t range_end = (base | mask) + 1;
  // NOTE: because DRAM regions might not be contiguous, we iterate over all
  //       the stripes in the DMA range and check that their DRAM regions are
  //       owned by the OS
  for (uintptr_t stripe_addr = base; stripe_addr < range_end;
       stripe_addr += g_dram_stripe_size) {
    size_t dram_region = dram_region_for(stripe_addr);
    if (dram_region != 0) {
      if (test_and_set_dram_region_lock(dram_region)) {
        clear_dram_region_lock(0);
        return monitor_concurrent_call;
      }
    }
    dram_region_info_t* region = &g_dram_region[dram_region];
    if (region->owner != 0)
      os_owns_regions = false;

    // NOTE: We're clearing each DRAM region lock after acquiring it, instead
    //       of acquiring all locks and clearing them. We do this so we don't
    //       have to do cascade releases if a lock acquisition fails. This is
    //       acceptable because we hold the lock of DRAM region 0 at all times,
    //       so we're guaranteed that no enclaves join or leave the group of
    //       OS-owned enclaves, and that's all we care about.
    if (dram_region != 0)
      clear_dram_region_lock(dram_region);
  }

  g_dma_range_start = base;
  g_dma_range_end = range_end;
  set_dmar_base(base);
  // NOTE: The hardware register stores the mask in negated form because it
  //       simplifies checks.
  set_dmar_mask(~mask);

  clear_dram_region_lock(0);
  return monitor_ok;
}

api_result_t create_metadata_region(size_t dram_region) {
  if (!is_valid_dram_region(dram_region))
    return monitor_invalid_value;
  if (test_and_set_dram_region_lock(dram_region))
    return monitor_concurrent_call;

  if (read_dram_region_owner(dram_region) != free_enclave_id) {
    clear_dram_region_lock(dram_region);
    return monitor_invalid_state;
  }

  init_metadata_region(dram_region);

  clear_dram_region_lock(dram_region);
  return monitor_ok;
}

api_result_t assign_dram_region(size_t dram_region, enclave_id_t new_owner) {
  // NOTE: non-dynamic DRAM regions will never be freed, so we don't need to
  //       explicitly check for them here
  if (!is_valid_dram_region(dram_region))
    return monitor_invalid_value;
  if (test_and_set_dram_region_lock(dram_region))
    return monitor_concurrent_call;

  if (read_dram_region_owner(dram_region) != free_enclave_id) {
    clear_dram_region_lock(dram_region);
    return monitor_invalid_state;
  }

  size_t new_owner_dram_region = dram_region_for(new_owner);
  // NOTE: We don't need to check if new_owner_dram_region is the same as
  //       dram_region. If that's the case, we'll simply fail to acquire the
  //       lock and return concurrent_call. This is acceptable. Ideally, we'd
  //       return invalid_value, but that'd increase code size.
  if (test_and_set_dram_region_lock(new_owner_dram_region)) {
    clear_dram_region_lock(dram_region);
    return monitor_concurrent_call;
  }

  api_result_t result;
  if (is_valid_enclave_id(new_owner)) {
    dram_region_info_t* region = &g_dram_region[dram_region];
    region->owner = new_owner;
    set_enclave_region_bitmap_bit(new_owner, dram_region, true);
    // NOTE: This is an OS call, so we know for sure that no enclave DRAM
    //       region bitmap is in effect. We only need to apply changes to the
    //       OS DRAM region bitmap.
    if (new_owner == 0)
      set_drb_map(g_os_region_bitmap);
    result = monitor_ok;
  } else {
    result = monitor_invalid_value;
  }

  clear_dram_region_lock(new_owner_dram_region);
  clear_dram_region_lock(dram_region);
  return result;
}

api_result_t free_dram_region(size_t dram_region) {
  // NOTE: non-dynamic DRAM regions will never be blocked, so we don't need to
  //       explicitly check for them here
  if (!is_valid_dram_region(dram_region))
    return monitor_invalid_value;
  if (test_and_set_dram_region_lock(dram_region))
    return monitor_concurrent_call;

  api_result_t result;
  enclave_id_t region_owner = read_dram_region_owner(dram_region);
  dram_region_info_t* region = &g_dram_region[dram_region];
  if (region_owner == blocked_enclave_id) {
    size_t blocked_at = region->blocked_at;

    bool can_free = true;
    // Mappings for OS-owned regions must be TLB-flushed from all cores.
    //
    // Mappings for enclave-owned regions must be TLB-flushed from cores that
    // execute enclave code. However, every enclave exit causes a TLB flush and
    // updates the core's clock.
    for (size_t i = 0; i < g_core_count; ++i) {
      core_info_t* core = &g_core[i];
      if (atomic_load(&(core->flushed_at)) < blocked_at) {
        can_free = false;
        break;
      }
    }
    if (can_free) {
      region->owner = free_enclave_id;
      result = monitor_ok;
    } else {
      result = monitor_invalid_state;
    }
  } else if (region_owner == metadata_enclave_id) {
    // NOTE: Specializing for metadata_enclave_id here takes less code than
    //       trying to handle it in block_dram_region instead.

    // Metadata DRAM regions will never have TLB mappings, so we don't need to
    // worry about TLB flushing. However, we do need to make sure they don't
    // have any in-use entries.
    if (region->pinned_pages == 0) {
      region->owner = free_enclave_id;
      result = monitor_ok;
    } else {
      result = monitor_invalid_state;
    }
  } else {
    result = monitor_invalid_state;
  }

  clear_dram_region_lock(dram_region);
  return result;
}

api_result_t flush_cached_dram_regions() {
  dram_region_tlb_flush();
  return monitor_ok;
}
