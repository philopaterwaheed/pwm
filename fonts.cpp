
#include "config.h"
#include "main.h"
#include <X11/Xft/Xft.h>
#include <cstring>
extern Display *display; // the connection to the X server
XftFont *xft_font;
extern XftColor xft_color;
std::unordered_map<FcChar32, XftFont *> font_cache;
extern Monitor *current_monitor;
extern int screen;
Cursor cursors[3];

// Function to render text, choosing appropriate font dynamically with caching
void draw_text_with_dynamic_font(Display *display, Window window, XftDraw *draw,
                                 XftColor *color, XftChar8 *utf8_string, int x,
                                 int y, int screen, int monitor_width) {
  int x_offset = x;
  int str_len = std::strlen((char *)utf8_string);
  for (auto i = 0; i < str_len;) {
    // Decode the UTF-8 character
    FcChar32 ucs4;
    int bytes = FcUtf8ToUcs4((FcChar8 *)(&utf8_string[i]), &ucs4, str_len - i);
    if (bytes <= 0)
      break;
    XftFont *font;
    // Select and cache the appropriate font for this character
    if (xft_font && XftCharExists(display, xft_font, ucs4))
      font = xft_font;
    else
      font = select_font_for_char(display, ucs4, screen);
    // font = XftFontOpenName(display, screen, BAR_FONT.c_str());
    if (!font) {
      std::cerr << "Failed to load font for character: " << ucs4 << std::endl;
      break;
    }

    // Get text extents
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font, (FcChar8 *)&utf8_string[i], bytes,
                       &extents);

    // Ensure text does not overflow beyond the monitor width
    if (x_offset + extents.xOff > monitor_width ||x_offset + extents.xOff  < 0  ) {
      // Handle text overflow: here we just break
      break; // Or implement wrapping/new line logic
    }

    // Render the character
    XftDrawStringUtf8(current_monitor->xft_draw, color, font, x_offset, y,
                      (FcChar8 *)&utf8_string[i], bytes);
    x_offset += extents.xOff;

    // Move to the next character
    i += bytes;
  }

  // Cleanup: Free resources
}

int get_utf8_string_width(Display *display, XftChar8 *utf8_string) {
  int x_offset = 0;
  int str_len = std::strlen((char *)utf8_string);
  for (auto i = 0; i < str_len;) {
    // Decode the UTF-8 character
    FcChar32 ucs4;
    int bytes = FcUtf8ToUcs4((FcChar8 *)(&utf8_string[i]), &ucs4, str_len - i);
    if (bytes <= 0)
      break;
    XftFont *font;
    // Select and cache the appropriate font for this character
    if (xft_font && XftCharExists(display, xft_font, ucs4))
      font = xft_font;
    else
      font = select_font_for_char(display, ucs4, screen);
    // font = XftFontOpenName(display, screen, BAR_FONT.c_str());
    if (!font) {
      std::cerr << "Failed to load font for character: " << ucs4 << std::endl;
      break;
    }

    // Get text extents
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font, (FcChar8 *)&utf8_string[i], bytes,
                       &extents);

    x_offset += extents.xOff;

    // Move to the next character
    i += bytes;
  }
  return x_offset;
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
    std::string siz = ":size=";
    siz += std::to_string(SPACIL_CHAR_FONT_SIZE);
    FcPatternGetString(match, FC_FAMILY, 0, (FcChar8 **)&font_name);

    if (font_name) {
      siz.insert(0, font_name);

      font = XftFontOpenName(display, screen, siz.c_str());
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
Cursor cur_create(int shape) {
  Cursor cur = XCreateFontCursor(display, shape);
  return cur;
}

void cur_free(Cursor *cursor) {
  if (!cursor)
    return;

  XFreeCursor(display, *cursor);
  free(cursor);
}
void cur_free(Cursor *cursor);
