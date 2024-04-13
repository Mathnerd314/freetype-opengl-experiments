// Copyright 2019 <Andrea Cognolato>
#include "./face_collection.h"

namespace face_collection {
FaceCollection LoadFaces(FT_Library ft, const vector<string> &face_names) {
  FaceCollection faces;

  for (auto &face_name : face_names) {
    FT_Face face;
    if (FT_New_Face(ft, face_name.c_str(), 0, &face)) {
      fprintf(stderr, "Could not load font\n");
      exit(EXIT_FAILURE);
    }

    // The active size must be set, otherwise scaling and hinting will not be performed.
    if (FT_HAS_COLOR(face)) {
      if (FT_Select_Size(face, 0)) {
        fprintf(stderr, "Could not request the font size (fixed)\n");
        exit(EXIT_FAILURE);
      }
    } else {
      if (FT_Error err = FT_Set_Pixel_Sizes(face, kFontPixelWidth, kFontPixelHeight)) {
        fprintf(stderr, "Could not request the font size (in pixels): %i\n", err);
        exit(EXIT_FAILURE);
      }
    }


    // The face's size and bbox are populated only after set pixel
    // sizes/select size have been called
    GLsizei width, height;
    if (FT_IS_SCALABLE(face)) {
      width = FT_MulFix(face->bbox.xMax - face->bbox.xMin,
                        face->size->metrics.x_scale) >>
              6;
      height = FT_MulFix(face->bbox.yMax - face->bbox.yMin,
                         face->size->metrics.y_scale) >>
               6;
    } else {
      width = (face->available_sizes[0].width);
      height = (face->available_sizes[0].height);
    }
    // Create a font using the face provided by freetype
    hb_font_t *hb_ft_font = hb_ft_font_create(face, nullptr);

    faces.push_back(SizedFace{face, hb_ft_font, width, height});
  }

  return faces;
}

}  // namespace face_collection
