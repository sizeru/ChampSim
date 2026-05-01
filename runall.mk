WARM_INS = 25000000
SIM_INS = 100000000
OUTDIR = out

TRACES = \
	traces/bfs.trace.gz \
	traces/cc.trace.gz \
	traces/astar_313B.trace.gz \
	traces/mcf_46B.trace.gz \
	traces/omnetpp_340B.trace.gz \
	traces/soplex_66B.trace.gz \
	traces/sphinx3_2520B.trace.gz \
	traces/xalancbmk_99B.trace.gz

BINS = \
  champsim_skylake \
	champsim_skylake_l2ghb_gac_i128_g2048 \
	champsim_skylake_l2ghb_gac_i128_g262144 \
	champsim_skylake_l2ghb_gac_i262144_g2048 \
	champsim_skylake_l2ghb_gac_i262144_g262144 \
	champsim_skylake_l2ghb_gdc_i128_g2048 \
	champsim_skylake_l2ghb_gdc_i128_g262144 \
	champsim_skylake_l2ghb_gdc_i262144_g2048 \
	champsim_skylake_l2ghb_gdc_i262144_g262144

.PHONY: $(BINS)

runall: $(BINS)

$(BINS):
	for trace in $(TRACES); do \
	  name="$(basename $(basename $(notdir traces/bfs.trace.gz)))"; \
		mkdir -p $(OUTDIR)/$$name; \
	  ./bin/$@ \
			--warmup-instructions ${WARM_INS} \
			--simulation-instructions ${SIM_INS} \
			--json $(OUTDIR)/$@.json \
			$$trace \
			2>&1 | tee $(OUTDIR)/@.txt; \
	done
