# -----------------------------------------------------------------
#
# The core Trameleon library
#
# -----------------------------------------------------------------

set(tra_base_dir ${CMAKE_CURRENT_LIST_DIR}/../../)
set(tra_src_dir ${tra_base_dir}/src)
set(tra_inc_dir ${tra_base_dir}/include)

# -----------------------------------------------------------------

list(APPEND tra_sources
  ${tra_src_dir}/tra/buffer.c
  ${tra_src_dir}/tra/log.c
  ${tra_src_dir}/tra/core.c
  ${tra_src_dir}/tra/registry.c
  ${tra_src_dir}/tra/module.c
  ${tra_src_dir}/tra/utils.c
  ${tra_src_dir}/tra/dict.c
  ${tra_src_dir}/tra/golomb.c
  ${tra_src_dir}/tra/avc.c
  ${tra_src_dir}/tra/types.c
  ${tra_src_dir}/tra/time.c
  ${tra_src_dir}/tra/profiler.c
  ${tra_src_dir}/tra/easy.c
  ${tra_src_dir}/tra/modules/easy/easy-encoder.c
  ${tra_src_dir}/tra/modules/easy/easy-decoder.c
  ${tra_src_dir}/tra/modules/easy/easy-transcoder.c
  #${tra_src_dir}/dev/generator.c 
  )

# -----------------------------------------------------------------

include_directories(
  ${tra_inc_dir}
  )

# -----------------------------------------------------------------

