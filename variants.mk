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
plain.common    := prepare.c bfmax.c

# --------------------------------------------------------------- versioni ----
#                 dal piu' leaky al piu' protetto
VARIANTS := vt ct_find fixed_neg fixed_scatter plain

# steroids.h -DVT_FIND -> find con early exit, segno adattivo
vt.dir            := steroids
vt.src            := find_vt.c       update_scatter.c
vt.flags          :=

# steroids.h (default) -> find CT, segno adattivo
ct_find.dir       := steroids
ct_find.src       := find_ct.c       update_scatter.c
ct_find.flags     :=

# steroids.h -DFIXED_NEG -> find CT, segno fisso (scatter di lunghezza variabile)
fixed_neg.dir     := steroids
fixed_neg.src     := find_ct.c       update_scatter.c
fixed_neg.flags   := -DFIXED_NEG

# steroids.h -DFIXED_SCATTER -> find CT, scatter di lunghezza fissa N0*V*V
fixed_scatter.dir := steroids
fixed_scatter.src := find_ct.c       update_fixed_scatter.c
fixed_scatter.flags :=

# bfmax.h -> decoder base (argmax CT ricalcolato, update riga per riga)
plain.dir         := plain
plain.src         :=
plain.flags       :=

