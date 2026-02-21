I intend to implement a global address correlation-based prefetcher using a Global History Buffer (GHB) for the L2 cache. 

The original GHB could be redundant by storing multiple copies of a physical address in the table. Thus, I intend to reduce this redundancy by modifying the GHB such that it no longer stores absolute memory addresses, but instead stores pointers back into the index table.
The original and modified designs are shown in the following diagrams. For clarity, only the pointers for address A are shown.

In the new design, the section highlighted in grey is the modified section.

This presents a new problem in that a GHB entry, to be prefetched, must also contain a corresponding entry in the index table.
Further, there is now a risk that eviction from the index table could lead to invalid pointers to that entry.
The hope is that by saving space in the GHB, the size both structures can be increased, reducing this risk.

A solution to this is to no longer use generation counts to invalidate entries in the index table, but instead maintain a valid/invalid bit for each entry of the GHB.
Upon GHB insertion, this would be valid.
Upon index eviction, a hardware structure could manually walk through the structure and invalidate values.

For the following hardware budgets, {4, 32}KB, I will perform a sweep over:
- the size of the GHB (roughly {20,40,60,80}% of HW budget) and the size of the index table (roughly {80,60,40,20}% of HW budget)
- A test comparing a 4-bit generation counter to the invalidation hardware mentioned. 

This is 16 different configurations.

I will compare against the baselines of:
- An L1 IP stride prefetcher, and no L2 prefetcher
- An L1 IP stride prefetcher, and an address-correlation based GHB with a 1 million entry GHB and index table respectively to show the overhead.

All tests will be done using the 16x16 hybrid scheme discussed in the paper, which means prefetch 16 addresses deep and 16 addresses wide.
