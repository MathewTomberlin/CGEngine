#include "ButtonBody.h"
#include "../World/WorldInstance.h"

namespace CGEngine {
    ButtonBody::ButtonBody() : BinaryStateBody() {
        buttonPressEventId = 0U;
        buttonReleaseEventId = 0U;
        updateEventId = 0U;
    }

    ButtonBody::ButtonBody(Transformable* b, Transformation handle, Body* p, Vector2f align, ScriptEvent clickedEvt, RectProperties normalProps, RectProperties clickProps) : BinaryStateBody(b, handle, p, align) {
        buttonPressEventId = 0U;
        buttonReleaseEventId = 0U;
        updateEventId = 0U;

        normalProperties = normalProps;
        clickProperties = clickProps;
        updateRect(normalProperties);

        buttonPressEventId = addOverlapMousePressScript(new Script(setClicked));
        buttonReleaseEventId = addOverlapMouseReleaseScript(new Script(unsetClicked));
        updateEventId = addUpdateScript(new Script(clickStyle));
        if (clickedEvt != nullptr) {
            setOnClickEvent(clickedEvt);
        }
    };

    void ButtonBody::updateRect(RectProperties props) {
        update([&](Shape* rect) {
            rect->setFillColor(props.fillColor);
            rect->setOutlineColor(props.outlineColor);
            rect->setOutlineThickness(props.outlineWidth);
            });
    }

    void ButtonBody::setOnClickEvent(ScriptEvent clickedEvt) {
        if (onClickEventId.has_value()) {
            eraseScript("mouseRelease_left", onClickEventId.value());
        }
        onClickEventId = addOverlapMouseReleaseScript(new Script(clickedEvt), Mouse::Button::Left, false);
    }
}