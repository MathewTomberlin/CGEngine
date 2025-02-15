#include "AnimationBehavior.h"
#include "../../Core/Types/DataMap.h"
#include "../../Core/Engine/Engine.h"

namespace CGEngine {
    AnimationBehavior::AnimationBehavior(Body* owner, AnimationParameters params) : Behavior(owner) {
        setParameters(params);
        resetProcessData();

        addScriptEventsByDomain(map<string, ScriptEvent>({
            {onUpdateEvent, animBehaviorUpdateEvt},
            {"startAnimation", startAnimEvt},
            {"pauseAnimation", pauseAnimEvt},
            {"endAnimation", endAnimEvt},
            {"calculateAnimLength", calculateFrameLengthEvt},
            {"animate", animateEvt},
            {onUpdateEvent, animBehaviorStartEvt},
            {"OnTranslate", onTranslateEvt},
        }));
    }

    void AnimationBehavior::setParameters(AnimationParameters params) {
        Sprite* sprite = getOwner()->get<Sprite*>();
        Texture spriteTex = sprite->getTexture();
        setInput(map<string, any>({
            {"maxFrameRate",params.frameRate},
            {"speed",params.speed},
            {"maxFrame",params.maxFrame},
            {"looping",params.looping},
            {"startRunning",params.startRunning},
            {"sheetSize",spriteTex.getSize()}
        }));
    }

    void AnimationBehavior::resetProcessData() {
        Sprite* sprite = getOwner()->get<Sprite*>();
        IntRect spriteRect = IntRect({ 0,0 }, sprite->getTextureRect().size);
        setProcess(map<string, any>({
            {"rect",spriteRect},
            {"frame",0},
            {"state",AnimationState::Ready},
            {"frameTime",0.f},
            {"animationStartPos",Vector2i({0,0})}
        }));
    }

    //When the Behavior is started (when the Body is started)
    ScriptEvent AnimationBehavior::animBehaviorStartEvt = [](ScArgs args) {
        IntRect spriteRect = args.behavior->getProcessData<IntRect>("rect");
        bool startRunning = args.behavior->getInputData<bool>("startRunning");

        args.caller->get<Sprite*>()->setTextureRect(spriteRect);
        if (startRunning) {
            args.behavior->callDomain("startAnimation");
        }
    };

    //Animation scripts
    ScriptEvent AnimationBehavior::calculateFrameLengthEvt = [](ScArgs args) {
        float maxFrameRate = args.behavior->getInputData<float>("maxFrameRate");
        float speed = args.behavior->getInputData<float>("speed");
        float frameRate = maxFrameRate * abs(speed);
        float frameLength = 1.f / frameRate;
        args.behavior->setProcessData("frameLength", frameLength);
    };

    ScriptEvent AnimationBehavior::pauseAnimEvt = [](ScArgs args) {
        id_t animationUpdateScriptId = args.behavior->getProcessData<id_t>("animUpdateId");
        args.behavior->setProcessData("state", AnimationState::Paused);
        args.behavior->removeScript(onUpdateEvent, animationUpdateScriptId, true);
    };

    ScriptEvent AnimationBehavior::endAnimEvt = [](ScArgs args) {
        args.behavior->setProcessData("frameTime", 0.f);

        Vector2i startingPos = args.behavior->getProcessData<Vector2i>("animationStartPos");
        IntRect spriteRect = args.behavior->getProcessData<IntRect>("rect");
        spriteRect.position = startingPos;
        args.behavior->setProcessData("rect", spriteRect);
        args.caller->get<Sprite*>()->setTextureRect(spriteRect);

        args.behavior->callDomain("pauseAnimation");
        args.behavior->setProcessData("state", AnimationState::Ready);

        args.behavior->setProcessData("frame", 0);
    };

    ScriptEvent AnimationBehavior::startAnimEvt = [](ScArgs args) {
        args.behavior->callDomain("calculateAnimLength");
        args.behavior->setProcessData("state", AnimationState::Running);

        id_t animationUpdateScriptId = args.behavior->addScript(onUpdateEvent, new Script([](ScArgs args) {
            args.behavior->callDomain("animate");
            }));
        args.behavior->setProcessData("animUpdateId", animationUpdateScriptId);
    };

    ScriptEvent AnimationBehavior::animateEvt = [](ScArgs args) {
        int frame = args.behavior->getProcessData<int>("frame");
        IntRect spriteRect = args.behavior->getProcessData<IntRect>("rect");
        float frameLength = args.behavior->getProcessData<float>("frameLength");
        float state = args.behavior->getProcessData<AnimationState>("state");
        Vector2i animationStartPos = args.behavior->getProcessData<Vector2i>("animationStartPos");

        bool looping = args.behavior->getInputData<bool>("looping");
        bool startRunning = args.behavior->getInputData<bool>("startRunning");
        Vector2u spriteSheetSize = args.behavior->getInputData<Vector2u>("sheetSize");
        int maxFrame = args.behavior->getInputData<int>("maxFrame");

        if (frameLength <= 0) return;
        float frameTime = args.behavior->getProcessData<float>("frameTime") + time.getDeltaSec();
        if (frameTime > frameLength) {
            while ((frameTime -= frameLength) >= 0.f) {
                if (maxFrame > 0 && frame >= maxFrame) {
                    if (looping) {
                        frame = 0;
                        spriteRect.position = animationStartPos;
                    }
                    else {
                        args.behavior->callDomain("pauseAnimation");
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
                                args.behavior->callDomain("pauseAnimation");
                                break;
                            }
                        }
                    }
                }
            }
            args.caller->get<Sprite*>()->setTextureRect(spriteRect);
            args.behavior->setProcessData("frame", frame);
            args.behavior->setProcessData("rect", spriteRect);
        }
        args.behavior->setProcessData("frameTime", frameTime);
    };

    ScriptEvent AnimationBehavior::animBehaviorUpdateEvt = [](ScArgs args) {
        Vector2f facing = args.behavior->getProcessData<Vector2f>("facing");
        Vector2f lastMove = args.behavior->getProcessData<Vector2f>("lastMove");

        if (facing != lastMove && lastMove != V2f({ 0,0 })) {
            if (lastMove == Vector2f({ 0, 1 })) {
                args.behavior->setProcessData("animationStartPos", Vector2i({ 0,0 }));
            }
            else if (lastMove == Vector2f({ 0, -1 })) {
                args.behavior->setProcessData("animationStartPos", Vector2i({ 0,96 }));
            }
            else if (lastMove == Vector2f({ 1, 0 })) {
                args.behavior->setProcessData("animationStartPos", Vector2i({ 0,32 }));
            }
            else {
                args.behavior->setProcessData("animationStartPos", Vector2i({ 0,64 }));
            }

            facing = lastMove;
            args.behavior->setProcessData("facing", facing);
            args.behavior->callDomain("endAnimation");
        }
        lastMove = { 0,0 };
        args.behavior->setProcessData("lastMove", lastMove);
    };

    ScriptEvent AnimationBehavior::onTranslateEvt = [](ScArgs args) {
        AnimationState animState = args.behavior->getProcessData<AnimationState>("state");
        if (animState != AnimationState::Running) {
            args.behavior->callDomain("startAnimation");
        }
        Vector2f lastMove = args.script->getInput().getData<Vector2f>("evt");
        args.behavior->setProcessData("lastMove", lastMove);
    };
}