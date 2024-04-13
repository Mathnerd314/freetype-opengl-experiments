// Copyright 2019 <Andrea Cognolato>
#include "./renderer.h"
#include "./constants.h"

namespace renderer {
using std::min;
using std::max;
ShapingCache LayoutText(const vector<string>& lines, const FaceCollection &faces, size_t mono_index, rmode_t mode) {
  printf("layout mode %i", mode);
  const hb_glyph_id_t CODEPOINT_MISSING_FACE = UINT32_MAX;
  const hb_glyph_id_t CODEPOINT_MISSING = UINT32_MAX;

  ShapingCache shaping_cache(lines.size());

  vector<size_t> face_idxs{mono_index, 1};

  size_t max_line_length = 0;
  // For each line of the file
  for (unsigned int ix = 0; ix < lines.size(); ix++) {
    auto &line = lines[ix];

    // Calculate which glyph_ids and faces to render the line with.
    auto& rendered_glyphs = shaping_cache[ix];
    // Flag to break the for loop when all of the glyph_ids have been assigned
    // to a face
    bool all_glyph_ids_have_a_face = false;
    for (size_t face_idx_idx = 0; (face_idx_idx < face_idxs.size()) && !all_glyph_ids_have_a_face; face_idx_idx++) {
      size_t face_idx = face_idxs[face_idx_idx];
      printf("face_idx %i", face_idx);
      all_glyph_ids_have_a_face = true;

      // Create the shaping buffer
      // The shaping buffer should contain each logical "paragraph", but as we do not wrap
      // lines this simply means the buffer should contain a line
      hb_buffer_t *buf = hb_buffer_create();

      // Put the text in the buffer
      hb_buffer_add_utf8(buf, line.data(), line.length(), 0, -1);

      // Set the script, language and direction of the buffer
      hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
      hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
      hb_buffer_set_language(buf, hb_language_from_string("en", -1));

      // Use the created hb_font
      hb_font_t* font = faces[face_idx].hb_ft_font;

      vector<hb_feature_t> features(3);
      assert(hb_feature_from_string("kern=1", -1, &features[0]));
      assert(hb_feature_from_string("liga=1", -1, &features[1]));
      assert(hb_feature_from_string("clig=1", -1, &features[2]));

      // Shape the font
      hb_shape(font, buf, &features[0], features.size());

      // Get the glyph and position information
      unsigned int glyph_info_length;
      unsigned int glyph_position_length;
      hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_info_length);
      hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_position_length);
      assert(glyph_info_length == glyph_position_length);

      // Assign a size to the vector on the first iteration and fill it with
      // UINT32_MAX which represents the absence of a value. This assumes that
      // all of the face runs will have the same lengths
      if (face_idx_idx == 0) {
        RenderedGlyph defaultGlyph{CODEPOINT_MISSING_FACE, CODEPOINT_MISSING, {0, 0, 0, 0, 0}, false, 0, 0};
        rendered_glyphs.resize(glyph_info_length, defaultGlyph);
      }
      assert(glyph_info_length == rendered_glyphs->size());
      max_line_length = std::max(max_line_length, (size_t)glyph_info_length);

      // Store each glyph if the characters haven't been assigned yet
      for (size_t j = 0; j < glyph_info_length; j++) {
        hb_glyph_id_t glyph_id = glyph_info[j].codepoint;

        // Newly rendered glyph
        if (glyph_id != 0 &&
            rendered_glyphs[j].glyph_id == CODEPOINT_MISSING) {
          rendered_glyphs[j].face = face_idx;
          rendered_glyphs[j].glyph_id = glyph_id;
          rendered_glyphs[j].position = glyph_pos[j];

          // Fix scale of emoji font
          if (FT_HAS_COLOR(faces[face_idx].face)) {
            rendered_glyphs[j].position.x_advance = kFontPixelWidth * 64;
          }

          // Hacky check for space
          rendered_glyphs[j].is_space = ' ' == line[glyph_info[j].cluster];
        }

        // If we find a glyph which is not present in this face (therefore its
        // glyph_id it's 0) and which has not been assigned already then we
        // need to iterate on the next font
        if (glyph_id == 0 &&
            rendered_glyphs[j].glyph_id == CODEPOINT_MISSING) {
          all_glyph_ids_have_a_face = false;
        }
      }

      // Destroy the shaping buffer
      hb_buffer_destroy(buf);
    }

    // Fallback for missing characters
    for (size_t i = 0; i < rendered_glyphs.size(); i++) {
      size_t face = rendered_glyphs[i].face;
      hb_glyph_id_t glyph_id = rendered_glyphs[i].glyph_id;
      if (face == CODEPOINT_MISSING_FACE && glyph_id == CODEPOINT_MISSING) {
        const auto REPLACEMENT_CHARACTER = 0x0000FFFD;
        rendered_glyphs[i].face = mono_index;
        if (hb_font_get_glyph(faces[mono_index].hb_ft_font, REPLACEMENT_CHARACTER, 0, &rendered_glyphs[i].glyph_id) == false) {
          fprintf(stderr, "Error loading replacement glyph\n");
          exit(EXIT_FAILURE);
        }
        rendered_glyphs[i].position.x_advance = hb_font_get_glyph_h_advance(faces[mono_index].hb_ft_font, rendered_glyphs[i].glyph_id);
        // assume 0 for other attributes
      }
    }
  }

  // Calculate monospace column adjustment
  // Definitions:
  // Character - For now we assume 1 glyph = 1 character. But properly we should check a monospace font in case a glyph spans multiple characters
  // Cell - a non-space sequence of characters, possibly followed by a space
  // Cell minimum width - the x-advance of all characters in the cell (including space)
  // preferred width/percent - not used
  // Cell colspan - the number of characters in the cell

  // Algorithm: overall, we identify the minimum start of each column. (as opposed to CSS3 automatic table layout algorithm based on minimum width)
  // We loop over each line and cell and update minimum starts based on previous starts and width constraints.
  // For example, suppose minimum starts are [0x3, 10x5] and we have "|a |cat |" which is 5 and 20.
  // Then the updated minimum starts are [0,0,5,10,10,10,25,25]
  // Probably there is an optimal iteration order but for now we just loop until fixedpoint
  // it should converge in O(line width) or less
  vector<vector<hb_position_t>> min_starts(lines.size(), vector<hb_position_t>(max_line_length, 0));

  if(mode == MODE_ALL_ALIGNED || mode == MODE_NEARBY_ALIGNED) {
    int loops = 0;
    bool changed = true;
    while(changed) {
      changed = false; loops++;
      for (unsigned int ix = 0; ix < lines.size(); ix++) {
        auto &line = shaping_cache[ix];
        hb_position_t start = 0; // start of cell
        hb_position_t width_accum = 0; // accumulator for cell minimum width

        for (size_t j = 0; j < max_line_length; j++) {
          // at start of cell, update with min_start
          if(width_accum == 0) {
            // start is maximum of nearby lines
            for(size_t ix2 = max((size_t)0, (size_t)(((int)ix)-1)); ix2 < min(lines.size(), ix+(size_t)2); ix2++) {
              // for nearby, ignore beyond line length
              if (mode == MODE_ALL_ALIGNED || j < lines[ix2].size())
                start = max(min_starts[ix2][j], start);
            }
          }
          // update min_start if it doesn't fit
          if(start > min_starts[ix][j]) {
            min_starts[ix][j] = start;
            changed = true;
          }
          // accumulate character widths
          if(j < line.size()) {
            width_accum += line[j].position.x_advance;
            if(line[j].is_space) {
              start = start + width_accum;
              width_accum = 0;
            }
          } else if(j == line.size()) {
            // close cell just like it's a zero-width space
            start = start + width_accum;
            width_accum = 0;
          }
        }
      }
    }
    printf("layout loops mode %i: %i", mode, loops);
  }

  // layout complete, calculate final coordinates
  hb_position_t x = 0, y = 0;
  for (unsigned int ix = 0; ix < lines.size(); ix++) {
    auto &line = shaping_cache[ix];
    for (size_t j = 0; j < line.size(); j++) {
      line[j].final_position_x = x + line[j].position.x_offset;
      line[j].final_position_y = y - line[j].position.y_offset;
      x += line[j].position.x_advance;
      if((mode == MODE_ALL_ALIGNED || mode == MODE_NEARBY_ALIGNED) && line[j].is_space && j + 1 < max_line_length) {
        x = min_starts[ix][j+1];
      }
      y -= line[j].position.y_advance;
    }
    x = 0;
    y = 0;
  }

  return shaping_cache;
}

void Render(const Shader &shader, const vector<string> &lines,
            const FaceCollection &faces, ShapingCache *shaping_cache,
            const vector<TextureAtlas *> &texture_atlases, const State &state,
            GLuint VAO, GLuint VBO, size_t mono_index) {
  // Set background color
  glClearColor(BACKGROUND_COLOR);
  glClear(GL_COLOR_BUFFER_BIT);

  // Calculate how many lines to display
  unsigned int start_line = state.GetStartLine(), last_line;
  if (state.GetVisibleLines() > lines.size()) {
    last_line = start_line + lines.size();
  } else {
    last_line = start_line + state.GetVisibleLines();
  }

  // For each visible line
  for (unsigned int ix = start_line; ix < last_line; ix++) {
    vector<RenderedGlyph> line = shaping_cache->at(ix);
    auto x = 0;
    auto y = state.GetHeight() -
             (state.GetLineHeight() * ((ix - state.GetStartLine()) + 1));
    // Render the line to the screen, given the faces, the glyph_ids, the
    // glyph_id's textures and a texture atlas
    {
      glBindVertexArray(VAO);

      size_t glyph_ids_in_line = line.size();
      // Get characters for all glyph_ids
      vector<Character> characters;
      for (size_t i = 0; i < glyph_ids_in_line; ++i) {
        size_t face = line[i].face;
        hb_glyph_id_t glyph_id = (line[i]).glyph_id;

        Character *ch = texture_atlases[face]->Get(glyph_id);
        if (ch != nullptr) {
          characters.push_back(*ch);
          continue;
        }

        // If I have got space
        if ((texture_atlases[face]->Contains_stale() ||
              !texture_atlases[face]->IsFull())) {
          FT_Face ft_face = faces[face].face;

          // Get its texture's coordinates and offset from the atlas
          auto p = RenderGlyph(ft_face, glyph_id);
          texture_atlases[face]->Insert(glyph_id, &p);
          characters.push_back(p.first);
        } else {
          break;
        }
      }

      // TODO(andrea): instead of allocating on each line, allocate externally
      // and eventually resize here
      vector<array<array<GLfloat, 4>, 6>> quads;
      quads.resize(characters.size());
      vector<array<GLuint, 2>> texture_ids;
      texture_ids.resize(characters.size() * 6);

      for (size_t k = 0; k < characters.size(); ++k) {
        Character &ch = characters[k];
        // Calculate the character position. Emojis are scaled.
        GLfloat ratio_x, ratio_y;
        if (ch.colored) {
          ratio_x = static_cast<GLfloat>(kFontPixelWidth) /
                          static_cast<GLfloat>(ch.size.x);
          ratio_y = static_cast<GLfloat>(kFontPixelHeight) /
                          static_cast<GLfloat>(ch.size.y);
        } else {
          ratio_x = ratio_y = 1.0; // since a * 1.0 == a
        }

        GLfloat w = ch.size.x * ratio_x;
        GLfloat h = ch.size.y * ratio_y;
        GLfloat xpos = x + (line[k].final_position_x / 64) + ch.bearing.x * ratio_x;
        GLfloat ypos = y + (line[k].final_position_y / 64) - (ch.size.y - ch.bearing.y) * ratio_y;

        auto tc = ch.texture_coordinates;

        // FreeTypes uses a different coordinate convention so we need to
        // render the quad flipped horizontally, that's why where we should
        // have 0 we have tc.y and vice versa
        array<array<GLfloat, 4>, 6> quad = {{ // a
                                              // |
                                              // |
                                              // |
                                              // c--------b
                                              {xpos, ypos, 0, tc.y},
                                              {xpos, ypos + h, 0, 0},
                                              {xpos + w, ypos, tc.x, tc.y},
                                              // d--------f
                                              // |
                                              // |
                                              // |
                                              // e
                                              {xpos, ypos + h, 0, 0},
                                              {xpos + w, ypos, tc.x, tc.y},
                                              {xpos + w, ypos + h, tc.x, 0}}};
        array<GLuint, 2> texture_id = {
            static_cast<GLuint>(ch.texture_array_index),
            static_cast<GLuint>(ch.colored)};

        quads[k] = quad;
        texture_ids[k * 6 + 0] = texture_ids[k * 6 + 1] =
            texture_ids[k * 6 + 2] = texture_ids[k * 6 + 3] =
                texture_ids[k * 6 + 4] = texture_ids[k * 6 + 5] = texture_id;
      }

      assert(6 * quads.size() == texture_ids.size());

      // Set the shader's uniforms
      glm::vec4 fg_color(FOREGROUND_COLOR);
      glUniform4fv(glGetUniformLocation(shader.programId, "fg_color_sRGB"), 1,
                    glm::value_ptr(fg_color));

      // Bind the texture to the active texture unit
      for (size_t j = 0; j < texture_atlases.size(); j++) {
        glActiveTexture(GL_TEXTURE0 + j);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture_atlases[j]->GetTexture());
      }

      glBindBuffer(GL_ARRAY_BUFFER, VBO);
      {
        // Allocate memory
        GLsizeiptr total_size =
            quads.size() * (sizeof(quads[0]) + 6 * sizeof(texture_ids[mono_index]));
        glBufferData(GL_ARRAY_BUFFER, total_size, nullptr, GL_STREAM_DRAW);

        // Load quads
        GLintptr offset = 0;
        GLsizeiptr quads_byte_size = quads.size() * (sizeof(quads[0]));
        glBufferSubData(GL_ARRAY_BUFFER, offset, quads_byte_size,
                        quads.data());

        // Load texture_ids
        offset = quads_byte_size;
        GLsizeiptr texture_ids_byte_size =
            texture_ids.size() * (sizeof(texture_ids[mono_index]));
        glBufferSubData(GL_ARRAY_BUFFER, offset, texture_ids_byte_size,
                        texture_ids.data());

        // Tell shader that layout=0 is a vec4 starting at offset 0
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                              nullptr);
        glEnableVertexAttribArray(0);

        // Tell shader that layout=1 is an ivec2 starting after
        // quads_byte_size
        glVertexAttribIPointer(1, 2, GL_UNSIGNED_INT, 2 * sizeof(GLuint),
                                reinterpret_cast<const GLvoid *>(offset));
        glEnableVertexAttribArray(1);
      }
      glBindBuffer(GL_ARRAY_BUFFER, 0);

      // Render quads
      glDrawArrays(GL_TRIANGLES, 0,
                    characters.size() * sizeof(characters[0]));

      glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

      for (size_t j = 0; j < texture_atlases.size(); j++) {
        texture_atlases[j]->Invalidate();
      }
      glBindVertexArray(0);
    }
  }
}

pair<Character, bitmap_buffer_t> RenderGlyph(FT_Face face,
                                                   hb_glyph_id_t glyph_id) {
  FT_Int32 flags = FT_LOAD_DEFAULT | FT_LOAD_TARGET_LCD;

  if (FT_HAS_COLOR(face)) {
    flags |= FT_LOAD_COLOR;
  }

  if (FT_Load_Glyph(face, glyph_id, flags)) {
    fprintf(stderr, "Could not load glyph with glyph_id: %u\n", glyph_id);
    exit(EXIT_FAILURE);
  }

  if (!FT_HAS_COLOR(face)) {
    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_LCD)) {
      fprintf(stderr, "Could not render glyph with glyph_id: %u\n", glyph_id);
      exit(EXIT_FAILURE);
    }
  }

  bitmap_buffer_t bitmap_buffer;
  if (!FT_HAS_COLOR(face)) {
    // face->glyph->bitmap.buffer is a rows * pitch matrix but we need a
    // matrix which is rows * width. For each row i, buffer[i][pitch] is
    // just a padding byte, therefore we can ignore it

    bitmap_buffer.resize(face->glyph->bitmap.rows * face->glyph->bitmap.width *
                         3);
    for (uint i = 0; i < face->glyph->bitmap.rows; i++) {
      for (uint j = 0; j < face->glyph->bitmap.width; j++) {
        unsigned char ch =
            face->glyph->bitmap.buffer[i * face->glyph->bitmap.pitch + j];
        bitmap_buffer[i * face->glyph->bitmap.width + j] = ch;
      }
    }
  } else {
    bitmap_buffer.resize(face->glyph->bitmap.rows * face->glyph->bitmap.width *
                         4);
    copy(face->glyph->bitmap.buffer,
         face->glyph->bitmap.buffer +
             face->glyph->bitmap.rows * face->glyph->bitmap.width * 4,
         bitmap_buffer.begin());
  }

  GLsizei texture_width;
  GLsizei texture_height;
  if (FT_HAS_COLOR(face)) {
    texture_width = face->glyph->bitmap.width;
    texture_height = face->glyph->bitmap.rows;
  } else {
    // If the glyph is not colored then it is subpixel antialiased so the
    // texture will have 3x the width
    texture_width = face->glyph->bitmap.width / 3;
    texture_height = face->glyph->bitmap.rows;
  }

  Character ch;
  ch.size = glm::ivec2(texture_width, texture_height);
  ch.bearing = glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top);
  ch.advance = static_cast<GLuint>(face->glyph->advance.x);
  ch.colored = static_cast<bool> FT_HAS_COLOR(face);

  return make_pair(ch, bitmap_buffer);
}

}  // namespace renderer
