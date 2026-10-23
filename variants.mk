# =============================================================================
#  BF-max Decoder variants.
#
#  Each decoder implementation is a directory inside: 
#	- src/decoders/<nome>/ 
#
#  Each directory must contains:
#	- decoder_params.h: define the parameters required by the decoder
#   - decode_prepare.c: starting the common parameters retrieve the one required my the decoder
#  
#  Options:
# 	- <name>.common: all the source file compiled
# 	- <name>.flags: we can add some flag to modify the decoder
# =============================================================================

plain.common    := decode_prepare.c decoder.c
steroids.common := decode_prepare.c decoder.c ctx.c

# --------------------------------------------------------------- versioni ----
#                 dal piu' leaky al piu' protetto
VARIANTS := plain steroids

steroids.dir         := steroids
steroids.src         :=
steroids.flags       := -DVT_FIND

# bfmax.h -> decoder base (argmax CT ricalcolato, update riga per riga)
plain.dir         := plain
plain.src         :=
plain.flags       :=

