
#include "main.h"
extern Display *display; // the connection to the X server
extern Window root; // the root window top level window all other windows are
                    // children of it and covers all the screen
extern Window focused_window, master_window, bar_window;
extern XftFont *xft_font;
extern XftDraw *xft_draw;
extern XftColor xft_color;
extern std::vector<Client> *clients;
std::unordered_map<FcChar32, XftFont *> font_cache;

// Function to render text, choosing appropriate font dynamically with caching
void draw_text_with_dynamic_font(Display *display, Window window, XftDraw *draw,
                                 XftColor *color, const std::string &text,
                                 int x, int y, int screen) {
  int x_offset = x;

  for (size_t i = 0; i < text.size();) {
    // Decode the UTF-8 character
    FcChar32 ucs4;
    int bytes = FcUtf8ToUcs4((FcChar8 *)(&text[i]), &ucs4, text.size() - i);
    if (bytes <= 0)
      break;

    // Select and cache the appropriate font for this character
    XftFont *font = select_font_for_char(display, ucs4, screen);
    if (!font) {
      std::cerr << "Failed to load font for character: " << ucs4 << std::endl;
      break;
    }

    // Render the character using the selected (or cached) font
    XftDrawStringUtf8(draw, color, font, x_offset, y, (FcChar8 *)&text[i],
                      bytes);
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font, (FcChar8 *)&text[i], bytes, &extents);
    x_offset += extents.xOff;

    // Move to the next character
    i += bytes;
  }
}

int get_utf8_string_width(Display *display, XftFont *font,
                          const std::string &text) {
  // Convert std::string to XftChar8 array

  XGlyphInfo extents;
  XftChar8 *utf8_string =
      reinterpret_cast<XftChar8 *>(const_cast<char *>(text.c_str()));

  // Measure the width of the UTF-8 string
  XftTextExtentsUtf8(display, font, utf8_string, text.length(), &extents);

  return extents.width;
}
//
// Function to select and cache the appropriate font for a character
XftFont *select_font_for_char(Display *display, FcChar32 ucs4, int screen) {
  // Check if the font for this character is already cached
  if (font_cache.find(ucs4) != font_cache.end()) {
    return font_cache[ucs4];
  }

  // If not cached, use Fontconfig to find the appropriate font
  FcPattern *pattern = FcPatternCreate();
  FcCharSet *charset = FcCharSetCreate();
  FcCharSetAddChar(charset, ucs4);
  FcPatternAddCharSet(pattern, FC_CHARSET, charset);
  FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);

  FcResult result;
  FcPattern *match = FcFontMatch(nullptr, pattern, &result);
  XftFont *font = nullptr;

  if (match) {
    char *font_name = nullptr;
    FcPatternGetString(match, FC_FAMILY, 0, (FcChar8 **)&font_name);
    if (font_name) {
      font = XftFontOpenName(display, screen, font_name);
    }
    FcPatternDestroy(match);
  }

  FcPatternDestroy(pattern);
  FcCharSetDestroy(charset);

  // Fallback to default font if no match was found
  if (!font) {
    font = XftFontOpenName(display, screen, "fixed");
  }

  // Cache the font for this character
  font_cache[ucs4] = font;

  return font;
}
