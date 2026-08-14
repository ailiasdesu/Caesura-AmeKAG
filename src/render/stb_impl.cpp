// stb_impl.cpp — single implementation TU for stb_image / stb_image_write.
// Keeps the STB_*_IMPLEMENTATION definitions inside src/ (AGENTS.md §6/§7:
// no "../../external/..." escapes) while preserving the symbol ownership
// in the render module: every other TU includes <stb/stb_image.h> (decls)
// and links against these definitions.
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
