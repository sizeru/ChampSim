#include "ghb_gac.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include "cache.h"
#include "champsim.h"

ghb_gac::IndexTable::IndexTable() {
  {
    for (size_t i = 0; i < INDEX_ENTRIES; i++) {
      this->entries[i].trigger = champsim::block_number{0};
    }
  }
}

ghb_gac::GlobalHistoryBuffer::GlobalHistoryBuffer() {
  {
    this->head = 0;
    for (ghb_ptr_t i = 0; i < GHB_ENTRIES; i++) {
      this->entries[i].next = i;
    }
  }
}

bool ghb_gac::is_valid_ghbe(ghb_ptr_t ptr) {
  bool is_too_old = calc_ghbe_distance(ptr, initial_trigger) >= GHB_ENTRIES;
  bool has_null_target = GHB.entries[entry_index(ptr)].target_addr == champsim::block_number{0};
  return !is_too_old && !has_null_target;
}

// Is this GHB entry the terminal node in the linked list
bool ghb_gac::has_next_ghbe(ghb_ptr_t ptr) {
  return GHB.entries[entry_index(ptr)].next != ptr;
}

ghb_gac::ghb_ptr_t ghb_gac::entry_index(ghb_ptr_t ptr) {
  return static_cast<ghb_ptr_t>((ptr & (GHB_PTR_SIZE - 1)) % GHB_ENTRIES);
}

std::size_t ghb_gac::calc_ghbe_distance(ghb_ptr_t lhs, ghb_ptr_t rhs) {
	// Need a logical index since entries aren't necessarily powers of 2
  std::size_t lhs_generation = lhs >> GHB_PTR_BITS;
  std::size_t rhs_generation = rhs >> GHB_PTR_BITS;
  std::size_t lhs_logical = lhs_generation * GHB_ENTRIES + entry_index(lhs);
  std::size_t rhs_logical = rhs_generation * GHB_ENTRIES + entry_index(rhs);
  std::size_t ring_size = GHB_GENERATIONS * GHB_ENTRIES;

  std::size_t direct_distance = lhs_logical >= rhs_logical ? lhs_logical - rhs_logical : rhs_logical - lhs_logical;
  return std::min(direct_distance, ring_size - direct_distance);
}

ghb_gac::ghb_ptr_t ghb_gac::make_ghb_ptr(ghb_ptr_t ptr) {
  return static_cast<ghb_ptr_t>(entry_index(ptr) | (GHB.generation << GHB_PTR_BITS));
}

ghb_gac::ghb_ptr_t ghb_gac::previous_ghb_ptr(ghb_ptr_t ptr) {
  ghb_ptr_t generation = ptr >> GHB_PTR_BITS;
  ghb_ptr_t idx = entry_index(ptr);
  if (idx == 0) {
    ghb_ptr_t prev_generation = (generation + GHB_GENERATIONS - 1) % GHB_GENERATIONS;
    return static_cast<ghb_ptr_t>((GHB_ENTRIES - 1) | (prev_generation << GHB_PTR_BITS));
  }
  return static_cast<ghb_ptr_t>((idx - 1) | (generation << GHB_PTR_BITS));
}

void ghb_gac::prefetcher_initialize() {
  std::cout << "[GHB] Initialized Index Table with INDEX_ENTRIES: " << INDEX_ENTRIES << std::endl;
  std::cout << "[GHB] Initialized GHB with GHB_ENTRIES: " << GHB_ENTRIES << std::endl;
  this->GHB.generation = 0;
  this->wide_prefetch_counter = PREFETCH_WIDTH;
  this->deep_prefetch_counter = PREFETCH_DEPTH;
  assert(GHB_GEN_BITS + GHB_PTR_BITS + 1 <= sizeof(ghb_ptr_t) * 8);
}

// Borrowed from MurmurHash3
// https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
// Good hash is more relevant when index table small and collisions likely
// Realistically, any hash could go here
uint64_t ghb_gac::fmix64 ( uint64_t k ) {
  k ^= k >> 33;
  k *= 0xff51afd7ed558ccd;
  k ^= k >> 33;
  k *= 0xc4ceb9fe1a85ec53;
  k ^= k >> 33;
  return k;
}

std::size_t ghb_gac::get_hash(champsim::address addr) {
  uint64_t mixed_bits = fmix64(champsim::block_number{addr}.to<uint64_t>());
  return mixed_bits % INDEX_ENTRIES;
}

uint32_t ghb_gac::prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit,
                                  bool useful_prefetch, access_type type, uint32_t metadata_in)
{
  if (cache_hit || type != access_type::LOAD ) {
    return metadata_in;
  }
  // Retrieve/compute relevant data
  size_t hash = get_hash(addr);
  const bool hash_collision = champsim::block_number{IT.entries[hash].trigger} != champsim::block_number{addr};
  const ghb_ptr_t head = GHB.head;
  const ghb_ptr_t head_idx = entry_index(GHB.head); // used for indexing
  // Add new GHB entry at head
  GHB.entries[head_idx].target_addr = champsim::block_number{addr};
  GHB.entries[head_idx].next = make_ghb_ptr(hash_collision ? head : IT.entries[hash].ghb_ptr);
  // Update IT entry
  IT.entries[hash].trigger = champsim::block_number{addr};
  IT.entries[hash].ghb_ptr = head;
  // Update shared data - these pointers are always one before the next prefetch
  cur_wide_ptr = head;
  initial_trigger = head;
  GHB.generation = (GHB.generation + (head_idx + 1 == GHB_ENTRIES)) % GHB_GENERATIONS;
  GHB.head = make_ghb_ptr(head + 1);
  // Begin prefetching - each new miss resets prefetcher
  wide_prefetch_counter = 0;
  deep_prefetch_counter = 0;
  return metadata_in;
}

// Prefetch one line per cycle
void ghb_gac::prefetcher_cycle_operate() {
  if (wide_prefetch_counter >= PREFETCH_WIDTH) {
    return;
  }
  // Cache old values
  // Every cycle, follow a pointer and prefetch what it points to
  const bool prefetch_wide = (deep_prefetch_counter % PREFETCH_DEPTH) == 0;

  // Go wide, if necessary
  if (prefetch_wide) {
    if (!is_valid_ghbe(cur_wide_ptr) || !has_next_ghbe(cur_wide_ptr)) {
      wide_prefetch_counter = PREFETCH_WIDTH;
      return;
    }
    cur_wide_ptr = GHB.entries[entry_index(cur_wide_ptr)].next;
    wide_prefetch_counter += 1;
    cur_deep_ptr = cur_wide_ptr;
  }

  // Go deep
  if (!is_valid_ghbe(cur_deep_ptr)) {
    deep_prefetch_counter = 0;
    return;
  }
  cur_deep_ptr = previous_ghb_ptr(cur_deep_ptr);
  deep_prefetch_counter = (deep_prefetch_counter + 1) % PREFETCH_DEPTH;

  // Perform prefetch of line
  if (!is_valid_ghbe(cur_deep_ptr)) {
    deep_prefetch_counter = 0;
    return;
  }
  champsim::block_number next_prefetch_addr = GHB.entries[entry_index(cur_deep_ptr)].target_addr;
  prefetch_line(champsim::address{next_prefetch_addr}, true, 0);
}
