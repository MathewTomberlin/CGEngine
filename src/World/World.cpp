#include "World.h"
#include "WorldInstance.h"
#include <memory>
#include <queue>

namespace CGEngine {
    World::World() : root(bodies.get(create())) { }

    World::World(Vector2u windowSize) : root(bodies.get(create())) { }

    World::World(string title) : root(bodies.get(create())) {
        appTitle = title;
    }

    World::World(Vector2u windowSize, string title) : root(bodies.get(create())) {
        appTitle = title;
    }

    void World::initializeConsole() {
        if (!consoleInitialized && consoleFeatureEnabled) {
            consoleTextBox = new Body(new Text(*defaultFont), Transformation());
            consoleTextBox->moveToAlignment(Alignment::Bottom_Left);
            consoleTextBox->move({ 20,-35 });
            consoleTextBox->zOrder = 100;
            consoleTextBox->addTextEnteredScript([](ScArgs args) {
                if (world->consoleInputEnabled) {
                    const Event::TextEntered* evt = args.script->pullOutInputPtr<const Event::TextEntered>();
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
                                Body* targetBody = world->findBodyByName(target);
                                targetBody->callScriptsWithData(command, DataStack(stack<any>({ inputStrings })));
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
                    Vector2f viewSize = world->getCurrentView()->getSize();
                    args.caller->setRotation(world->getCurrentView()->getRotation());
                    args.caller->setPosition((world->getCurrentView()->getInverseTransform() * V2f({ -1,1 })));
                    args.caller->get<Text*>()->setString("_");
                } else {
                    args.caller->get<Text*>()->setString("");
                }
            }, Keyboard::Scan::Grave);

            addWorldScript("ToggleBounds", new Script([](ScArgs args) {
                Body* foundBody = nullptr;
                vector<string> inputStrings = args.script->pullOutInput<vector<string>>();
                if (inputStrings.size() >= 1) {
                    string objName = inputStrings[0];
                    foundBody = world->findBodyByName(objName);
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

    void World::end() {
        if (input != nullptr) {
            input->clear();
        }
        end(root);
        window->close();
    }

    void World::end(Body* body) {
        if (auto bd = body) {
            bd->callScripts(onDeleteEvent);
            for (auto iterator = bd->children.begin(); iterator != bd->children.end(); iterator++) {
                Body* b = *iterator;
                end(b);
            }
        }
    }

    Body* World::getRoot() {
        return root;
    }

    Body* World::findBodyByName(string name) {
        //Starting with root
        queue<Body*> bodies = queue<Body*>({ root });
        //For each body in the stack
        while (!bodies.empty()) {
            //If the top body has the name, return it
            Body* b = bodies.front();
            if (b != nullptr && b->getName() == name) {
                return b;
            }
            //Otherwise add its children to the stack
            for (Body* child : b->children) {
                bodies.push(child);
            }
            //And pop it
            bodies.pop();
        }
        //Body with the name wasn't found
        return nullptr;
    }

    View* World::getCurrentView() {
        return currentView;
    }

    void World::moveView(Vector2f delta) {
        currentView->move(delta);
        window->setView(*currentView);
    }

    void World::rotateView(Angle delta) {
        currentView->rotate(delta);
        window->setView(*currentView);
    }

    void World::zoomView(float delta) {
        currentView->zoom(delta);
        window->setView(*currentView);
    }

    Vector2f World::viewToGlobal(Vector2i pixels) {
        if (window != nullptr) {
            return window->mapPixelToCoords(pixels);
        }
        return Vector2f();
    }

    vector<Body*> World::zRayCast(Vector2f worldPos, optional<int> startZ, int distance, bool backward, bool linecast) {
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
        vector<Body*> hits;
        for (int i = 0; i <= zDist; ++i) {
            int index = currentZ + (i * d);
            vector<Body*> bodies = renderer.getZBodies(index);
            if (!backward) {
                for (int x = bodies.size() - 1; x >= 0; x--) {
                    if (bodies[x]->contains(worldPos)) {
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
                    if (bodies[x]->contains(worldPos)) {
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

    vector<Body*> World::raycast(Vector2f worldPos, Vector2f castDir, int zIndex, float distance, bool linecast) {
        vector<Body*> hits;
        vector<Body*> bodies = renderer.getZBodies(zIndex);
        Vector2f targetPos = worldPos + (castDir * distance);
        for (int x = bodies.size() - 1; x >= 0; x--) {
            if (bodies.at(x)->lineIntersects(worldPos, targetPos)) {
                hits.push_back(bodies.at(x));
                if (!linecast) {
                    break;
                }
            }
        }
        return hits;
    }

    Screen* World::createScreen(Vector2u windowSize, string appTitle) {
        return new Screen(windowSize, appTitle);
    }

    RenderWindow* World::getWindow() const {
        return window;
    }

    void World::addScene(string sceneName, Scene* scene) {
        scenes[sceneName] = scene;
    }

    void World::loadScene(string sceneName, Body* sceneRoot) {
        if (scenes.find(sceneName) != scenes.end()) {
            Scene* scene = scenes[sceneName];
            scene->load(sceneRoot);
        }
    }

    void World::loadSceneWithData(string sceneName, DataStack input, Body* sceneRoot) {
        if (scenes.find(sceneName) != scenes.end()) {
            Scene* scene = scenes[sceneName];
            scene->setInput(input);
            scene->load(sceneRoot);
        }
    }

    optional<DataStack> World::getSceneData(string sceneName) {
        if (scenes.find(sceneName) != scenes.end()) {
            Scene* scene = scenes[sceneName];
            DataStack output = scene->getOutput();
            return output;
        }
    }

    void World::startWorld() {
        V2f screenSize = screen->getSize();
        window = new RenderWindow(VideoMode(screenSize), appTitle);
        screen->window = window;
        input->setWindow(window);
        currentView = new View(screenSize / 2.f, screenSize);
        window->setView(*currentView);
        if(sceneList.size()>0){
            for (auto iterator = sceneList.begin(); iterator != sceneList.end(); ++iterator) {
                addScene((*iterator)->getDisplayName(), (*iterator));
            }
            loadScene(sceneList.at(0)->getDisplayName());
        }
    }

    void World::renderWorld() {
        window->clear();
        render();
        window->display();
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
        while (window->isOpen()) {
            deleted.clear();
            updateTime();
            startUninitializedBodies();
            callScripts(onUpdateEvent);
            input->gather();
            renderWorld();
        }
    }

    void World::updateTime() {
        time.updateDeltaTime();
    }

    void World::render() {
        renderer.clear();
        root->render(*window, root->getTransform());
        renderer.render(window);
    }

    void World::callScripts(string scriptDomain, Body* body) {
        if (body == nullptr) {
            body = root;
        }
        if (!isDeleted(body)) {
            if (scriptDomain != "delete" || body != root){
                body->callScripts(scriptDomain);
            }

            for (int i = body->children.size() - 1; i >= 0; i--) {
                callScripts(scriptDomain, body->children[i]);
            }
        }
    }

    void World::addDefaultExitActuator() {
        root->addKeyReleaseScript([](ScArgs args) { world->end(); }, Keyboard::Scan::Escape);
    }

    id_t World::create(Transformable* entity, Transformation transform, Body* parent, Script* startScript) {
        Body* newBody = (entity==nullptr) ? new Body("Root") : new Body(entity, transform, parent);
        id_t bodyId = receiveBodyId(newBody);
        if (startScript != nullptr) {
            newBody->addStartScript(startScript);
        }
        return bodyId;
    }

    id_t World::create(Transformable* entity) {
        return create(entity, Transformation());
    }

    id_t World::create(Transformable* entity, Body* parent, Script* startScript, Transformation transform) {
        return create(entity, transform, parent, startScript);
    }
    
    id_t World::create(Transformable* entity, Script* startScript, Transformation transform, Body* parent) {
        return create(entity, transform, parent, startScript);
    }

    void World::addDeletedBody(Body* body) {
        deleted.insert(body);
    }

    bool World::isDeleted(Body* body) {
        return deleted.find(body) != deleted.end();
    }

    /// <summary>
    /// Delete a body and set the provided pointer null
    /// </summary>
    /// <param name="body"></param>
    /// <param name="childTermination"></param>
    void World::deleteBody(Body* body, ChildrenTermination childTermination) {
        body->deleteBody(childTermination);
        deleted.insert(body);
    }

    id_t World::receiveBodyId(Body* body) {
        //return ids.receive(&body->bodyId);

        id_t id = bodies.add(body);
        body->bodyId = id;
        return id;
    }

    void World::refundBodyId(Body* body) {
        //ids.refund(&body->bodyId);

        optional<id_t> bodyId = body->getId();
        if (bodyId.has_value()) {
            bodies.remove(bodyId.value());
            body->bodyId = nullopt;
        }
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

    bool World::getBoundsRenderingEnabled() {
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
}