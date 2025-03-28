#include "Body.h"
#include "../Engine/Engine.h"

namespace CGEngine {
    Body::Body(string displayName) : ScriptController(this) {
        bodyParams.name = displayName;
        scripts.initialize();
    }

    Body::Body(bool isWorldRoot) : ScriptController(this) {
        bodyParams.name = "Root";
        if (!isWorldRoot) {
            world->receiveBodyId(this);
        }
        scripts.initialize();
    }

    Body::Body(Transformable* d, Transformation handle, Body* p, Vector2f uv) : Body() {
        //Cache the base transformable and cast it to a shape
        entity = d;
        //Create the bounds rect and set draw bounds to world's value
        createBoundsRect();
        
        parent = p;
        if (p != nullptr) {
            attach(p);
        } else {
            parent = world->getRoot();
            attach(world->getRoot());
        }
        //Add to be initialized (started)
        world->addUninitialized(this);
        //First move to uv alignment
        moveToAlignment(uv);
        //Then offset by handle, if supplied
        setPosition(handle.position);
        setRotation(handle.angle);
        setScale(handle.scale);
    }
    Body::Body(Transformable* d, Body* p, Transformation handle) : Body(d, handle, p, { 0,0 }) {};

    Body::~Body() {
        //Remove input actions from their domains (without deleting domains) and delete the input actions
        input->eraseActuatorIds(listenerIds);
        //Delete any timers
        timers.deleteTimers(this);
        //Call assigned OnDeleteEvent scripts
        callScripts(onDeleteEvent);
        //Delete scripts and domains (AFTER calling OnDeleteEvent scripts)
        scripts.clear();
        if (entity != nullptr) {
            delete entity;
            entity = nullptr;
        }
        //Remove reference to this Body in its parent, then clear parent
        drop();
        parent = nullptr;
        //Detach any children, then clear the children
        detachChildren();
        children.clear();
        //Delete bounds rect and clear it
        if (boundsRect != nullptr) {
            delete boundsRect;
            boundsRect = nullptr;
        }
    }

    optional<size_t> Body::getId() {
        return bodyId;
    }

    string Body::getName() {
        return bodyParams.name;
    }

    void Body::setName(string name) {
        bodyParams.name = name;
    }

    Transformable* Body::get() const {
        return entity;
    }

    void Body::update(function<void(Shape*)> script, bool updateChildren) {
        update<Shape*>(script, updateChildren);
    }

    void Body::update(function<void(Text*)> script, bool updateChildren) {
        update<Text*>(script, updateChildren);
    }

    void Body::update(function<void(Sprite*)> script, bool updateChildren) {
        update<Sprite*>(script, updateChildren);
    }

    void Body::createBoundsRect() {
        FloatRect rect = getGlobalBounds();
        boundsRect = new RectangleShape();
        boundsRect->setPosition({ rect.position.x, rect.position.y });
        boundsRect->setSize(sf::Vector2f(rect.size.x, rect.size.y));
        boundsRect->setFillColor(sf::Color::Transparent);
        boundsRect->setOutlineColor(sf::Color::White);
        boundsRect->setOutlineThickness(3);
    }

    void Body::updateBoundsRect() {
        if (boundsRect == nullptr)return;
        FloatRect rect = getGlobalBounds();
        boundsRect->setPosition({ rect.position.x, rect.position.y });
        boundsRect->setSize(sf::Vector2f(rect.size.x, rect.size.y));
    }

    void Body::start() {
        if (!initialized) {
            callScripts(onStartEvent);
            initialized = true;
        }
    }

    Transform Body::getGlobalTransform() const {
        Transform wTransform = getTransform();
        Body* p = parent;
        while (p != nullptr && p != world->getRoot()) {
            wTransform.combine(p->getTransform());
            p = p->parent;
        }
        return wTransform;
    }

    FloatRect Body::getGlobalBounds() const{
        if (entity != nullptr) {
            if (Shape* t = dynamic_cast<Shape*>(entity)) {
                return (getGlobalTransform()).transformRect(t->getLocalBounds());
            }
            else if (Text* t = dynamic_cast<Text*>(entity)) {
                return (getGlobalTransform()).transformRect(t->getLocalBounds());
            }
            else if (Sprite* t = dynamic_cast<Sprite*>(entity)) {
                return (getGlobalTransform()).transformRect(t->getLocalBounds());
            }
        } else {
            if (this == world->getRoot()) {
                return FloatRect({ 0,0 }, screen->getSize());
            }
        }
        return getBodyGlobalBounds(this);
    }

    FloatRect Body::getLocalBounds() const {
        if (entity != nullptr) {
            if (Shape* t = dynamic_cast<Shape*>(entity)) {
                return t->getLocalBounds();
            }
            else if (Text* t = dynamic_cast<Text*>(entity)) {
                return t->getLocalBounds();
            }
            else if (Sprite* t = dynamic_cast<Sprite*>(entity)) {
                return t->getLocalBounds();
            }
        } else {
            if (this == world->getRoot()) {
                return FloatRect({ 0,0 }, screen->getSize());
            }
        }
        return getBodyLocalBounds(this);
    }

    V2f Body::getLocalCenter()  const {
        return ((Shape*)entity)->getGeometricCenter();
    }

    V2f Body::getGlobalCenter()  const {
        return getGlobalTransform().transformPoint(((Shape*)entity)->getGeometricCenter());
    }

    V2f Body::getGlobalPosition() const {
        return getGlobalTransform().transformPoint({ 0,0 });
    }

    Angle Body::getGlobalRotation() const {
        V2f dir = getForward();
        float angle = atan2(dir.y, dir.x) * 180.0f / (float)M_PI;
        if (angle < 0) angle += 360.0f;
        return degrees(angle);
    }

    V2f Body::getForward() const {
        Transform wTr = getGlobalTransform();
        Vector2f wPos = wTr.transformPoint({ 0,0 });
        Vector2f rPos = wTr.transformPoint({ 1,0 });
        Vector2f dir = (rPos - wPos);
        if (dir.lengthSquared() > 0.001f) dir = dir.normalized();
        return dir;
    }

    V2f Body::getRight() const {
        Transform wTr = getGlobalTransform();
        Vector2f wPos = wTr.transformPoint({ 0,0 });
        Vector2f uPos = wTr.transformPoint({ 0,1 });
        Vector2f dir = (uPos - wPos);
        if (dir.lengthSquared() > 0.001f) dir = dir.normalized();
        return dir;
    }

    V2f Body::getGlobalScale() const {
        Transform wTr = getGlobalTransform().rotate(-getGlobalRotation());
        Vector2f wPos = wTr.transformPoint({ 0,0 });
        Vector2f rPos = wTr.transformPoint({ 1,1 });
        Vector2f dir = (rPos - wPos);
        return dir;
    }

    void Body::setOriginToUV(V2f uv) {
        setOrigin(getLocalBounds().size.componentWiseMul(uv));
    }

    void Body::setOriginToAlignment(Alignment targetAlignment) {
        setOriginToUV(toUV(targetAlignment));
    }

    V2f Body::getInverseGlobalScale() const{
        return Vector2f({ 1.f,1.f }).componentWiseDiv(getGlobalScale());
    }

    Angle Body::getInverseGlobalRotation() const{
        return degrees(-getGlobalRotation().asDegrees());
    }

    void Body::translate(Vector2f delta, bool useWorldRotation, bool useWorldScale) {
        V2f inverseScale = useWorldScale ? V2f({ 1,1 }) : getInverseGlobalScale();
        Angle inverseRotation = useWorldRotation ? degrees(0) : getInverseGlobalRotation();
        move(delta);
    }

    V2f Body::globalToLocal(V2f worldPoint) const {
        return getGlobalTransform().getInverse() * worldPoint;
    }

    Vector2f Body::viewToGlobal(Vector2i input) const {
        return screen->getWindow()->mapPixelToCoords(input);
    }

    Vector2f Body::viewToLocal(Vector2i input) const {
        return globalToLocal(screen->viewToGlobal(input));
    }

    bool Body::lineIntersects(Vector2f lineStart, Vector2f lineEnd) {
        float xMax = max(lineStart.x, lineEnd.x);
        float xMin = min(lineStart.x, lineEnd.x);
        float yMax = max(lineStart.y, lineEnd.y);
        float yMin = min(lineStart.y, lineEnd.y);
        FloatRect bnds = FloatRect(getGlobalPosition(), getLocalBounds().size);
        float rectLft = bnds.position.x;
        float rectRgt = bnds.position.x + bnds.size.x;
        float rectTop = bnds.position.y;
        float rectBtm = bnds.position.y + bnds.size.y;
        if(rectLft > xMax || rectRgt < xMin){
            return false;
        }
        if (rectTop > yMax || rectBtm < yMin) {
            return false;
        }
        if (rectRgt < xMin && xMax < rectRgt) {
            return true;
        }
        if (rectLft < yMin && yMax < rectLft) {
            return true;
        }
        float m = -((lineEnd.y - lineStart.y) / (lineEnd.x - lineStart.x));
        float lineYAtRectLeft = m * (rectLft - lineStart.x) + lineStart.y;
        float lineYAtRectRight = m * (rectRgt - lineStart.x) + lineStart.y;

        if (rectTop > lineYAtRectLeft && rectTop > lineYAtRectRight) {
            return false;
        }

        if (rectBtm < lineYAtRectLeft && rectBtm < lineYAtRectRight) {
            return false;
        }
        return true;
    }

    bool Body::contains(V2f point) const {
        return getGlobalBounds().contains(world->getRoot()->getTransform() * point);
    }

    Vector2f Body::toUV(Alignment targetAlignment) const {
        switch (targetAlignment) {
        case Center:
            return V2f(0.5f, 0.5f);
        case Center_Right:
            return V2f(1, 0.5f);
        case Center_Left:
            return V2f(0, 0.5f);
        case Top_Right:
            return V2f(1, 0);
        case Top_Left:
            return V2f(0, 0);
        case Top_Center:
            return V2f(0, 0.5f);
        case Bottom_Right:
            return V2f(1, 1);
        case Bottom_Left:
            return V2f(0, 1);
        case Bottom_Center:
            return V2f(0.5f, 1);
        }
        return V2f(0, 0);
    }

    void Body::setAlignment(Alignment targetAlignment) {
        alignment = toUV(targetAlignment);
    }

    void Body::moveToAlignment(V2f uvAlignment, bool updateAlignment) {
        FloatRect parentBounds = FloatRect({ 0,0 }, screen->getSize());
        //No alignment without a parent
        if (parent == nullptr) return;

        parentBounds = parent->getGlobalBounds();
        //Update the alignment if desired
        if (updateAlignment) alignment = uvAlignment;
        Vector2f crnOffset = (parentBounds.position - getGlobalBounds().position);
        setPosition(parent->getLocalBounds().size.componentWiseMul(uvAlignment) - getLocalBounds().size.rotatedBy(getRotation()).componentWiseMul(getScale()).componentWiseMul(uvAlignment) + getOrigin());
    }

    void Body::moveToAlignment(Alignment targetAlignment, bool updateAlignment) {
        Vector2f uv = toUV(targetAlignment);
        moveToAlignment(uv, updateAlignment);
    }

    void Body::moveToAlignment() {
        moveToAlignment(alignment);
    }

    bool Body::getRenderingEnabled() const {
        return bodyParams.rendering;
    }

    void Body::setRenderingEnabled(bool enabled) {
        bodyParams.rendering = enabled;
    }

    bool Body::getBoundsRenderingEnabled() const {
        return bodyParams.boundsRendering;
    }

    void Body::setBoundsRenderingEnabled(bool enabled) {
        bodyParams.boundsRendering = enabled;
    }

    bool Body::getIntersectEnabled() const {
        return bodyParams.intersecting;
    }

    void Body::setIntersectEnabled(bool enabled) {
        bodyParams.intersecting = enabled;
    }

    size_t Body::getChildCount() const {
        return children.size();
    }

    void Body::attachBody(Body* child) {
        //Do nothing on null input
        if (child != nullptr) {
            //If child is already attached, detach
            if (child->parent!=nullptr && child->parent != world->getRoot()) {
                child->detach();
            }
            //Add the child to children
            children.push_back(child);
            //Set the child's parent to this
            child->parent = this;
        }
    }

    void Body::detachBody(Body* child, const bool keepWorldTranform) {
        //Do nothing on null input
        if (child != nullptr) {
            //Find the body to detach
            auto iterator = find_if(children.begin(), children.end(), [child](Body* c) { return c == child; });
            //If found among childen
            if (iterator != children.end()) {
                if (keepWorldTranform) {
                    child->move(getPosition());
                    child->rotate(getRotation());
                    child->scale(getScale());
                }
                //Remove child from children
                children.erase(iterator);
                //parent the child to the world root
                world->getRoot()->attachBody(child);
            }
        }
    }

    void Body::detachChildren(const bool keepWorldTranform) {
        for (int i = children.size() - 1; i >= 0; i--) {
            Body* child = children[i];
            if (keepWorldTranform) {
                child->move(getPosition());
                child->rotate(getRotation());
                child->scale(getScale());
            }
            //Remove child from children
            children.erase(children.begin() + i);
            //parent the child to the world root
            world->getRoot()->attachBody(child);
        }
    }

    void Body::dropBody(Body* child) {
        //Do nothing on null input
        if (child != nullptr) {
            //Find the body to detach
            auto iterator = find_if(children.begin(), children.end(), [child](Body* c) { return c == child; });
            //If found among childen
            if (iterator != children.end()) {
                //Remove child from children
                children.erase(iterator);
                child->parent = nullptr;
            }
        }
    }

    void Body::drop() {
        if (parent != nullptr) {
            parent->dropBody(this);
        }
    }

    void Body::exchangeBody(Body* child, Body* target) {
        if (child != nullptr) {
            //Find the body to detach
            auto iterator = find_if(children.begin(), children.end(), [child](Body* c) { return c == child; });
            //If found among childen
            if (iterator != children.end()) {
                //Remove child from children
                children.erase(iterator);
                //parent the child to the world root
                target->attachBody(child);
            }
        }
    }

    void Body::attach(Body* target) {
        if (target != nullptr) {
            target->attachBody(this);
        }
    }

    void Body::detach() {
        parent->detachBody(this);
    }

    void Body::exchange(Body* target) {
        if (target == nullptr) return;

        parent->exchangeBody(this, target);
    }

    optional<id_t> Body::addOverlapMousePressScript(Script* script, Mouse::Button button, optional<id_t> behaviorId, bool alwaysAddListener) {
        //The input condition called by the InputMap
        InputCondition inputCondition = InputCondition((int)button, InputType::Button, InputState::Pressed);
        if (alwaysAddListener || listenerIds.find(inputCondition) == listenerIds.end()) {
            Behavior* behavior = nullptr;
            if (behaviorId.has_value()) {
                behavior = behaviors.get(behaviorId.value());
            }
            input->addActuator(inputCondition, new Actuator(MouseOverlapPressEvent, this, behavior));
        }
        //Add the script to the input condition for clicking
        if (script != nullptr) {
            return scripts.addScript("mousePress_"+to_string((int)button), script);
        }
        return nullopt;
    }

    optional<id_t> Body::addOverlapMouseReleaseScript(Script* script, Mouse::Button button, optional<id_t> behaviorId, bool alwaysAddListener) {
        //The input condition called by the InputMap
        InputCondition inputCondition = InputCondition((int)button, InputType::Button, InputState::Released);
        if (alwaysAddListener || listenerIds.find(inputCondition) == listenerIds.end()) {
            Behavior* behavior = nullptr;
            if (behaviorId.has_value()) {
                behavior = behaviors.get(behaviorId.value());
            }
            input->addActuator(inputCondition, new Actuator(MouseOverlapReleaseEvent, this, behavior));
        }
        if (script != nullptr) {
            return scripts.addScript("mouseRelease_" + to_string((int)button), script);
        }
        return nullopt;
    }

    optional<id_t> Body::addOverlapMouseHoldScript(Script* script, Mouse::Button button, optional<id_t> behaviorId) {
        addMousePressScript(MouseOverlapHoldPressEvent, button, behaviorId);
        if (script != nullptr) {
            return scripts.addScript("mouseHold_" + to_string((int)button), script);
        }
        return nullopt;
    }

    optional<id_t> Body::addOverlapEnterMouseScript(Script* script, optional<id_t> behaviorId) {
        addMouseMovedScript(MouseOverlapEnterEvent, behaviorId);
        if (script != nullptr) {
            return scripts.addScript("mouseEnter", script);
        }
        return nullopt;
    }

    optional<id_t> Body::addOverlapExitMouseScript(Script* script, optional<id_t> behaviorId) {
        addMouseMovedScript(MouseOverlapExitEvent, behaviorId);
        if (script != nullptr) {
            return scripts.addScript("mouseExit", script);
        }
        return nullopt;
    }

    optional<id_t> Body::addMouseMovedScript(ScriptEvent scriptEvt, optional<id_t> behaviorId) {
        InputCondition inputCondition = InputCondition(0, InputType::Cursor, InputState::Atomic);
        Behavior* behavior = nullptr;
        if (behaviorId.has_value()) {
            behavior = behaviors.get(behaviorId.value());
        }
        return input->addActuator(inputCondition, new Actuator(scriptEvt, this, behavior));
    }

    optional<id_t> Body::addMousePressScript(ScriptEvent scriptEvt, Mouse::Button button, optional<id_t> behaviorId) {
        //The input condition called by the InputMap
        InputCondition inputConditionGlobal = InputCondition((int)button, InputType::Button, InputState::Pressed);
        Behavior* behavior = nullptr;
        if (behaviorId.has_value()) {
            behavior = behaviors.get(behaviorId.value());
        }
        return input->addActuator(inputConditionGlobal, new Actuator(scriptEvt, this, behavior));
    }

    optional<id_t> Body::addMouseReleaseScript(ScriptEvent scriptEvt, Mouse::Button button, optional<id_t> behaviorId) {
        //The input condition called by the InputMap
        InputCondition inputConditionGlobal = InputCondition((int)button, InputType::Button, InputState::Released);
        Behavior* behavior = nullptr;
        if (behaviorId.has_value()) {
            behavior = behaviors.get(behaviorId.value());
        }
        return input->addActuator(inputConditionGlobal, new Actuator(scriptEvt, this, behavior));
    }

    optional<id_t> Body::addKeyPressScript(ScriptEvent scriptEvt, Keyboard::Scan key, optional<id_t> behaviorId) {
        InputCondition inputConditionGlobal = InputCondition((int)key, InputType::Key, InputState::Pressed);
        Behavior* behavior = nullptr;
        if (behaviorId.has_value()) {
            behavior = behaviors.get(behaviorId.value());
        }
        return input->addActuator(inputConditionGlobal, new Actuator(scriptEvt, this, behavior));
    }

    optional<id_t> Body::addKeyReleaseScript(ScriptEvent scriptEvt, Keyboard::Scan key, optional<id_t> behaviorId) {
        InputCondition inputConditionGlobal = InputCondition((int)key, InputType::Key, InputState::Released);
        Behavior* behavior = nullptr;
        if (behaviorId.has_value()) {
            behavior = behaviors.get(behaviorId.value());
        }
        return input->addActuator(inputConditionGlobal, new Actuator(scriptEvt, this, behavior));
    }

    optional<id_t> Body::addTextEnteredScript(ScriptEvent scriptEvt, optional<id_t> behaviorId) {
        InputCondition inputConditionGlobal = InputCondition(0, InputType::Character, InputState::Atomic);
        Behavior* behavior = nullptr;
        if (behaviorId.has_value()) {
            behavior = behaviors.get(behaviorId.value());
        }
        return input->addActuator(inputConditionGlobal, new Actuator(scriptEvt, this, behavior));
    }

    optional<id_t> Body::addKeyHoldScript(Script* script, Keyboard::Scan key, optional<id_t> behaviorId) {
        Behavior* behavior = nullptr;
        addKeyPressScript(KeyHoldPressedEvent, key, behaviorId);
        if (script != nullptr) {
            return scripts.addScript("keyHold_" + to_string((int)key), script);
        }
        return nullopt;
    }

    id_t Body::addIntersectScript(Script* script) {
        if (!intersectScriptId.has_value()) {
            intersectScriptId = addUpdateScript(new Script(onIntersectScript));
        }
        return scripts.addScript(onIntersectEvent, script);
    }

    void Body::removeOverlapMousePressScript(Script* script, Mouse::Button button, bool shouldDelete) {
        if (script != nullptr) {
            scripts.removeScript("mousePress_" + to_string((int)button), script);
        }
    }

    void Body::removeOverlapMouseReleaseScript(Script* script, Mouse::Button button, bool shouldDelete) {
        if (script != nullptr) {
            scripts.removeScript("mouseRelease_" + to_string((int)button), script);
        }
    }

    void Body::removeOverlapMousePressScript(id_t scriptId, Mouse::Button button, bool shouldDelete) {
        scripts.removeScript("mousePress_" + to_string((int)button), scriptId);
    }

    void Body::removeOverlapMouseReleaseScript(id_t scriptId, Mouse::Button button, bool shouldDelete) {
        scripts.removeScript("mouseRelease_" + to_string((int)button), scriptId);
    }

    void Body::eraseListener(InputCondition inputDomain, id_t inputActionId) {
        input->eraseActuator(inputDomain, inputActionId);
    }

    void Body::removeListener(InputCondition inputDomain, id_t inputActionId) {
        input->removeActuator(inputDomain, inputActionId);
    }

    void Body::addListenerId(InputCondition domainName, id_t listenerId) {
        vector<id_t> ids;
        if (listenerIds.find(domainName) != listenerIds.end()) {
            ids = listenerIds[domainName];
        }
        ids.push_back(listenerId);
        listenerIds[domainName] = ids;
    }

    void Body::removeListenerId(InputCondition domainName, id_t listenerId) {
        if (listenerIds.find(domainName) != listenerIds.end()) {
            listenerIds[domainName].erase(listenerIds[domainName].begin() + listenerId);
        }
    }

    timerId_t Body::setTimer(sec_t duration, Script* onCompleteEvent, int loopCount, string timerDisplayName) {
        return timers.setTimer(this, duration, onCompleteEvent, loopCount, timerDisplayName);
    }

    void Body::cancelTimer(size_t timerId) {
        timers.cancelTimer(this, timerId);
    }

    void Body::cancelTimer(timerId_t* timerId) {
        timers.cancelTimer(this, timerId);
    }

    void Body::callScripts(string domain) {
        if (domain == onUpdateEvent) {
            sec_t elapsed = time.getElapsedSec();
            if (elapsed - lastUpdateTime > scriptUpdateInterval) {
                lastUpdateTime = elapsed;
            } else {
                return;
            }
        }

        behaviors.forEach([&domain](Behavior* behavior) { behavior->callDomain(domain); });
        scripts.callDomain(domain);
    }

    void Body::callScriptsWithData(string domain, DataMap data) {
        behaviors.forEach([&domain, &data](Behavior* behavior) { behavior->callDomainWithData(domain, data); });
        scripts.callDomainWithData(domain, nullptr, data);
    }

    ScriptEvent Body::onIntersectScript = [](ScArgs args) {
        vector<Body*> intersections;
        stack<any> intersects;
        //Walk through the Body hierarchy from root and check intersection vs this Body bounds
        vector<Body*> search = { world->getRoot() };
        while (search.size() > 0) {
            Body* other = search.back();
            //For each other body that has bodyParams.intersecting enabled
            if (args.caller != nullptr && other != nullptr){
                if (other != args.caller && other->getIntersectEnabled()) {
                    //If they're intersecting
                    optional<FloatRect> intersection = args.caller->getGlobalBounds().findIntersection(other->getGlobalBounds());
                    if (intersection.has_value()) {
                        //Cache any intersecting bodies and intersection rects
                        intersections.push_back(other);
                        intersects.push(intersection.value());
                        break;
                    }
                }
                //Check the search Body's children
                search.insert(search.begin(), other->children.begin(), other->children.end());
            }

            //Pop from the search list
            search.pop_back();
        }

        //If there are any intersected bodies
        if (intersections.size() > 0) {
            args.script->setOutput(DataMap(map<string, any>({ {"intersects",intersects }})));
            //Call any intersect domain scripts on this body, passing intersections and the intersectScript's data stack
            args.caller->callScriptsWithData(onIntersectEvent, args.script->getOutput());
        }
    };

    void Body::draw(RenderTarget& target, RenderStates states) const {
        // combine the parent transform with the node's one
        Transform tr = getGlobalTransform();

        onDraw(target, tr);

        // draw its children
        for (id_t i = 0; i < children.size(); ++i) {
            children[i]->render(target, tr);
        }
    }

    void Body::render(RenderTarget& target, const Transform& parentTransform) {
        //Combine parent transform and local transform
        Transform combinedTransform = getGlobalTransform();
        renderer.add(this, combinedTransform);

        //Draw children recursively
        for (id_t i = 0; i < children.size(); ++i) {
            children[i]->render(target, combinedTransform);
        }
    }

    void Body::onDraw(RenderTarget& target, const Transform& transform) const {
        if (bodyParams.boundsRendering && boundsRect != nullptr) {
            target.draw(*boundsRect, transform);
        }
        if (bodyParams.rendering && entity != nullptr) {
            if (Mesh* mesh = dynamic_cast<Mesh*>(entity)) {
                // Pull OpenGL State
                mesh->render(transform);
            } else {
                target.draw(*(Shape*)entity, transform);
            }
        }
    }

    Body* Body::deleteBody(ChildrenTermination termination) {
        //Default behavior is that children detach (and therefor attach to world root)
        for (int i = children.size() - 1; i >= 0; --i) {
            switch (termination) {
            case ChildrenTermination::Inherit:
                if (parent != nullptr) {
                    children[i]->exchange(parent);
                    break;
                }
                [[fallthrough]];
            case ChildrenTermination::Orphan:
                children[i]->detach();
                break;
            case ChildrenTermination::Terminate:
                children[i]->deleteBody();
                break;
            }
        }

        delete this;
        return nullptr;
    }

    id_t Body::addBehavior(Behavior* behavior) {
        return behaviors.add(behavior);
    }

    void Body::removeBehavior(id_t behaviorId) {
        behaviors.remove(behaviorId);
    }

    Behavior* Body::getBehavior(id_t behaviorId) {
        return behaviors.get(behaviorId);
    }
}