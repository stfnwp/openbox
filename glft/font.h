#ifndef __glft_font_h
#define __glft_font_h

#include <fontconfig/fontconfig.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <fontconfig/fcfreetype.h>

#include <glib.h>

struct GlftFont {
    FcPattern *pat;
    FcCharSet *chars;

    char *filename;
    int index;

    FT_Face face;
    FT_Int ftflags;
    FT_F26Dot6 ftcharsize;

    FcBool antialias;
    int spacing;
    FcBool minspace;
    int char_width;
    /* extended font attributes */
    FcBool shadow;
    int shadow_offset;
    float shadow_alpha;

    GHashTable *glyph_map;

    /* public shit */
    int ascent;
    int descent;
    int height;
    int max_advance_width;
};

struct GlftGlyph {
    /* The character in UCS-4 encoding */
    FcChar32 w;
    /* OpenGL display list for the character */
    unsigned int dlist;

    int width;
    int height;
};

/*! Takes a character in UTF-8 encoding and returns an OpenGL display list
 for it */
struct GlftGlyph *GlftFontGlyph(struct GlftFont *font, const char *c);

#endif
