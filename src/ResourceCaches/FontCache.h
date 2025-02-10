#pragma once

#include "SFML/Graphics.hpp"
#include <map>
using namespace sf;
using namespace std;

namespace CGEngine {
    class FontCache {
    public:
        FontCache();
        Font* createFont(string fontName, string fontPath);
        Font* getFont(string fontName);
        void setDefaultFont(Font* name);
        Font* getDefaultFont();
    protected:
        map<string, Font*> fonts;
        Font* defaultFont = nullptr;
    };
}