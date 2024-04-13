// Copyright 2019 <Andrea Cognolato>
// TODO(andrea): use a better map, try in some way to have dynamic gpu memory
// allocation, find a better data structure
#ifndef SRC_TEXTURE_ATLAS_H_
#define SRC_TEXTURE_ATLAS_H_

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H

#include <harfbuzz/hb.h>
typedef hb_codepoint_t hb_glyph_id_t; // confusingly, the same type

#include <glad/glad.h>

#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace texture_atlas {
using std::pair;
using std::unordered_map;
using std::vector;


struct Character {
  size_t texture_array_index;
  glm::vec2 texture_coordinates;
  size_t texture_id;
  glm::ivec2 size;
  glm::ivec2 bearing;

  GLuint advance;

  bool colored;
};

typedef vector<unsigned char> bitmap_buffer_t;

class TextureAtlas {
 private:
  typedef struct {
    Character character;
    bool fresh;
  } cache_element_t;

  GLuint index_ = 0;
  GLuint texture_;
  GLsizei textureWidth_, textureHeight_;

  unordered_map<hb_glyph_id_t, cache_element_t> texture_cache_;
  GLenum format_;

 public:
  TextureAtlas(GLsizei textureWidth, GLsizei textureHeight,
               GLuint shaderProgramId, const char* textureUniformLocation,
               GLenum internalformat, GLenum format,
               GLint shader_texture_index);

  ~TextureAtlas();

  void Insert(const bitmap_buffer_t& bitmap_buffer, GLsizei width,
              GLsizei height, Character* ch, GLuint offset);

  void Append(pair<Character, bitmap_buffer_t>* p,
              hb_glyph_id_t glyph_id);

  void Replace(pair<Character, bitmap_buffer_t>* p, hb_glyph_id_t stale,
               hb_glyph_id_t glyph_id);

  bool Contains(hb_glyph_id_t glyph_id) const;

  Character* Get(hb_glyph_id_t glyph_id);
  void Insert(hb_glyph_id_t glyph_id,
              pair<Character, bitmap_buffer_t>* ch);

  pair<Character, bitmap_buffer_t> RenderGlyph(FT_Face face,
                                                     hb_glyph_id_t glyph_id);

  bool IsFull() const;

  bool Contains_stale() const;

  void Invalidate();

  GLuint GetTexture() const;
};
}  // namespace texture_atlas

#endif  // SRC_TEXTURE_ATLAS_H_
