// Copyright 2019 <Andrea Cognolato>
#ifndef SRC_FACE_COLLECTION_H_
#define SRC_FACE_COLLECTION_H_

#include <glad/glad.h>

#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H

#include <cassert>
#include <string>
#include <tuple>
#include <vector>

#include "./constants.h"

namespace face_collection {
using std::get;
using std::make_tuple;
using std::string;
using std::tuple;
using std::vector;

struct SizedFace {
  FT_Face face;
  hb_font_t* hb_ft_font;
  GLsizei width, height;
};
using FaceCollection = vector<SizedFace>;

FaceCollection LoadFaces(FT_Library ft, const vector<string> &face_names);

}  // namespace face_collection

#endif  // SRC_FACE_COLLECTION_H_
