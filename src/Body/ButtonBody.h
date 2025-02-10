#pragma once

#include "../World/World.h"
#include "BinaryStateBody.h"

namespace CGEngine {
    class ButtonBody : public BinaryStateBody {
    public:
        ButtonBody();
        ButtonBody(Transformable* b, Transformation handle = Transformation(), Body* p = nullptr, Vector2f align = {0,0}, ScriptEvent clickedEvt = nullptr, RectProperties normalProps = RectProperties(Color(80, 80, 80), Color(220, 220, 220), 1.f), RectProperties clickProps = RectProperties(Color(40, 40, 40), Color(140, 140, 140), 1.f));

        void updateRect(RectProperties props);
        void setOnClickEvent(ScriptEvent clickedEvt);

        RectProperties normalProperties = RectProperties(Color(80, 80, 80), Color(220, 220, 220), 1.f);
        RectProperties clickProperties = RectProperties(Color(40, 40, 40), Color(140, 140, 140), 1.f);

        optional<id_t> buttonPressEventId = 0U;
        optional<id_t> buttonReleaseEventId = 0U;
        id_t updateEventId = 0U;
    private:
        optional<id_t> onClickEventId;
        ScriptEvent setClicked = [](ScArgs args) {
            BinaryStateBody* t = (BinaryStateBody*)args.caller;
            t->state = true;
            t->dirty = true;
            };
        ScriptEvent unsetClicked = [](ScArgs args) {
            BinaryStateBody* t = (BinaryStateBody*)args.caller;
            t->state = false;
            t->dirty = true;
            };
        ScriptEvent clickStyle = [this](ScArgs args) {
            BinaryStateBody* t = (BinaryStateBody*)args.caller;
            if (t->dirty) {
                ButtonBody* btn = (ButtonBody*)args.caller;
                btn->updateRect((t->state) ? clickProperties : normalProperties);
                t->dirty = false;
            }
            };
    };
}