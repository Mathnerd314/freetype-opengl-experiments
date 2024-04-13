// Copyright 2019 <Andrea Cognolato>
// TODO(andrea): subpixel positioning (si puo' fare con freetype? lo fa gia?)
// TODO(andrea): statically link glfw, harfbuzz; making sure that both are
// TODO(andrea): benchmark startup time (500ms on emoji file, wtf)
// compiled in release mode

// 👚🔇🐕🏠 📗🍢💵📏🐁🌓 💼🐦👠
// 👚🔇🐕🏠 📗🍢💵📏🐁🌓 💼🐦👠

// glad - OpenGL loader
#include <glad/glad.h>

// fontconfig
#include <fontconfig/fontconfig.h>

// FreeType
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H
#include <freetype/ftadvanc.h>
#include <freetype/ftsnames.h>
#include <freetype/tttables.h>

#include <GLFW/glfw3.h>

// HarfBuzz
#include <harfbuzz/hb.h>
// HarfBuzz FreeType
#include <harfbuzz/hb-ft.h>

#include <fstream>
#include <unordered_map>
#include <vector>

// glm - OpenGL mathematics
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>

#include "./callbacks.h"
#include "./constants.h"
#include "./renderer.h"
#include "./shader.h"
#include "./state.h"
#include "./texture_atlas.h"
#include "./util.h"
#include "./window.h"

namespace lettera {
using face_collection::FaceCollection;
using face_collection::LoadFaces;
using renderer::Render;
using renderer::ShapingCache;
using state::State;
using std::get;
using std::make_pair;
using std::make_tuple;
using std::pair;
using std::string;
using std::tuple;
using std::unordered_map;
using std::vector;
using texture_atlas::Character;
using texture_atlas::TextureAtlas;
using window::Window;

GLuint VAO, VBO;

void InitOpenGL() {
  // Check that glad worked
  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
    fprintf(stderr, "glad failed to load OpenGL loader\n");
    exit(EXIT_FAILURE);
  }

  // Enable debug output
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(util::GLDebugMessageCallback, nullptr);
}

int main(int argc UNUSED, char **argv) {
  Window window(kInitialWindowWidth, kInitialWindowHeight, kWindowTitle,
                callbacks::KeyCallback, callbacks::ScrollCallback,
                callbacks::ResizeCallback);
  State state(kInitialWindowWidth, kInitialWindowHeight, kLineHeight,
              kInitialLine);

  InitOpenGL();

  // Compile and link the shaders
  Shader shader("src/shaders/text.vert", "src/shaders/text.frag");
  shader.use();

  callbacks::glfw_user_pointer_t glfw_user_pointer;
  glfwSetWindowUserPointer(window.window, &glfw_user_pointer);

  glfw_user_pointer.shader_program_id = shader.programId;
  glfw_user_pointer.state = &state;

  // https://stackoverflow.com/questions/48491340/use-rgb-texture-as-alpha-values-subpixel-font-rendering-in-opengl
  // TODO(andrea): understand WHY it works, and if this is an actual solution,
  // then write a blog post
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR);

  // Set the viewport
  glViewport(0, 0, kInitialWindowWidth, kInitialWindowHeight);

  // Disable byte-alignment restriction (our textures' size is not a multiple
  // of 4)
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  // Init Vertex Array Object (VAO)
  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);
  // Init Vertex Buffer Object (VBO)
  glGenBuffers(1, &VBO);
  glBindVertexArray(0);

  // Init projection matrix
  glm::mat4 projection =
      glm::ortho(0.0f, static_cast<GLfloat>(kInitialWindowWidth), 0.0f,
                 static_cast<GLfloat>(kInitialWindowHeight));
  glUniformMatrix4fv(glGetUniformLocation(shader.programId, "projection"), 1,
                     GL_FALSE, glm::value_ptr(projection));

  // Initialize FreeType
  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    fprintf(stderr, "Could not load freetype\n");
    exit(EXIT_FAILURE);
  }

  // Set which filter to use for the LCD Subpixel Antialiasing
  FT_Library_SetLcdFilter(ft, FT_LCD_FILTER_DEFAULT);

  /* Load our fonts */
  // Init font config
  FcInit(); //initializes Fontconfig
  FcConfig* config = FcInitLoadConfigAndFonts();
  assert(config);

  FcPattern* pat = FcNameParse((const FcChar8*)"Noto Sans");

  // Increase the possible matches
  FcConfigSubstitute(config, pat, FcMatchPattern);
  FcDefaultSubstitute(pat);

  FcResult result;

  FcPattern* font = FcFontMatch(config, pat, &result);
  assert(font);

  /* Will be freed with font */
  FcChar8* font_file = NULL;

  if(FcPatternGetString(font, FC_FILE, 0, &font_file) != FcResultMatch) {
    fprintf(stderr, "Could not find font\n");
    exit(EXIT_FAILURE);
  }
  printf("Found font: %s\n", (char*) font_file);

  // TODO(andrea): make this support multiple fonts with font fallback
  vector<string> face_names{string((char*)font_file),
                            "./assets/fonts/NotoColorEmoji.ttf",
                            "./assets/fonts/FiraCode-Retina.ttf"};
  FaceCollection faces = LoadFaces(ft, face_names);

  /* Cleanup font config */
  FcPatternDestroy(font);
  FcPatternDestroy(pat);
  FcConfigDestroy(config);
  FcFini();

  // Load the texture atlases
  TextureAtlas monochrome_texture_atlas(
      faces[0].width, faces[0].height, shader.programId,
      "monochromatic_texture_array", GL_RGB8, GL_RGB, 0);
  TextureAtlas colored_texture_atlas(faces[1].width, faces[1].height,
                                     shader.programId, "colored_texture_array",
                                     GL_RGBA8, GL_BGRA, 1);
  TextureAtlas monochrome_texture_atlas_2(
      faces[2].width, faces[2].height, shader.programId,
      "monochromatic_texture_array_2", GL_RGB8, GL_RGB, 2);
  vector<TextureAtlas *> texture_atlases;
  texture_atlases.push_back(&monochrome_texture_atlas);
  texture_atlases.push_back(&colored_texture_atlas);
  texture_atlases.push_back(&monochrome_texture_atlas_2);

  // Read the file
  vector<string> lines;
  {
    std::ifstream file(argv[1]);
    std::string line;
    while (std::getline(file, line)) {
      lines.push_back(line);
    }
    assert(lines.size() > 0);
  }
  glfw_user_pointer.state->lines = &lines;

  // TODO(andrea): invalidation and capacity logic (LRU?, Better Hashmap?)
  // Init Shaping caches
  ShapingCache shaping_cache_proportional = renderer::LayoutText(lines, faces, 0, MODE_PROPORTIONAL);
  ShapingCache shaping_cache_mono = renderer::LayoutText(lines, faces, 2, MODE_MONOSPACE);
  ShapingCache shaping_cache_all = renderer::LayoutText(lines, faces, 0, MODE_ALL_ALIGNED);
  ShapingCache shaping_cache_nearby = renderer::LayoutText(lines, faces, 0, MODE_NEARBY_ALIGNED);

  while (!glfwWindowShouldClose(window.window)) {
    glfwWaitEvents();

    auto t1 = glfwGetTime();

    ShapingCache* shaping_cache;
    switch(state.mode) {
      case MODE_MONOSPACE: shaping_cache = &shaping_cache_mono; break;
      case MODE_ALL_ALIGNED: shaping_cache = &shaping_cache_all; break;
      case MODE_NEARBY_ALIGNED: shaping_cache = &shaping_cache_nearby; break;
      case MODE_PROPORTIONAL:
      default:
       shaping_cache = &shaping_cache_proportional; break;
    }

    Render(shader, lines, faces, shaping_cache, texture_atlases, state, VAO, VBO, state.mode == MODE_MONOSPACE ? 2 : 0);

    auto t2 = glfwGetTime();
    printf("Rendering lines took %f ms (%3.0f fps/Hz)\n", (t2 - t1) * 1000, 1.f / (t2 - t1));

    // Swap buffers when drawing is finished
    glfwSwapBuffers(window.window);
  }

  for (auto &face : faces) {
    FT_Done_Face(face.face);
  }

  FT_Done_FreeType(ft);

  return 0;
}
}  // namespace lettera

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage %s FILE\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  lettera::main(argc, argv);

  return 0;
}
