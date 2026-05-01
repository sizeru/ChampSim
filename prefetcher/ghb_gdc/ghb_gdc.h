#ifndef ghb_gdc_H
#define ghb_gdc_H

#include <cstdint>

#include "cache.h"
#include "modules.h"

#ifndef INDEX_ENTRIES
#define INDEX_ENTRIES 1024
#endif

#ifndef GHB_ENTRIES
#define GHB_ENTRIES 4096
#endif

// Global Delta Correlation prefetcher using Global History Buffer
struct ghb_gdc : public champsim::modules::prefetcher {

  // Parameters
  constexpr static std::size_t GHB_GENERATIONS = 16;
  constexpr static std::size_t PREFETCH_DEPTH = 16;
  constexpr static std::size_t PREFETCH_WIDTH = 16;
  constexpr static std::size_t GHB_GEN_BITS = champsim::lg2(GHB_GENERATIONS);
  constexpr static std::size_t GHB_PTR_SIZE = champsim::next_pow2(GHB_ENTRIES);
  constexpr static std::size_t GHB_PTR_BITS = champsim::lg2(GHB_PTR_SIZE);

  using ghb_ptr_t = unsigned;
  using delta_t = int64_t;

  using prefetcher::prefetcher;
  void prefetcher_initialize();
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit,
                                    bool useful_prefetch, access_type type, uint32_t metadata_in);

  void prefetcher_cycle_operate();

  std::size_t get_hash(delta_t delta);
  static ghb_ptr_t entry_index(ghb_ptr_t ptr);
  static std::size_t calc_ghbe_distance(ghb_ptr_t lhs, ghb_ptr_t rhs);
  uint64_t fmix64 ( uint64_t k );
  bool is_valid_ghbe(ghb_ptr_t ptr);
  bool has_next_ghbe(ghb_ptr_t ptr);
  ghb_ptr_t make_ghb_ptr(ghb_ptr_t ptr);
  ghb_ptr_t previous_ghb_ptr(ghb_ptr_t ptr);

  // Entries are accessed by a hash of the observed delta, but the full delta is
  // kept to detect hash collisions.
  struct ItEntry {
    delta_t trigger_delta;
    ghb_ptr_t ghb_ptr;
  };
  class IndexTable {
    public:
    ItEntry entries[INDEX_ENTRIES];
    IndexTable();
  };

  struct GhbEntry {
    champsim::block_number target_addr;
    delta_t delta;
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
  champsim::block_number last_miss_addr = champsim::block_number{0};
  bool has_last_miss = false;
};

#endif /* GHB_GDC_H */
