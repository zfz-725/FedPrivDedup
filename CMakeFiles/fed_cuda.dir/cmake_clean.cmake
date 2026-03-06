file(REMOVE_RECURSE
  "libfed_cuda.a"
  "libfed_cuda.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang )
  include(CMakeFiles/fed_cuda.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
