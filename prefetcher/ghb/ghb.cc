#include "ghb.h"

#include <cassert>
#include <cinttypes>
#include <iostream>
#include "cache.h"
#include "champsim.h"
#include "extent.h"

ghb::IndexTable::IndexTable() {
  {
    for (size_t i = 0; i < INDEX_ENTRIES; i++) {
      this->entries[i].trigger = champsim::block_number{0};
    }
  }
}

ghb::GlobalHistoryBuffer::GlobalHistoryBuffer() {
  {
    this->head = 0;
    for (ghb_ptr_t i = 0; i < GHB_ENTRIES; i++) {
      this->entries[i].next = i;
    }
  }
}

bool ghb::is_valid_ghbe(ghb_ptr_t ptr) {
  bool is_too_old = std::abs(static_cast<int>(ptr) - static_cast<int>(initial_trigger)) >= GHB_ENTRIES;
  bool has_null_target = GHB.entries[ptr % GHB_PTR_SIZE].target_addr == champsim::block_number{0};
  return !is_too_old && !has_null_target;
}

// Is this GHB entry the terminal node in the linked list
bool ghb::has_next_ghbe(ghb_ptr_t ptr) {
  return GHB.entries[ptr % GHB_PTR_SIZE].next != ptr;
}

ghb::ghb_ptr_t ghb::make_ghb_ptr(ghb_ptr_t ptr) {
  return static_cast<ghb_ptr_t>(((ptr & (GHB_PTR_SIZE-1)) % GHB_ENTRIES) | (GHB.generation << GHB_PTR_BITS));
}

void ghb::prefetcher_initialize() {
  std::cout << "[GHB] Initialized Index Table with INDEX_ENTRIES: " << INDEX_ENTRIES << std::endl;
  std::cout << "[GHB] Initialized GHB with GHB_ENTRIES: " << GHB_ENTRIES << std::endl;
  this->GHB.generation = 0;
  this->wide_prefetch_counter = PREFETCH_WIDTH;
  this->deep_prefetch_counter = PREFETCH_DEPTH;
  assert(GHB_GEN_BITS + GHB_PTR_BITS + 1 <= sizeof(ghb_ptr_t) * 8);
}

std::size_t ghb::get_hash(champsim::address addr) {
  champsim::data::bits ubound{champsim::lg2(INDEX_ENTRIES) + LOG2_BLOCK_SIZE};
  champsim::data::bits lbound{LOG2_BLOCK_SIZE};
  champsim::address_slice hash{ champsim::dynamic_extent{ubound, lbound}, addr};

  return hash.to<std::size_t>();
}

uint32_t ghb::prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit,
                                  bool useful_prefetch, access_type type, uint32_t metadata_in)
{
  if (cache_hit || type != access_type::LOAD ) {
    return 0;
  }
  // Retrieve/compute relevant data
  size_t hash = get_hash(addr);
  // std::cout << std::endl << "[DEBUG] addr: " << addr << " hash: " << hash << std::endl;
  const bool hash_collision = champsim::block_number{IT.entries[hash].trigger} != champsim::block_number{addr};
  // std::cout << "[DEBUG] block_number{addr}: " << champsim::block_number{addr} << " IT.entries[hash].trigger: " << champsim::block_number{IT.entries[hash].trigger} << " collision: " << hash_collision << std::endl;
  const ghb_ptr_t head = GHB.head;
  const ghb_ptr_t head_idx = GHB.head % GHB_PTR_SIZE; // used for indexing
  // std::cout << "[DEBUG] head: " << head << std::endl;
  // Add new GHB entry at head
  GHB.entries[head_idx].target_addr = champsim::block_number{addr};
  GHB.entries[head_idx].next = make_ghb_ptr(hash_collision ? head : IT.entries[hash].ghb_ptr);
  // std::cout << "[DEBUG] :: next: " << GHB.entries[head_idx].next << " generation: " << GHB.generation << std::endl;
  if (!hash_collision) {
    std::cout << std::endl << "[DEBUG] Found correlation when head: " << head << " @ IT.entries[hash].ghb_ptr: " << IT.entries[hash].ghb_ptr << std::endl;
  }
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
  return 0;
}

// Prefetch one line per cycle
void ghb::prefetcher_cycle_operate() {
  // std::cout << "[DEBUG] 2" << std::endl;
  if (wide_prefetch_counter >= PREFETCH_WIDTH) {
    return;
  }
  // std::cout << "[DEBUG] Let's prefetch!" << std::endl;
  // Cache old values
  // Every cycle, follow a pointer and prefetch what it points to
  const bool prefetch_wide = (deep_prefetch_counter % PREFETCH_DEPTH) == 0;

  // Go wide, if necessary
  if (prefetch_wide) {
    if (!is_valid_ghbe(cur_wide_ptr) || !has_next_ghbe(cur_wide_ptr)) {
      // std::cout << "[DEBUG] (1.a) Darn.. is_valid_ghbe: " << is_valid_ghbe(cur_wide_ptr) << " has_next_ghbe: " << has_next_ghbe(cur_wide_ptr)  << std::endl;
      wide_prefetch_counter = PREFETCH_WIDTH;
      return;
    }
    std::cout << "[DEBUG] (1.b) Go Wide" << std::endl;
    cur_wide_ptr = GHB.entries[cur_wide_ptr % GHB_PTR_SIZE].next;
    wide_prefetch_counter += 1;
    cur_deep_ptr = cur_wide_ptr;
  }

  // Go deep
  if (!is_valid_ghbe(cur_deep_ptr)) {
    std::cout << "[DEBUG] (2.a) Darn.. is_valid_ghbe: " << is_valid_ghbe(cur_deep_ptr) << " has_next_ghbe: " << has_next_ghbe(cur_deep_ptr)  << std::endl;
    std::cout << "        initial_trigger_ghbe: " << initial_trigger << " {gen: " << (initial_trigger >> GHB_PTR_BITS) << ", index: " << (initial_trigger & (GHB_PTR_SIZE-1)) << "}" << std::endl;
    std::cout << "        GHB.entries[" << cur_deep_ptr << "]: {target: " << GHB.entries[cur_deep_ptr % GHB_PTR_SIZE].target_addr << ", next: " << GHB.entries[cur_deep_ptr % GHB_PTR_SIZE].next << "}" << std::endl;
    std::cout << "        GHB.entries[{gen: " << (cur_deep_ptr >> GHB_PTR_BITS) << ", idx: " << (cur_deep_ptr % GHB_PTR_SIZE) << "}" << std::endl;
    deep_prefetch_counter = 0;
    return;
  }
  std::cout << "[DEBUG] (2.b) Go Deep" << std::endl;
  // std::cout << "[Debug] Subtracting one from " << cur_deep_ptr << " gives " << (cur_deep_ptr - 1) % GHB_PTR_SIZE << std::endl;
  cur_deep_ptr = cur_deep_ptr == 0 ? GHB_ENTRIES - 1 : cur_deep_ptr - 1;
  deep_prefetch_counter = (deep_prefetch_counter + 1) % PREFETCH_DEPTH;

  // Perform prefetch of line
  if (!is_valid_ghbe(cur_deep_ptr)) {
    deep_prefetch_counter = 0;
    return;
  }
  std::cout << "[Debug] (3) Line valid. attemping prefetch" << std::endl;
  champsim::block_number next_prefetch_addr = GHB.entries[cur_deep_ptr % GHB_PTR_SIZE].target_addr;
  prefetch_line(champsim::address{next_prefetch_addr}, true, 0);
}

// uint32_t ghb::prefetcher_cache_fill(
//   champsim::address addr, uint32_t set, uint32_t way,
//   bool prefetch, champsim::address evicted_address, uint32_t metadata_in
// )
// {
//   return metadata_in;
// }
