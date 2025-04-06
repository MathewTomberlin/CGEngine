#include "World.h"
#include "../Engine/Engine.h"
#include "../../Standard/Models/CommonModels.h"

namespace CGEngine {
    World::World() : root(assets.get<Body>(assets.create<Body>("Root", true).value())) {
		if (root == nullptr) {
			log(this, LogError, "Failed to create root body");
        } else {
			log(this, LogInfo, "Created root body with ID: {}", root->getId());
        }
        init();
    }

    void World::initializeConsole() {
        if (!consoleInitialized && consoleFeatureEnabled) {
            Font* defaultFont = assets.get<FontResource>(assets.getDefaultId<FontResource>().value())->getFont();
            consoleTextBox = new Body(new Text(*defaultFont), Transformation());
            consoleTextBox->moveToAlignment(Alignment::Bottom_Left);
            consoleTextBox->move({ 20,-35 });
            consoleTextBox->zOrder = 100;
            consoleTextBox->addTextEnteredScript([](ScArgs args) {
                if (world->consoleInputEnabled) {
                    TextEnteredInput* evt = args.script->getInput().getDataPtr<TextEnteredInput>("evt");
                    if (evt == nullptr) return;
                    char32_t ch = evt->unicode;
                    if (ch < 128 && ch != 8 && ch != 13 && ch != 96) {
                        char c = static_cast<char>(ch);
                        Text* txt = args.caller->get<Text*>();
                        string lastEntered = txt->getString();
                        lastEntered = lastEntered.substr(0, lastEntered.size() - 1);
                        txt->setString(lastEntered + c + "_");
                    }
                    else if (ch == 8) {
                        Text* txt = args.caller->get<Text*>();
                        string lastEntered = txt->getString();
                        if (lastEntered.length() > 1) {
                            lastEntered = lastEntered.substr(0, max(0, (int)lastEntered.size() - 2));
                            txt->setString(lastEntered + "_");
                        }
                    }
                    else if (ch == 13) {
                        Text* txt = args.caller->get<Text*>();
                        string lastEntered = txt->getString();
                        if (lastEntered.length() > 1) {
                            lastEntered = lastEntered.substr(0, max(0, (int)lastEntered.size() - 1));
                            vector<string> inputStrings;
                            string target = "Root";
                            string commandString = lastEntered;
                            string command = lastEntered;
                            string inputString = "";
                            auto dotOperatorIndex = lastEntered.find('.');
                            if (dotOperatorIndex < lastEntered.size()) {
                                target = lastEntered.substr(0, dotOperatorIndex);
                                commandString = lastEntered.substr(dotOperatorIndex + 1, lastEntered.size());
                                command = commandString;
                            }
                            auto argOperatorIndex = commandString.find(':');
                            if (argOperatorIndex < commandString.size()) {
                                command = commandString.substr(0, argOperatorIndex);
                                inputString = commandString.substr(argOperatorIndex + 1, commandString.size());
                                string parse = "";
                                stringstream inputStream(inputString);
                                while (getline(inputStream, parse, ',')) {
                                    inputStrings.push_back(parse);
                                }
                            }
                            if (inputStrings.size() <= 0) {
                                inputStrings.push_back(inputString);
                            }
                            world->lastConsoleTarget = target;
                            world->lastConsoleCommand = command;
                            world->lastConsoleInput = inputString;
                            if (target != "") {
                                Body* targetBody = assets.get<Body>(target);
                                targetBody->callScriptsWithData(command, DataMap(map<string, any>({ { "args",inputStrings} })));
                            }
                        }
                        txt->setString("_");
                    }
                }
            });

            consoleTextBox->addKeyReleaseScript([](ScArgs args) {
                if (world->consoleInputEnabled) {
                    Text* txt = args.caller->get<Text*>();
                    string argOperatorString = ((world->lastConsoleInput != "") ? ":" : "");
                    string dotOperatorString = ((world->lastConsoleTarget != "" && world->lastConsoleTarget != "Root") ? "." : "");
                    string targetString = (world->lastConsoleTarget == "Root") ? "" : world->lastConsoleTarget;
                    txt->setString(targetString + dotOperatorString + world->lastConsoleCommand + argOperatorString + world->lastConsoleInput + "_");
                }
            }, Keyboard::Scan::Up);

            consoleTextBox->addKeyReleaseScript([](ScArgs args) {
                world->consoleInputEnabled = !world->consoleInputEnabled;
                if (world->consoleInputEnabled) {
                    View* currentView = screen->getCurrentView();
                    Vector2f viewSize = currentView->getSize();
                    args.caller->setRotation(currentView->getRotation());
                    args.caller->setPosition((currentView->getInverseTransform() * V2f({ -1,1 })));
                    args.caller->get<Text*>()->setString("_");
                } else {
                    args.caller->get<Text*>()->setString("");
                }
            }, Keyboard::Scan::Grave);

            addWorldScript("ToggleBounds", new Script([](ScArgs args) {
                Body* foundBody = nullptr;
                vector<string> inputStrings = args.script->getInput().getData<vector<string>>("args");
                if (inputStrings.size() >= 1) {
                    string objName = inputStrings[0];
                    foundBody = assets.get<Body>(objName);
                }
                if (foundBody != nullptr) {
                    cout << "Drawing bounds for " << foundBody->getName() << "\n";
                    foundBody->updateBoundsRect();
                    foundBody->setBoundsRenderingEnabled(!foundBody->getBoundsRenderingEnabled());
                }
                else {
                    world->setBoundsRenderingEnabled(!world->getBoundsRenderingEnabled());
                }
            }));
            consoleInitialized = true;
        }
    }

    void World::addWorldScript(string domain, Script* script) {
        if (auto worldRoot = root) {
            worldRoot->addScript(domain, script);
        }
    }

    void World::endWorld() {
        if (input != nullptr) {
            input->clear();
        }
        root->apply([](Body* b) { b->callStaticScripts(DeleteDomain); });
        running = false;
        window->close();
    }

    Body* World::getRoot() {
        return root;
    }

    vector<id_t> World::zRayCast(Vector2f worldPos, optional<int> startZ, int distance, bool backward, bool linecast) {
        int zMax = renderer.zMax();
        int zMin = renderer.zMin();

        //Start at the max or min Z if startZ is nullopt
        int currentZ = 0;
        if (startZ == nullopt) {
            if (!backward) {
                currentZ = zMax;
            } else {
                currentZ = zMin;
            }
        } else {
            currentZ = startZ.value();
        }
        
        //Traverse the whole Z distance if distance < 0
        int zDist = currentZ;
        if (distance < 0) {
            zDist = abs(zMax - zMin);
        }
        else {
            zDist = distance;
        }

        //Traverse the renderOrder by z index, getting the ordered stack of bodies and returning if it's contained
        int d = (backward) ? 1 : -1;
        vector<id_t> hits;
        for (int i = 0; i <= zDist; ++i) {
            int index = currentZ + (i * d);
            vector<id_t> bodies = renderer.getZBodies(index);
            if (!backward) {
                for (int x = bodies.size() - 1; x >= 0; x--) {
					Body* body = assets.get<Body>(bodies[x]);
                    if (body->contains(worldPos)) {
                        if (!linecast) {
                            return { bodies[x] };
                        } else {
                            hits.push_back(bodies[x]);
                        }
                    }
                }
            }
            else {
                for (int x = 0; x < bodies.size(); x++) {
					Body* body = assets.get<Body>(bodies[x]);
                    if (body->contains(worldPos)) {
                        if (!linecast) {
                            return { bodies[x] };
                        } else {
                            hits.push_back(bodies[x]);
                        }
                    }
                }
            }
        }
        return hits;
    }

    vector<id_t> World::raycast(Vector2f worldPos, Vector2f castDir, int zIndex, float distance, bool linecast) {
        vector<id_t> hits;
        vector<id_t> bodies = renderer.getZBodies(zIndex);
        Vector2f targetPos = worldPos + (castDir * distance);
        for (int x = bodies.size() - 1; x >= 0; x--) {
			Body* body = assets.get<Body>(bodies[x]);
            if (body->lineIntersects(worldPos, targetPos)) {
                hits.push_back(bodies.at(x));
                if (!linecast) {
                    break;
                }
            }
        }
        return hits;
    }

    void World::addScene(string sceneName, Behavior* scene) {
        scenes[sceneName] = scene;
    }

    void World::loadScene(string sceneName) {
        if (scenes.find(sceneName) != scenes.end()) {
            Behavior* scene = scenes[sceneName];
            scene->callDomain(onLoadEvent);
        }
    }

    void World::loadSceneWithInput(string sceneName, DataMap input) {
        if (scenes.find(sceneName) != scenes.end()) {
            Behavior* scene = scenes[sceneName];
            scene->setInput(input);
            scene->callDomain(onLoadEvent);
        }
    }

    optional<DataMap> World::getSceneOutput(string sceneName) {
        if (scenes.find(sceneName) != scenes.end()) {
            Behavior* scene = scenes[sceneName];
            DataMap output = scene->getOutput();
            return output;
        }
        return nullopt;
    }

    optional<DataMap> World::getSceneInput(string sceneName) {
        if (scenes.find(sceneName) != scenes.end()) {
            Behavior* scene = scenes[sceneName];
            DataMap input = scene->getInput();
            return input;
        }
        return nullopt;
    }

    optional<DataMap> World::getSceneProcess(string sceneName) {
        if (scenes.find(sceneName) != scenes.end()) {
            Behavior* scene = scenes[sceneName];
            DataMap process = scene->getProcess();
            return process;
        }
        return nullopt;
    }

    void World::startWorld() {
        //Create window (via Screen and using the static WindowParameters) and set InputMap's window
        screen->setWindowParameters(windowParameters);
        window = screen->createWindow();
        if(!window->setActive(true)) {
            log(LogLevel::LogError, "World", "Failed to set window as active OpenGL context");
        }
        renderer.setWindow(window);
        renderer.initGlew();
        input->setWindow(window);

        assets.initialize();

        //Assign the fallback material
        renderer.fallbackMaterialId = 0;

        running = true;
    }

    void World::initSceneList() {
        if (sceneList.size() > 0) {
            for (auto iterator = sceneList.begin(); iterator != sceneList.end(); ++iterator) {
                Behavior* scene = (*iterator);
                addScene(scene->getName(), scene);
            }

            loadScene(sceneList.at(0)->getName());
        }
    }

    void World::addUninitialized(Body* body) {
        uninitialized.push_back(body);
    }

    void World::callUninitializedStart() {
        while (uninitialized.size() > 0) {
            if (auto uninit = uninitialized.back()) {
                uninit->start();
                uninitialized.pop_back();
            }
        }
        if (!consoleInitialized) {
            initializeConsole();
        }
    }

    void World::runWorld() {
        while (running) {
            renderer.initializeOpenGL();
            initSceneList();

            while (window->isOpen()) {
                updateTime();
				if (uninitialized.size() > 0) {
					callUninitializedStart();
				}
                callStaticScripts(UpdateDomain);
                input->gather();
                if (window->isOpen()) {
                    if (renderer.setGLWindowState(true)) {
                        renderer.clearGL(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                        if (!renderer.processRender()) return;
                        renderer.setGLWindowState(false);
                    }
                }
            }
        }
    }

    void World::updateTime() {
        time.updateDeltaTime();
    }

    void World::callScripts(string scriptDomain, Body* body) {
        if (body == nullptr) {
            body = root;
        }

        body->apply([&scriptDomain](Body* b) { if (scriptDomain != onDeleteEvent || b != world->root) { b->callScripts(scriptDomain); } });
    }

    void World::callStaticScripts(StaticScriptDomain scriptDomainId, Body* body) {
		if (scriptDomainId < 0 || scriptDomainId > 2) return;

        if (body == nullptr) {
            body = root;
        }

        body->apply([&scriptDomainId](Body* b) { if (scriptDomainId != 0 || b != world->root) { b->callStaticScripts(scriptDomainId); } });
    }

    void World::addDefaultExitActuator() {
        root->addKeyReleaseScript([](ScArgs args) { world->endWorld(); }, Keyboard::Scan::Escape);
    }

    void World::setBoundsRenderingEnabled(bool enabled) {
        boundsRendering = enabled;
		root->apply([&enabled](Body* b) { b->setBoundsRenderingEnabled(enabled); });
    };

    bool World:: getBoundsRenderingEnabled() const {
        return boundsRendering;
    };

    void World::setBoundsColor(Color color) {
        boundsColor = color;
		root->apply([&color](Body* b) { if (b->boundsRect) { b->boundsRect->setOutlineColor(color); } });
    };

    void World::setBoundsThickness(float thickness) {
        boundsThickness = thickness;
		root->apply([&thickness](Body* b) { if (b->boundsRect) { b->boundsRect->setOutlineThickness(thickness); } });
    };
}