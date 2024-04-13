// Copyright 2019 <Andrea Cognolato>
#ifndef SRC_RENDERER_H_
#define SRC_RENDERER_H_

#include <glad/glad.h>

#include <string>
#include <utility>
#include <vector>
#include <array>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include "./face_collection.h"
#include "./shader.h"
#include "./state.h"
#include "./texture_atlas.h"

namespace renderer {
using face_collection::FaceCollection;
using state::State;
using std::array;
using std::get;
using std::pair;
using std::string;
using std::unordered_map;
using std::vector;
using texture_atlas::Character;
using texture_atlas::TextureAtlas;
using texture_atlas::bitmap_buffer_t;

struct RenderedGlyph {
  size_t face;
  hb_glyph_id_t glyph_id;
  hb_glyph_position_t position;
  bool is_space;
  hb_position_t final_position_x, final_position_y; // relative to line origin, 64*pixel
};
typedef vector<vector<RenderedGlyph>> ShapingCache;

ShapingCache LayoutText(const vector<string>& lines, const FaceCollection &faces, size_t mono_index, rmode_t mode);
void Render(const Shader &shader, const vector<string> &lines,
            const FaceCollection &faces, ShapingCache *shaping_cache,
            const vector<TextureAtlas *> &texture_atlases, const State &state,
            GLuint VAO, GLuint VBO, size_t mono_index);
pair<Character, bitmap_buffer_t> RenderGlyph(FT_Face face,
                                                   hb_glyph_id_t glyph_id);
}  // namespace renderer

#endif  // SRC_RENDERER_H_
