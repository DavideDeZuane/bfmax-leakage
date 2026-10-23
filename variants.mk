# =============================================================================
#  Decoder e versioni.
#
#  DECODER: una cartella src/decoders/<nome>/ autonoma, con le proprie
#  funzioni e il proprio loop. Deve fornire:
#    decoder_params.h   dec_params_t
#    decode_prepare / decode_native / decode / decoder_variant_name
#  <nome>.common elenca i file compilati per TUTTE le versioni del decoder.
#
#  VERSIONE: un decoder + i file che la distinguono dalle altre versioni
#  dello stesso decoder (.src) + flag (.flags).
#
#  Per aggiungere un decoder: la cartella, una riga <nome>.common, le sue
#  versioni. Per aggiungere una versione: riga in VARIANTS, .dir/.src/.flags,
#  riga in docs/expected.csv.
# =============================================================================

# ---------------------------------------------------------------- decoder ----
# steroids.h: loop comune + kernel intercambiabili (contratto in kernels.h)
steroids.common := prepare.c decoder.c ctx.c

# bfmax.h: decoder base, un'unica implementazione
plain.common    := prepare.c decoder.c

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

