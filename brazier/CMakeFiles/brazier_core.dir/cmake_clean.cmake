file(REMOVE_RECURSE
  "libbrazier_core.a"
  "libbrazier_core.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/brazier_core.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
