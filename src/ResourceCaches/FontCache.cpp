#include "FontCache.h"

namespace CGEngine {
    FontCache::FontCache() {
        fonts = map<string, Font*>();
        setDefaultFont(createFont("default", "defaultFont.ttf"));
    }

    Font* FontCache::createFont(string fontName, string fontPath) {
        Font* new_font = new Font(fontPath);
        fonts[fontName] = new_font;
        return new_font;
    }

    Font* FontCache::getFont(string fontName) {
        return fonts[fontName];
    }

    void FontCache::setDefaultFont(Font* font) {
        if (defaultFont == nullptr) {
            defaultFont = font;
        }
    }

    Font* FontCache::getDefaultFont() {
        return defaultFont;
    }
}