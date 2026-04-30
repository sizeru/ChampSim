#include "ghb.h"

#include <cassert>
#include <cinttypes>
#include <iostream>
#include "cache.h"
#include "extent.h"

ghb::IndexTable::IndexTable() {
  {
    for (size_t i = 0; i < INDEX_ENTRIES; i++) {
      this->entries[i].trigger = champsim::address{0};
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

ghb::ghb_ptr_t ghb::make_ghb_ptr(ghb_ptr_t ptr) {
  return static_cast<ghb_ptr_t>((ptr & (GHB_PTR_SIZE-1)) | (GHB.generation << GHB_PTR_BITS));
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
  // std::cout << "[DEBUG] 1" << std::endl;
  if (cache_hit || type != access_type::LOAD ) {
    return 0;
  }
  // Retrieve/compute relevant data
  size_t hash = get_hash(addr);
  std::cout << "[DEBUG] addr: " << addr << " hash: " << hash << std::endl;
  const bool hash_collision = champsim::block_number{IT.entries[hash].trigger} != champsim::block_number{addr};
  std::cout << "[DEBUG] " << " IT.entries[hash].trigger " << champsim::block_number{IT.entries[hash].trigger} << " collision: " << hash_collision << std::endl;
  const ghb_ptr_t head = GHB.head % GHB_ENTRIES;
  std::cout << "[DEBUG] head: " << head << std::endl;
  // Add new GHB entry at head
  // std::cout << "[DEBUG] 3" << std::endl;
  // std::cout << "[DEBUG] head: " << head << std::endl;
  // std::cout << "[DEBUG] hash collision: " << hash_collision << std::endl;
  GHB.entries[head].target_addr = champsim::block_number{addr};
  GHB.entries[head].next = make_ghb_ptr(hash_collision ? head : IT.entries[hash].ghb_ptr);
  std::cout << "[DEBUG] GHB.entries[head].next: " << GHB.entries[head].next << std::endl;
  // Update IT entry
  // std::cout << "[DEBUG] 4" << std::endl;
  IT.entries[hash].trigger = champsim::block_number{addr};
  IT.entries[hash].ghb_ptr = head;
  // Update shared data - these pointers are always one before the next prefetch
  // std::cout << "[DEBUG] 5" << std::endl;
  cur_wide_ptr = head;
  initial_trigger = head;
  GHB.generation += (head + 1 == GHB_ENTRIES) % champsim::next_pow2(GHB_GEN_BITS);
  GHB.head = make_ghb_ptr((head + 1) % GHB_ENTRIES);
  // Begin prefetching
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
  // Cache old values
  // Every cycle, follow a pointer and prefetch what it points to
  const bool prefetch_wide = (deep_prefetch_counter % PREFETCH_DEPTH) == 0;
  if (prefetch_wide) {
    if (std::abs(static_cast<int>(cur_wide_ptr) - static_cast<int>(initial_trigger)) >= GHB_PTR_SIZE) {
      wide_prefetch_counter = PREFETCH_WIDTH;
      return;
    }
    if (cur_wide_ptr == GHB.entries[cur_wide_ptr].next) {
      wide_prefetch_counter = PREFETCH_WIDTH;
      return;
    }
    cur_wide_ptr = GHB.entries[cur_wide_ptr].next;
    wide_prefetch_counter += 1;
    cur_deep_ptr = cur_wide_ptr;
  }

  // Go deep
  if (
    deep_prefetch_counter >= PREFETCH_DEPTH
    || std::abs(static_cast<int>(cur_deep_ptr) - static_cast<int>(initial_trigger)) >= GHB_PTR_SIZE
  ) {
    deep_prefetch_counter = 0;
    return;
  }
  if (cur_deep_ptr == GHB.entries[cur_deep_ptr].next) {
    deep_prefetch_counter = 0;
    return;
  }
  cur_deep_ptr = (cur_deep_ptr - 1) % GHB_ENTRIES;
  deep_prefetch_counter += 1;
  champsim::address next_prefetch_addr = GHB.entries[cur_deep_ptr].target_addr;
  prefetch_line(next_prefetch_addr, true, 0);
}

uint32_t ghb::prefetcher_cache_fill(
  champsim::address addr, uint32_t set, uint32_t way,
  bool prefetch, champsim::address evicted_address, uint32_t metadata_in
)
{
  return metadata_in;
}
