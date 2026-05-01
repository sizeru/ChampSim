WARM_INS = 25000000
SIM_INS = 100000000
OUTDIR = out

# TRACEDIR = traces
TRACEDIR = /scratch/cluster/speedway/cs395t/hw1/part2/traces
TRACES = \
	/scratch/cluster/speedway/cs395t/hw1/part2/traces/bfs.trace.gz \
	/scratch/cluster/speedway/cs395t/hw1/part2/traces/cc.trace.gz \
	/scratch/cluster/speedway/cs395t/hw1/part2/traces/astar_313B.trace.gz \
	/scratch/cluster/speedway/cs395t/hw1/part2/traces/mcf_46B.trace.gz \
	/scratch/cluster/speedway/cs395t/hw1/part2/traces/omnetpp_340B.trace.gz \
	/scratch/cluster/speedway/cs395t/hw1/part2/traces/soplex_66B.trace.gz \
	/scratch/cluster/speedway/cs395t/hw1/part2/traces/sphinx3_2520B.trace.gz \
	/scratch/cluster/speedway/cs395t/hw1/part2/traces/xalancbmk_99B.trace.gz

# BINS = \
#   champsim_skylake \
# 	champsim_skylake_l2ghb_gac_i128_g2048 \
# 	champsim_skylake_l2ghb_gac_i128_g262144 \
# 	champsim_skylake_l2ghb_gac_i262144_g2048 \
# 	champsim_skylake_l2ghb_gac_i262144_g262144 \
# 	champsim_skylake_l2ghb_gdc_i128_g2048 \
# 	champsim_skylake_l2ghb_gdc_i128_g262144 \
# 	champsim_skylake_l2ghb_gdc_i262144_g2048 \
# 	champsim_skylake_l2ghb_gdc_i262144_g262144

OUTS += $(foreach trace, \
					    $(TRACES), \
					 		$(OUTDIR)/$(basename $(basename $(notdir $(trace))))/champsim_skylake)
OUTS += $(foreach trace, \
					    $(TRACES), \
					 		$(OUTDIR)/$(basename $(basename $(notdir $(trace))))/champsim_skylake_l2ghb_gac_i128_g2048)
OUTS += $(foreach trace, \
					    $(TRACES), \
					 		$(OUTDIR)/$(basename $(basename $(notdir $(trace))))/champsim_skylake_l2ghb_gac_i128_g262144)
OUTS += $(foreach trace, \
					    $(TRACES), \
					 		$(OUTDIR)/$(basename $(basename $(notdir $(trace))))/champsim_skylake_l2ghb_gac_i262144_g2048)
OUTS += $(foreach trace, \
					    $(TRACES), \
					 		$(OUTDIR)/$(basename $(basename $(notdir $(trace))))/champsim_skylake_l2ghb_gac_i262144_g262144)
OUTS += $(foreach trace, \
					    $(TRACES), \
					 		$(OUTDIR)/$(basename $(basename $(notdir $(trace))))/champsim_skylake_l2ghb_gdc_i128_g2048)
OUTS += $(foreach trace, \
					    $(TRACES), \
					 		$(OUTDIR)/$(basename $(basename $(notdir $(trace))))/champsim_skylake_l2ghb_gdc_i128_g262144 )
OUTS += $(foreach trace, \
					    $(TRACES), \
					 		$(OUTDIR)/$(basename $(basename $(notdir $(trace))))/champsim_skylake_l2ghb_gdc_i262144_g2048 )
OUTS += $(foreach trace, \
					    $(TRACES), \
					 		$(OUTDIR)/$(basename $(basename $(notdir $(trace))))/champsim_skylake_l2ghb_gdc_i262144_g262144)


.PHONY: $(OUTS)

runall: $(OUTS)

$(OUTS):
	mkdir -p $(dir $@)
	./bin/$(notdir $@) \
		--warmup-instructions ${WARM_INS} \
		--simulation-instructions ${SIM_INS} \
		--json $@.json \
		$(TRACEDIR)/$(word 2,$(subst /, ,$@)).trace.gz \
		2>&1 | tee $@.txt;
