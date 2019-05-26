#version 330

in vec2 ex_texCoords;

uniform sampler2D glyph_texture;
uniform vec4 fg_color_sRGB;

// Dual source blending
// https://www.khronos.org/opengl/wiki/Blending#Dual_Source_Blending
// https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_blend_func_extended.txt
// Since OpenGL 3.3
layout(location = 0, index = 0) out vec4 color;
layout(location = 0, index = 1) out vec4 colorMask;

// https://stackoverflow.com/questions/48491340/use-rgb-texture-as-alpha-values-subpixel-font-rendering-in-opengl
// TODO: understand WHY it works, and if this is an actual solution, then write a blog post
void main()
{
    vec4 alpha_map = texture(glyph_texture, ex_texCoords);

    color = alpha_map;

    colorMask = fg_color_sRGB.a*alpha_map;
}
