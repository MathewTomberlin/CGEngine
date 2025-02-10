#include "CommonScripts.h"
#include "../World/WorldInstance.h"

namespace CGEngine {
    struct TranslateArgs {
        TranslateArgs(float spd = 0.f, Vector2f dir = {0,0}, bool view = false) :speed(spd), direction(dir), viewBound(view) {};
        float speed = 0.f;
        Vector2f direction = { 0,0 };
        bool viewBound = false;
    };

    struct RotateArgs {
        RotateArgs(float degPerSec = 0.f, bool view = false) :degreesPerSecond(degPerSec), viewBound(view) {};
        float degreesPerSecond = 0.f;
        bool viewBound = false;
    };

    ScriptEvent translateEvent = [](ScArgs args) {
        if (world->consoleInputEnabled) return;
        //Re-readable input
        args.script->setOutput(args.script->getInput());
        TranslateArgs evtArgs = args.script->pullOutOutput<TranslateArgs>();

        vector<Body*> hits = world->raycast(args.caller->getGlobalPosition() + (args.caller->getGlobalBounds().size / 2.f), evtArgs.direction, 1, (args.caller->getGlobalBounds().size / 2.f).x + evtArgs.speed * time.getDeltaSec());
        if (hits.size() <= 0) {
            V2f delta = evtArgs.direction * evtArgs.speed * time.getDeltaSec();
            args.caller->translate(delta, true);
            if (evtArgs.viewBound) {
                world->moveView(delta);
            }
        }
        args.caller->callScriptsWithData("OnTranslate", stack<any>({ evtArgs.direction }));
    };

    ScriptEvent rotateEvent = [](ScArgs args) {
        if (world->consoleInputEnabled) return;
        //Re-readable input
        args.script->setOutput(args.script->getInput());
        RotateArgs evtArgs = args.script->pullOutOutput<RotateArgs>();

            Angle delta = degrees(evtArgs.degreesPerSecond * time.getDeltaSec());
            args.caller->rotate(delta);
            if (evtArgs.viewBound) {
                world->rotateView(delta);
            }
            args.caller->callScriptsWithData("OnRotate", stack<any>({ evtArgs.degreesPerSecond }));
    };

	ScriptEvent keyboardMovementController = [](ScArgs args) {
            float moveSpeed = args.script->pullOutInput<float>();
            InputKeyMap horizonalKeyMap = args.script->pullOutOptionalInput<InputKeyMap>().value_or(InputKeyMap(Keyboard::Scan::D, Keyboard::Scan::A));
            InputKeyMap verticalKeyMap = args.script->pullOutInput<InputKeyMap>();

            Script* translateUpScript = new Script(translateEvent);
            translateUpScript->setInput(stack<any>({ TranslateArgs(moveSpeed, {0,-1}, true) }));
            args.caller->addKeyHoldScript(translateUpScript, verticalKeyMap.negative);

            Script* translateDownScript = new Script(translateEvent);
            translateDownScript->setInput(stack<any>({ TranslateArgs(moveSpeed, {0,1}, true) }));
            args.caller->addKeyHoldScript(translateDownScript, verticalKeyMap.positive);

            Script* translateLeftScript = new Script(translateEvent);
            translateLeftScript->setInput(stack<any>({ TranslateArgs(moveSpeed, {-1,0}, true) }));
            args.caller->addKeyHoldScript(translateLeftScript, horizonalKeyMap.negative);

            Script* translateRightScript = new Script(translateEvent);
            translateRightScript->setInput(stack<any>({ TranslateArgs(moveSpeed, {1,0}, true) }));
            args.caller->addKeyHoldScript(translateRightScript, horizonalKeyMap.positive);
	};

    ScriptEvent keyboardRotationController = [](ScArgs args) {
        float rotateSpeed = args.script->pullOutInput<float>();

        Script* translateRightScript = new Script(rotateEvent);
        translateRightScript->setInput(stack<any>({ RotateArgs(rotateSpeed, false) }));
        args.caller->addKeyHoldScript(translateRightScript, Keyboard::Scan::E);

        Script* translateLeftScript = new Script(rotateEvent);
        translateLeftScript->setInput(stack<any>({ RotateArgs(-rotateSpeed, false) }));
        args.caller->addKeyHoldScript(translateLeftScript, Keyboard::Scan::Q);
    };
}