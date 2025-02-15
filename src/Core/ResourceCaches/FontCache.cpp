#include "FontCache.h"
#include <filesystem>
#include <iostream>

namespace CGEngine {
    FontCache::FontCache() {
        fonts = map<string, Font*>();
        setDefaultFont(createFont("default", "fonts/defaultFont.ttf"));
    }

    Font* FontCache::createFont(string fontName, string fontPath) {
        filesystem::path path(fontPath);
        if (filesystem::exists(path)) {
            Font* new_font = new Font(fontPath);
            fonts[fontName] = new_font;
            return new_font;
        }
        return nullptr;
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