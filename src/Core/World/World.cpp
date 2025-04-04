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
        endWorld(root);
        running = false;
        window->close();
    }

    void World::endWorld(Body* body) {
        if (auto bd = body) {
            bd->callScripts(onDeleteEvent);
            for (auto iterator = bd->children.begin(); iterator != bd->children.end(); iterator++) {
                Body* b = *iterator;
                endWorld(b);
            }
        }
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

    void World::startUninitializedBodies() {
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
                startUninitializedBodies();
                callScripts(onUpdateEvent);
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

        if (scriptDomain != "delete" || body != root){
            body->callScripts(scriptDomain);
        }

        for (int i = body->children.size() - 1; i >= 0; i--) {
            callScripts(scriptDomain, body->children[i]);
        }
    }

    void World::addDefaultExitActuator() {
        root->addKeyReleaseScript([](ScArgs args) { world->endWorld(); }, Keyboard::Scan::Escape);
    }

    void World::setBoundsRenderingEnabled(bool enabled) {
        boundsRendering = enabled;
        setBoundsRenderingEnabled(enabled, root);
    };

    void World::setBoundsRenderingEnabled(bool enabled, Body* body) {
        body->setBoundsRenderingEnabled(enabled);

        for (int i = body->children.size() - 1; i >= 0; i--) {
            setBoundsRenderingEnabled(enabled, body->children[i]);
        }
    };

    bool World:: getBoundsRenderingEnabled() const {
        return boundsRendering;
    };

    void World::setBoundsColor(Color color) {
        boundsColor = color;
        setBoundsColor(boundsColor, root);
    };

    void World::setBoundsColor(Color color, Body* body) {
        if (body->boundsRect != nullptr) {
            body->boundsRect->setOutlineColor(color);
        }

        for (int i = body->children.size() - 1; i >= 0; i--) {
            setBoundsColor(color, body->children[i]);
        }
    };

    void World::setBoundsThickness(float thickness) {
        boundsThickness = thickness;
        setBoundsThickness(boundsThickness, root);
    };

    void World::setBoundsThickness(float thickness, Body* body) {
        if (body->boundsRect != nullptr) {
            body->boundsRect->setOutlineThickness(thickness);
        }

        for (int i = body->children.size() - 1; i >= 0; i--) {
            setBoundsThickness(thickness, body->children[i]);
        }
    }

    V2f World::getGlobalPosition(Transform transform) const {
        return transform.transformPoint({ 0,0 });
    }

    Angle World::getGlobalRotation(Transform transform) const {
        V2f dir = getForward(transform);
        float angle = atan2(dir.y, dir.x) * 180.0f / (float)M_PI;
        if (angle < 0) angle += 360.0f;
        return degrees(angle);
    }

    V2f World::getForward(Transform transform) const {
        Vector2f wPos = transform.transformPoint({ 0,0 });
        Vector2f rPos = transform.transformPoint({ 1,0 });
        Vector2f dir = (rPos - wPos);
        if (dir.lengthSquared() > 0.001f) dir = dir.normalized();
        return dir;
    }

    V2f World::getRight(Transform transform) const {
        Vector2f wPos = transform.transformPoint({ 0,0 });
        Vector2f uPos = transform.transformPoint({ 0,1 });
        Vector2f dir = (uPos - wPos);
        if (dir.lengthSquared() > 0.001f) dir = dir.normalized();
        return dir;
    }

    V2f World::getGlobalScale(Transform transform) const {
        Transform r_wT = transform.rotate(getInverseGlobalRotation(transform));
        Vector2f wPos = transform.transformPoint({ 0,0 });
        Vector2f rPos = transform.transformPoint({ 1,1 });
        Vector2f dir = (rPos - wPos);
        return dir;
    }
    V2f World::getInverseGlobalScale(Transform transform) const {
        return Vector2f({ 1.f,1.f }).componentWiseDiv(getGlobalScale(transform));
    }

    Angle World::getInverseGlobalRotation(Transform transform) const {
        return degrees(-getGlobalRotation(transform).asDegrees());
    }
}