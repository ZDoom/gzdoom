cmake_minimum_required( VERSION 3.1.0 )

make_release_only()

add_definitions( -DBZ_NO_STDIO )
add_library( bz2 STATIC
    blocksort.c
    bzlib.c
    compress.c
    crctable.c
    decompress.c
    huffman.c
    randtable.c )
target_link_libraries( bz2 )
