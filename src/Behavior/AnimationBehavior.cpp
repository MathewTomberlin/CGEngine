#include "AnimationBehavior.h"
#include "../Types/DataMap.h"
#include "../World/WorldInstance.h"

namespace CGEngine {
    AnimationBehavior::AnimationBehavior(Body* owner, string textureName, Vector2i spriteSize) : Behavior(owner) {
        Texture* spriteTex = textures->get(textureName);
        IntRect spriteRect = IntRect({ 0,0 }, spriteSize);
        input = DataMap(map<string, any>({
                    {"maxFrameRate",15.0f},
                    {"speed",1.0f},
                    {"maxFrame",7},
                    {"looping",true},
                    {"startRunning",false},
                    {"sheetSize",{spriteTex->getSize()}}
            }));
        data = DataMap(map<string, any>({
            {"rect",spriteRect},
            {"frame",0},
            {"state",AnimationState::Ready},
            {"frameTime",0.f},
            {"animationStartPos",Vector2i({0,0})}
            }));
        scripts.addScript(onUpdateEvent, new Script(animBehaviorUpdateEvt));
        scripts.addScript("startAnimation", new Script(startAnimEvt));
        scripts.addScript("pauseAnimation", new Script(pauseAnimEvt));
        scripts.addScript("endAnimation", new Script(endAnimEvt));
        scripts.addScript("calculateAnimLength", new Script(calculateFrameLengthEvt));
        scripts.addScript("animate", new Script(animateEvt));
        scripts.addScript(onStartEvent, new Script(animBehaviorStartEvt));
        scripts.addScript("OnTranslate", new Script(onTranslateEvt));
    }

    //When the Behavior is started (when the Body is started)
    ScriptEvent AnimationBehavior::animBehaviorStartEvt = [](ScArgs args) {
        IntRect spriteRect = args.behavior->data.getData<IntRect>("rect");
        bool startRunning = args.behavior->input.getData<bool>("startRunning");

        args.caller->get<Sprite*>()->setTextureRect(spriteRect);
        if (startRunning) {
            args.behavior->scripts.callDomain("startAnimation", args.behavior);
        }
    };

    //Animation scripts
    ScriptEvent AnimationBehavior::calculateFrameLengthEvt = [](ScArgs args) {
        float maxFrameRate = args.behavior->input.getData<float>("maxFrameRate");
        float speed = args.behavior->input.getData<float>("speed");
        float frameRate = maxFrameRate * abs(speed);
        float frameLength = 1.f / frameRate;
        args.behavior->data.addData("frameLength", frameLength);
    };

    ScriptEvent AnimationBehavior::pauseAnimEvt = [](ScArgs args) {
        id_t animationUpdateScriptId = args.behavior->data.getData<id_t>("animUpdateId");
        args.behavior->data.addData("state", AnimationState::Paused);
        args.behavior->scripts.removeScript(onUpdateEvent, animationUpdateScriptId, true);
    };

    ScriptEvent AnimationBehavior::endAnimEvt = [](ScArgs args) {
        args.behavior->data.addData("frameTime", 0.f);

        Vector2i startingPos = args.behavior->data.getData<Vector2i>("animationStartPos");
        IntRect spriteRect = args.behavior->data.getData<IntRect>("rect");
        spriteRect.position = startingPos;
        args.behavior->data.addData("rect", spriteRect);
        args.caller->get<Sprite*>()->setTextureRect(spriteRect);

        args.behavior->scripts.callDomain("pauseAnimation", args.behavior);
        args.behavior->data.addData("state", AnimationState::Ready);

        args.behavior->data.addData("frame", 0);
    };

    ScriptEvent AnimationBehavior::startAnimEvt = [](ScArgs args) {
        args.behavior->scripts.callDomain("calculateAnimLength", args.behavior);
        args.behavior->data.addData("state", AnimationState::Running);

        id_t animationUpdateScriptId = args.behavior->scripts.addScript(onUpdateEvent, new Script([](ScArgs args) {
            args.behavior->scripts.callDomain("animate", args.behavior);
            }));
        args.behavior->data.addData("animUpdateId", animationUpdateScriptId);
    };

    ScriptEvent AnimationBehavior::animateEvt = [](ScArgs args) {
        int frame = args.behavior->data.getData<int>("frame");
        IntRect spriteRect = args.behavior->data.getData<IntRect>("rect");
        float frameLength = args.behavior->data.getData<float>("frameLength");
        float state = args.behavior->data.getData<AnimationState>("state");

        bool looping = args.behavior->input.getData<bool>("looping");
        bool startRunning = args.behavior->input.getData<bool>("startRunning");
        Vector2u spriteSheetSize = args.behavior->input.getData<Vector2u>("sheetSize");
        Vector2i animationStartPos = args.behavior->data.getData<Vector2i>("animationStartPos");
        int maxFrame = args.behavior->input.getData<int>("maxFrame");

        if (frameLength <= 0) return;
        float frameTime = args.behavior->data.getData<float>("frameTime") + time.getDeltaSec();
        if (frameTime > frameLength) {
            while ((frameTime -= frameLength) >= 0.f) {
                if (maxFrame > 0 && frame >= maxFrame) {
                    if (looping) {
                        frame = 0;
                        spriteRect.position = animationStartPos;
                    }
                    else {
                        args.behavior->scripts.callDomain("pauseAnimation", args.behavior);
                        break;
                    }
                }

                //Advance frame rect
                if (maxFrame < 0 || frame <= (unsigned)maxFrame) {
                    if ((unsigned)(spriteRect.position.x + spriteRect.size.x) < spriteSheetSize.x) {
                        spriteRect.position.x += spriteRect.size.x;
                        frame++;
                    }
                    else {
                        if ((unsigned)(spriteRect.position.y + spriteRect.size.y) < spriteSheetSize.y) {
                            spriteRect.position.y += spriteRect.size.y;
                            spriteRect.position.x = 0;
                            frame++;
                        }
                        else {
                            if (looping) {
                                spriteRect.position = animationStartPos;
                                frame++;
                            }
                            else {
                                args.behavior->scripts.callDomain("pauseAnimation", args.behavior);
                                break;
                            }
                        }
                    }
                }
            }
            args.caller->get<Sprite*>()->setTextureRect(spriteRect);
            args.behavior->data.addData("frame", frame);
            args.behavior->data.addData("rect", spriteRect);
        }
        args.behavior->data.addData("frameTime", frameTime);
    };

    ScriptEvent AnimationBehavior::animBehaviorUpdateEvt = [](ScArgs args) {
        Vector2f facing = args.behavior->data.getData<Vector2f>("facing");
        Vector2f lastMove = args.behavior->data.getData<Vector2f>("lastMove");

        if (facing != lastMove && lastMove != V2f({ 0,0 })) {
            if (lastMove == Vector2f({ 0, 1 })) {
                args.behavior->data.addData("animationStartPos", Vector2i({ 0,0 }));
            }
            else if (lastMove == Vector2f({ 0, -1 })) {
                args.behavior->data.addData("animationStartPos", Vector2i({ 0,96 }));
            }
            else if (lastMove == Vector2f({ 1, 0 })) {
                args.behavior->data.addData("animationStartPos", Vector2i({ 0,32 }));
            }
            else {
                args.behavior->data.addData("animationStartPos", Vector2i({ 0,64 }));
            }

            facing = lastMove;
            args.behavior->data.addData("facing", facing);
            args.behavior->scripts.callDomain("endAnimation", args.behavior);
        }
        lastMove = { 0,0 };
        args.behavior->data.addData("lastMove", lastMove);
    };

    ScriptEvent AnimationBehavior::onTranslateEvt = [](ScArgs args) {
        AnimationState animState = args.behavior->data.getData<AnimationState>("state");
        if (animState != AnimationState::Running) {
            args.behavior->scripts.callDomain("startAnimation", args.behavior);
        }
        Vector2f lastMove = args.script->getInput().getData<Vector2f>("evt");
        args.behavior->data.addData("lastMove", lastMove);
    };
}