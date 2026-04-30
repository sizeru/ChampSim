#ifndef GHB_H
#define GHB_H

#include <bitset>

#include "cache.h"
#include "modules.h"
#include "msl/lru_table.h"

// Global History Buffer
struct ghb : public champsim::modules::prefetcher {

  // Parameters
  constexpr static std::size_t GHB_ENTRIES = 1024;
  constexpr static std::size_t INDEX_ENTRIES = 128;
  constexpr static std::size_t PREFETCH_DEPTH = 16;
  constexpr static std::size_t PREFETCH_WIDTH = 16;
  constexpr static std::size_t GHB_GEN_BITS = 4;
  constexpr static std::size_t GHB_PTR_SIZE = champsim::next_pow2(GHB_ENTRIES);
  constexpr static std::size_t GHB_PTR_BITS = champsim::lg2(GHB_ENTRIES);
  // Derived params
  // constexpr static champsim::data::bits GHB_PTR_SIZE{  };

  using ghb_ptr_t = unsigned;

  using prefetcher::prefetcher;
  void prefetcher_initialize();
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit,
                                    bool useful_prefetch, access_type type, uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(
    champsim::address addr, uint32_t set, uint32_t way,
    bool prefetch, champsim::address evicted_address, uint32_t metadata_in
  );

  void prefetcher_cycle_operate();
  // void prefetcher_final_stats();


  std::size_t get_hash(champsim::address addr);
  ghb_ptr_t make_ghb_ptr(ghb_ptr_t ptr);

  // Entries in the index table contain the entire address of the triggering miss.
  // Entries are still accessed by a hash of the triggering miss,
  // but the full address is used to detect collisions.
  struct ItEntry {
    champsim::address trigger;
    ghb_ptr_t ghb_ptr;
  };
  class IndexTable {
    public:
    ItEntry entries[INDEX_ENTRIES];
    IndexTable();
  };

  struct GhbEntry {
    champsim::address target_addr;
    ghb_ptr_t next;
  };
  class GlobalHistoryBuffer {
  public:
    GhbEntry entries[GHB_ENTRIES];
    ghb_ptr_t head;
    size_t generation;
    GlobalHistoryBuffer();
  };

  IndexTable IT;
  GlobalHistoryBuffer GHB;
  ghb_ptr_t cur_deep_ptr;
  ghb_ptr_t cur_wide_ptr;
  size_t wide_prefetch_counter;
  size_t deep_prefetch_counter;
  ghb_ptr_t initial_trigger;
};

#endif /* GHB */
