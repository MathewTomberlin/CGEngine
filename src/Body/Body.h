#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include <memory>
#include "../Types/V2.h"
#include "../Scripts/ScriptMap.h"
#include "../Input/InputMap.h"
#include "../Timers/TimerMap.h"
#include "../Drawables/Tilemap.h"
#include "../Behavior/Behavior.h"
#include "../Types/UniqueDomain.h"
#include "../Types/DataMap.h"
using namespace std;
using namespace sf;

namespace CGEngine {
    enum ChildrenTermination { Inherit, Orphan, Terminate };
    enum Alignment { Center, Top_Center, Bottom_Center, Center_Right, Center_Left, Top_Right, Top_Left, Bottom_Right, Bottom_Left };

    struct BodyParameters {
        BodyParameters(bool _intersecting = false, bool _rendering = true, bool _boundsRendering = false, string _name = "") : intersecting(_intersecting), rendering(_rendering), boundsRendering(_boundsRendering), name(_name) {

        }

        string name = "";
        bool rendering = true;
        bool intersecting = false;
        bool boundsRendering = false;
    };

    class Body : public Transformable, public Drawable {
    public:
        Body(Transformable* d, Transformation handle = Transformation(), Body* p = nullptr, Vector2f align = {0,0});
        Body(Transformable* d, Body* p, Transformation handle = Transformation());

        virtual ~Body();
        /// <summary>
        /// Get the Body's unique id
        /// </summary>
        /// <returns>The Body's unique id or nullopt</returns>
        optional<size_t> getId();
        /// <summary>
        /// The Body's user assigned name (or 'Root' for the world root body)
        /// </summary>
        /// <returns>The user assigned Body name</returns>
        string getName();
        /// <summary>
        /// Set the Body's name
        /// </summary>
        /// <param name="name">The new name</param>
        void setName(string name);
        /// <summary>
        /// Return the drawn Shape as an object of the indicated type
        /// </summary>
        /// <typeparam name="T">The type of object to return the drawn Shape as</typeparam>
        /// <returns>The drawn Shape as the indicated type</returns>

        template <typename T = Shape*>
        T get() {
            return dynamic_cast<T>(entity);
        }

        void set(Transformable* ent);
        /// <summary>
        /// Call the script on the drawn object, cast to the supplied type, and on each child recursively (if updateChildren is true)
        /// </summary>
        /// <typeparam name="T">The type to cast the drawn object to</typeparam>
        /// <param name="script">The script to call for the drawn object</param>
        /// <param name="updateChildren">Whether to recursively call the script on children</param>
        template <typename T = Shape*>
        void update(function<void(T)> script, bool updateChildren = false) {
            T ent = get<T>();
            if (ent != nullptr) {
                script(ent);
            }

            if (updateChildren) {
                for (int i = children.size() - 1; i >= 0; i--) {
                    children[i]->update(script, true);
                }
            }
        }
        void update(function<void(Shape*)> script, bool updateChildren = false);
        void update(function<void(Text*)> script, bool updateChildren = false);
        void update(function<void(Sprite*)> script, bool updateChildren = false);

        /// <summary>
        /// Returns the Transform of the Body in world space
        /// </summary>
        /// <returns>The Body's Transform in world space</returns>
        Transform getGlobalTransform() const;
        /// <summary>
        /// Returns the rectangle surrounding the drawn Shape with all translate, rotate, and scale transformations applied
        /// </summary>
        /// <returns>Rect  without any translate, rotate, or scale transformations applied</returns>
        FloatRect getGlobalBounds() const;
        /// <summary>
        /// Returns the rectangle surrounding the drawn Shape without any translate, rotate, or scale transformations applied
        /// </summary>
        /// <returns>Rect  without any translate, rotate, or scale transformations applied</returns>
        FloatRect getLocalBounds() const;
        /// <summary>
        /// Overrideable method to get the entity local bounds when the entity is not a built-in type
        /// </summary>
        /// <returns>The FloatRect local bounds of the entity</returns>
        virtual FloatRect getBodyLocalBounds() const;
        /// <summary>
        /// Overrideable method to get the entity global bounds when the entity is not a built-in type
        /// </summary>
        /// <returns>The FloatRect global bounds of the entity</returns>
        virtual FloatRect getBodyGlobalBounds() const;
        /// <summary>
        /// Returns the position of the Body in world space
        /// </summary>
        /// <returns>The Body's position in world space</returns>
        V2f getGlobalPosition() const;
        /// <summary>
        /// Returns the rotation (from 0 to 360 degrees) of the Body in world space
        /// </summary>
        /// <returns>The Body's rotation in world space</returns>
        Angle getGlobalRotation() const;
        /// <summary>
        /// Returns the scale of the Body in world space
        /// </summary>
        /// <returns>The Body's scale in world space</returns>
        V2f getGlobalScale() const;
        /// <summary>
        /// Return 1 / global scale (inverse global scale)
        /// </summary>
        /// <returns>The Body's scale in world space</returns>
        V2f getInverseGlobalScale() const;
        /// <summary>
        /// Return the negative (inverse) global rotation
        /// </summary>
        /// <returns>The Body's inverse global rotation</returns>
        Angle getInverseGlobalRotation() const;
        /// <summary>
        /// Returns the normalized world space direction of the Body's forward vector
        /// </summary>
        /// <returns>The Body's rotation in world space as a normalized direction vector</returns>
        V2f getForward() const;
        /// <summary>
        /// Returns the normalized world space direction of the Body's right vector
        /// </summary>
        /// <returns>The Body's rotation in world space as a normalized direction vector</returns>
        V2f getUp() const;
        /// <summary>
        /// Gets the drawn Shape's local geometric center
        /// </summary>
        /// <returns>The local geometric center of the drawn Shape</returns>
        V2f getLocalCenter() const;
        /// <summary>
        /// Move the body, ignoring its rotation and scale when moving unless useWorldRotation and/or useWorldScale are true
        /// </summary>
        /// <param name="delta">The delta vector to move the body in Global Space</param>
        /// <param name="useWorldRotation">Whether to apply world rotation when making the move</param>
        /// <param name="useWorldScale">Whether to apply world scale when making the move</param>
        void translate(Vector2f delta, bool useWorldRotation = false, bool useWorldScale = false);
        /// <summary>
        /// Gets the drawn Shape's world space geometric center
        /// </summary>
        /// <returns>The world space geometric center of the drawn Shape</returns>
        V2f getGlobalCenter() const;
        /// <summary>
        /// Set the drawn Shape's origin (and the boundsRect origin, if valid) to a UV
        /// </summary>
        /// <param name="uvPosition">The UV position of the origin</param>
        void setOriginToUV(V2f uvPosition);
        /// <summary>
        /// Set the drawn Shape's origin (and the boundsRect origin, if valid) to a cardinal position
        /// </summary>
        /// <param name="alignmentPosition">The named cardinal position of the origin</param>
        void setOriginToAlignment(Alignment alignmentPosition);
        /// <summary>
        /// Convert a world space point to a local space point
        /// </summary>
        /// <param name="worldPoint">The point in world space</param>
        /// <returns></returns>
        V2f globalToLocal(V2f worldPoint) const;
        /// <summary>
        /// Given a vector in pixel space (such as a mouse position), convert it to a world position
        /// </summary>
        /// <param name="input">The vector in pixel space</param>
        /// <returns>The vector in world space</returns>
        Vector2f viewToGlobal(Vector2i input) const;
        /// <summary>
        /// Given a vector in pixel space (such as a mouse position), convert it to a local space position
        /// </summary>
        /// <param name="input">The vector in pixel space</param>
        /// <returns>The vector in local space</returns>
        Vector2f viewToLocal(Vector2i input) const;

        bool lineIntersects(Vector2f lineStart, Vector2f lineEnd);
        /// <summary>
        /// Checks if the assigned point is within the drawn Shape's global bounds
        /// </summary>
        /// <param name="point">The point to check</param>
        /// <returns>True if the point is within the shape's global bounds</returns>
        bool contains(V2f point) const;

        /// <summary>
        /// The UV location the object should be visually located in the parent's bounds
        /// </summary>
        V2f alignment = { 0,0 };
        /// <summary>
        /// Given an Alignment, return the corresponding UV vector
        /// </summary>
        /// <param name="targetAlignment"></param>
        /// <returns></returns>
        Vector2f toUV(Alignment targetAlignment) const;
        /// <summary>
        /// Set the Body's alignment to a named cardinal value. Does not move the object to the alignment that is set.
        /// </summary>
        /// <param name="targetAlignment">The named cardinal alignment to move the object to</param>
        void setAlignment(Alignment targetAlignment);
        /// <summary>
        /// Move the Body to the assigned, named cardinal alignment. If desired, set the Body alignment to the assigned value
        /// </summary>
        /// <param name="targetAlignment">The named cardinal alignment to move the Body to</param>
        /// <param name="updateAlignment">If true, the Body alignment will be updated to the assigned value</param>
        void moveToAlignment(Alignment targetAlignment, bool updateAlignment = true);
        /// <summary>
        /// Move the Body to the assigned UV position. If desired, set the Body alignment to the assigned value
        /// </summary>
        /// <param name="uvAlignment">The UV position to move the Body to</param>
        /// <param name="updateAlignment">If true, the Body alignment will be updated to the assigned value</param>
        void moveToAlignment(V2f uvAlignment, bool updateAlignment = true);
        /// <summary>
        /// Move the Body to the its alignment position.
        /// </summary>
        void moveToAlignment();

        /// <summary>
        /// Return whether or not the Body will be rendered (by world->renderWorld())
        /// </summary>
        /// <returns>True if the Body will be rendered</returns>
        bool getRenderingEnabled() const;
        /// <summary>
        /// Disable rendering of this Body (by world->renderWorld())
        /// </summary>
        /// <param name="visible">Whether the Body is visible or not</param>
        void setRenderingEnabled(bool enabled);
        /// <summary>
        /// Return whether or not the Body will be rendered (by world->renderWorld())
        /// </summary>
        /// <returns>True if the Body will be rendered</returns>
        bool getBoundsRenderingEnabled() const;
        /// <summary>
        /// Disable rendering of this Body (by world->renderWorld())
        /// </summary>
        /// <param name="visible">Whether the Body is visible or not</param>
        void setBoundsRenderingEnabled(bool enabled);
        /// <summary>
        /// Return whether or not the Body will intersect other Bodies with intersect enabled
        /// </summary>
        /// <returns>True if the Body will intersect others</returns>
        bool getIntersectEnabled() const;
        /// <summary>
        /// Set whether or not the Body will intersect other Bodies with intersect enabled
        /// </summary>
        /// <param name="visible">Whether the Body intersects others or not</param>
        void setIntersectEnabled(bool enabled);

        /// <summary>
        /// Detach the Body from its parent, if it has one, and attach it to this Body
        /// </summary>
        /// <param name="body">The Body to attach to this Body</param>
        void attachBody(Body* body);
        /// <summary>
        /// Detach from a parent, if this Body has one, then attack to the target Body
        /// </summary>
        /// <param name="target">The Body to attach this Body to</param>
        void attach(Body* target);
        /// <summary>
        /// Detach this Body from its parent (and attach it to the world root), keeping its global transform as its new transform
        /// if keepWorldTransform is true
        /// </summary>
        /// <param name="body">The Body to detach from this Body</param>
        /// <param name="keepWorldTransform">Whether to keep its global transform as its new transform</param>
        void detachBody(Body* body, const bool keepWorldTransform = true);
        /// <summary>
        /// Detach from a parent (and attack to the world root)
        /// </summary>
        void detach();
        /// <summary>
        /// Detach each child (and attach it to the world root), keeping the child's global transform as its new transform if
        /// keepWorldTransform is true
        /// </summary>
        /// <param name="keepWorldTranform">Whether to keep its global transform as its new transform</param>
        void detachChildren(const bool keepWorldTranform = true);
        /// <summary>
        /// Detach the Body (and don't attach to the world root)
        /// </summary>
        /// <param name="body">The Body to detach</param>
        void dropBody(Body* body);
        /// <summary>
        /// Detach from the parent (and don't attach to the world root)
        /// </summary>
        void drop();
        /// <summary>
        /// Detach the Body and attach it to the target Body
        /// </summary>
        /// <param name="body">The Body to detach</param>
        /// <param name="target">The target to attach the Body to</param>
        void exchangeBody(Body* body, Body* target);
        /// <summary>
        /// Detach from parent and attach to the target Body
        /// </summary>
        /// <param name="target">The Body to attach to</param>
        void exchange(Body* target);
        /// <summary>
        /// Draw the shape hierarchy using their global transforms
        /// </summary>
        /// <param name="target">The RenderTarget</param>
        /// <param name="states">The RenderStates</param>
        void draw(RenderTarget& target, RenderStates states) const;
        /// <summary>
        /// Add the Body to the Renderer to initialize the draw order by hierarchy
        /// </summary>
        /// <param name="target">The RenderTarget</param>
        /// <param name="parentTransform">This Body's parent transform</param>
        void render(RenderTarget& target, const Transform& parentTransform);
        /// <summary>
        /// Add the script to the ScriptDomain, creating the domain if it doesn't already exist
        /// </summary>
        /// <param name="domain">The name of the domain to add the script to</param>
        /// <param name="script">The script to add to the domain</param>
        /// <returns>The unique id of the script within the domain</returns>
        id_t addScript(string domain, Script* script);
        /// <summary>
        /// Add the script to the "start" ScriptDomain to be called when the Body is created or when the world starts
        /// </summary>
        /// <param name="script">The script to add</param>
        /// <returns>The unique id of the script within the domain</returns>
        id_t addStartScript(Script* script);
        /// <summary>
        /// Add the script to the "update" ScriptDomain to be called each update cycle
        /// </summary>
        /// <param name="script">The script to add</param>
        /// <returns>The unique id of the script within the domain</returns>
        id_t addUpdateScript(Script* script);
        /// <summary>
        /// Add the script to the "delete" ScriptDomain to be called when the Body is deleted
        /// </summary>
        /// <param name="script">The script to add</param>
        /// <returns>The unique id of the script within the domain</returns>
        id_t addDeleteScript(Script* script);
        /// <summary>
        /// Add the script to the "intersect" ScriptDomain to be called when the Body's GlobalBounds intersects another Body's GlobalBounds (if that Body has intersecting enabled)
        /// </summary>
        /// <param name="script">The script to add</param>
        /// <returns>The unique id of the script within the domain</returns>
        id_t addIntersectScript(Script* script);
        /// <summary>
        /// Erase and delete the script with the id in the domain.
        /// </summary>
        /// <param name="domain">The domain to erase the script from</param>
        /// <param name="scriptId">The unique id of the script to delete within the domain</param>
        /// <param name="shouldDelete">Whether the domain should be deleted if empty after the erase</param>
        void eraseScript(string domain, id_t scriptId, bool shouldDelete = false);
        /// <summary>
        /// Erase and delete the script with the id in the domain.
        /// </summary>
        /// <param name="domain">The domain to erase the script from</param>
        /// <param name="script">The script to delete</param>
        /// <param name="shouldDelete">Whether the domain should be deleted if empty after the erase</param>
        void eraseScript(string domain, Script* script, bool shouldDelete = false);
        /// <summary>
        /// Erase and delete the script with the id in the "start" domain.
        /// </summary>
        /// <param name="scriptId">The unique id of the script to delete with in the domain</param>
        /// <param name="shouldDelete">Whether the domain should be deleted if empty after the erase</param>
        void eraseStartScript(id_t scriptId, bool shouldDelete = false);
        /// <summary>
        /// Erase and delete the script with the id in the "update" domain.
        /// </summary>
        /// <param name="scriptId">The unique id of the script to delete with in the domain</param>
        /// <param name="shouldDelete">Whether the domain should be deleted if empty after the erase</param>
        void eraseUpdateScript(id_t scriptId, bool shouldDelete = false);
        /// <summary>
        /// Erase and delete the script with the id in the "delete" domain.
        /// </summary>
        /// <param name="scriptId">The unique id of the script to delete with in the domain</param>
        /// <param name="shouldDelete">Whether the domain should be deleted if empty after the erase</param>
        void eraseDeleteScript(id_t scriptId, bool shouldDelete = false);
        void deleteDomain(string domain);
        void clearDomain(string domain);
        /// <summary>
        /// Call each script within the domain, providing the indicated bodies and inheriting return data from predecessors.
        /// </summary>
        /// <param name="domain">The domain of the scripts to call</param>
        /// <param name="bodies">Input Bodies</param>
        /// <param name="predecessor">The parent script to inherit return data from</param>
        void callScripts(string domain);
        /// <summary>
        /// Call each script within the domain, providing the indicated bodies and data.
        /// </summary>
        /// <param name="domain">The domain of the scripts to call</param>
        /// <param name="bodies">Input Bodies</param>
        /// <param name="data">The DataStack to provide as input</param>
        void callScriptsWithData(string domain, DataMap data = DataMap());

        optional<id_t> addOverlapMousePressScript(Script* script, Mouse::Button button = Mouse::Button::Left, optional<id_t> behaviorId = nullopt, bool alwaysAddListener = false);
        optional<id_t> addOverlapMouseReleaseScript(Script* script, Mouse::Button button = Mouse::Button::Left, optional<id_t> behaviorId = nullopt, bool alwaysAddListener = false);
        optional<id_t> addOverlapMouseHoldScript(Script* script, Mouse::Button button = Mouse::Button::Left, optional<id_t> behaviorId = nullopt);
        optional<id_t> addOverlapEnterMouseScript(Script* script, optional<id_t> behaviorId = nullopt);
        optional<id_t> addOverlapExitMouseScript(Script* script, optional<id_t> behaviorId = nullopt);
        optional<id_t> addMousePressScript(ScriptEvent scriptEvt, Mouse::Button button = Mouse::Button::Left, optional<id_t> behaviorId = nullopt);
        optional<id_t> addMouseReleaseScript(ScriptEvent scriptEvt, Mouse::Button button = Mouse::Button::Left, optional<id_t> behaviorId = nullopt);
        optional<id_t> addKeyPressScript(ScriptEvent scriptEvt, Keyboard::Scan key = Keyboard::Scan::Space, optional<id_t> behaviorId = nullopt);
        optional<id_t> addKeyHoldScript(Script* script, Keyboard::Scan key = Keyboard::Scan::Space, optional<id_t> behaviorId = nullopt);
        optional<id_t> addKeyReleaseScript(ScriptEvent scriptEvt, Keyboard::Scan key = Keyboard::Scan::Space, optional<id_t> behaviorId = nullopt);
        optional<id_t> addTextEnteredScript(ScriptEvent scriptEvt, optional<id_t> behaviorId = nullopt);
        optional<id_t> addMouseMovedScript(ScriptEvent scriptEvt, optional<id_t> behaviorId = nullopt);

        void removeOverlapMousePressScript(Script* script, Mouse::Button button, bool shouldDelete = false);
        void removeOverlapMouseReleaseScript(Script* script, Mouse::Button button, bool shouldDelete = false);
        void removeOverlapMousePressScript(id_t scriptId, Mouse::Button button, bool shouldDelete = false);
        void removeOverlapMouseReleaseScript(id_t scriptId, Mouse::Button button, bool shouldDelete = false);
        /// <summary>
        /// Remove the InputActionEvent for the domain and delete it
        /// </summary>
        /// <param name="inputDomain">The domain of the InputActionEvent</param>
        /// <param name="inputActionId">The id of the InputActionEvent to remove</param>
        /// <param name="input"></param>
        void eraseListener(InputCondition inputDomain, id_t inputActionId);
        /// <summary>
        /// Remove the InputActionEvent for the domain without deleting it
        /// </summary>
        /// <param name="inputDomain">The domain of the InputActionEvent</param>
        /// <param name="inputActionId">The id of the InputActionEvent to remove</param>
        /// <param name="input"></param>
        void removeListener(InputCondition inputDomain, id_t inputActionId);

        /// <summary>
        /// Set a timer for duration seconds, calling the onCompleteEvent script when complete and restarting if loopCount is > 0 or < 0
        /// </summary>
        /// <param name="duration">The amount of time to run the timer for</param>
        /// <param name="onCompleteEvent">The script to call when the timer is complete</param>
        /// <param name="loopCount">The number of loops. -1 causes infinite looping</param>
        /// <param name="timerDisplayName">The optional display name of the timer</param>
        /// <returns></returns>
        timerId_t setTimer(sec_t duration, Script* onCompleteEvent, int loopCount = 0, string timerDisplayName = "");
        /// <summary>
        /// Cancel the timer with this id
        /// </summary>
        /// <param name="timerId">The id of the timer to cancel</param>
        void cancelTimer(size_t timerId);
        void cancelTimer(timerId_t* timerId);
        /// <summary>
        /// Bodies with greater Z-Order are drawn in front of objects with lower Z-Order. Without modifying Z-Order, children are drawn
        /// in front of their parents
        /// </summary>
        int zOrder = 0;
        /// <summary>
        /// The amount of time between calling scripts in the "update" domain for this Body
        /// </summary>
        sec_t scriptUpdateInterval = -1;

        id_t addBehavior(Behavior* behavior);
        void removeBehavior(id_t behaviorId);
        Behavior* getBehavior(id_t behaviorId);
    protected:
        /// <summary>
        /// Base Body initialization with a display name, taking a unique ID from world's body IDs stack, and assigning itself to the ScriptMap's owner pointer.
        /// </summary>
        /// <param name="name">Optional body display name</param>
        /// <param name="isWorldRoot">Whether this is the world root body or not</param>
        Body(string displayName = "");
    private:
        friend class World;
        friend class Scene;
        friend class Renderer;
        friend class InputMap;
        /// <summary>
        /// The unique id of the Body provided by the world
        /// </summary>
        optional<size_t> bodyId = nullopt;
        /// <summary>
        /// This Body's parameters
        /// </summary>
        BodyParameters bodyParams;
        /// <summary>
        /// The assigned Transformable entity
        /// </summary>
        Transformable* entity = nullptr;

        UniqueDomain<id_t, Behavior*> behaviors = UniqueDomain<id_t, Behavior*>(1000);
        /// <summary>
        /// The Body whose transform is the parent of this Body's transform
        /// </summary>
        Body* parent = nullptr;
        /// <summary>
        /// The Bodies that are attached to (and inherit the Transform of) this Body
        /// </summary>
        vector<Body*> children;
        /// <summary>
        /// The RectangleShape of this body's bounds
        /// </summary>
        RectangleShape* boundsRect = nullptr;
        /// <summary>
        /// Private constructor meant for the world to create the world root Body (if isWorldRoot is true).
        /// </summary>
        /// <param name="isWorldRoot">If true, the Body (the world root) will have a nullopt id</param>
        Body(bool isWorldRoot);
        /// <summary>
        /// Base Body start function which calls assigned OnStartEvent ("start") domain scripts
        /// </summary>
        void start();
        /// <summary>
        /// Create and initialize the bounds RectangleShape
        /// </summary>
        void createBoundsRect();
        void updateBoundsRect();
        sec_t lastUpdateTime = 0;
        /// <summary>
        /// The TimerMap for a Body holds references to each of its Timers by id
        /// </summary>
        TimerMap timers;
        /// <summary>
        /// The ScriptMap for a Body holds references to each of its ScriptDomains and their assigned Scripts by id
        /// </summary>
        ScriptMap scripts;
        optional<id_t> intersectScriptId;
        /// <summary>
        /// The map of listener ids by domain for the Body so they may be erased when the Body is deleted
        /// </summary>
        map<InputCondition, vector<id_t>> listenerIds;
        /// <summary>
        /// Add the listener id so that it can be erased when the Body is deleted
        /// </summary>
        /// <param name="domainName">The domain of the listener</param>
        /// <param name="listenerId">The listener id to add</param>
        void addListenerId(InputCondition domainName, id_t listenerId);
        /// <summary>
        /// Remove the listener id for the domain
        /// </summary>
        /// <param name="domainName">The domain of the listener</param>
        /// <param name="listenerId">The listener id to remove</param>
        void removeListenerId(InputCondition domainName, id_t listenerId);
        /// <summary>
        /// Whether the start function for the newly created Body has been called or not
        /// </summary>
        bool initialized = false;
        /// <summary>
        /// Delete the Body and, based on the setting, either orphan, inherit, or terminate its children
        /// </summary>
        /// <param name="inheritChildren">Whether the Body's children should be orphaned, inherited, or terminated</param>
        /// <returns>The nulled reference</returns>
        Body* deleteBody(ChildrenTermination inheritChildren = ChildrenTermination::Orphan);
        //Draw the assigned Drawable shape to target with the provided transform
        void onDraw(RenderTarget& target, const Transform& transform) const;

        ScriptEvent MouseOverlapReleaseEvent = [](ScArgs args) {
            //Get the mouse release event from the script input data
            MouseReleaseInput* evt = args.script->getInput().getDataPtr<MouseReleaseInput>("evt");
            if (evt == nullptr) return;

            //If the caller's bounds contains the mouse position (converted from View Space)
            if (args.caller->contains(args.caller->viewToGlobal(evt->position))) {
                //Call any mouseRelease+button domain scripts with the mouse release event as input
                args.caller->scripts.callDomainWithData("mouseRelease_" + to_string((int)evt->button), nullptr, DataMap(map<string, any>({ {"evt",evt} })));
            }
        };
        ScriptEvent MouseOverlapPressEvent = [](ScArgs args) {
            //Get the mouse press event from the script input data
            MousePressInput* evt = args.script->getInput().getDataPtr<MousePressInput>("evt");
            if (evt == nullptr) return;

            //If the caller's bounds contains the mouse position (converted from View Space)
            if (args.caller->contains(args.caller->viewToGlobal(evt->position))) {
                //Call any mousePress+button domain scripts with the mouse press event as input
                args.caller->scripts.callDomainWithData("mousePress_" + to_string((int)evt->button), nullptr, DataMap(map<string, any>({ {"evt",evt} })));
            }
        };
        ScriptEvent MouseOverlapHoldReleasedEvent = [](ScArgs args) {
            //Get the mouse release event from the script input data
            MouseReleaseInput* evt = args.script->getInput().getDataPtr<MouseReleaseInput>("evt");
            if (evt == nullptr) return;

            //If the caller has a mouseOverlapHold update script id assigned
            if (args.caller->mouseOverlapHoldUpdateId.has_value()) {
                //Erase the mouseOverlapHold update script
                args.caller->eraseUpdateScript(args.caller->mouseOverlapHoldUpdateId.value(), true);
                //Remove this key release actuator
                args.caller->removeListener(InputCondition((int)evt->button, InputType::Button, InputState::Released), args.script->id.value());
                //Add a key press actuator
                args.caller->addMousePressScript(args.caller->MouseOverlapHoldPressEvent);
            }
        };
        ScriptEvent MouseOverlapHoldPressEvent = [](ScArgs args) {
            //Get the mouse press event from the scipt input data
            MousePressInput* evt = args.script->getInput().getDataPtr<MousePressInput>("evt");
            if (evt == nullptr) return;

            //If the caller's bounds contains the mouse position (converted from View Space)
            if (args.caller->contains(args.caller->viewToGlobal(evt->position))) {
                Mouse::Button button = evt->button;

                //Add an update script to call mouseHold+button scripts each tick
                args.caller->mouseOverlapHoldUpdateId = args.caller->addUpdateScript(new Script([button](ScArgs args) {
                    //Call any mouseHold+button scripts each tick
                    args.caller->scripts.callDomain("mouseHold_" + to_string((int)button), {});
                }));

                //Remove this key press actuator
                args.caller->removeListener(InputCondition((int)button, InputType::Button, InputState::Pressed), args.script->id.value());
                //Add a key release actuator
                args.caller->addMouseReleaseScript(args.caller->MouseOverlapHoldReleasedEvent);
            }
        };
        optional<id_t> mouseOverlapHoldUpdateId = nullopt;
        

        ScriptEvent KeyHoldReleasedEvent = [](ScArgs args) {
            //Get the key released event from the script input data
            KeyReleaseInput* evt = args.script->getInput().getDataPtr<KeyReleaseInput>("evt");
            if(evt == nullptr) return;
            Keyboard::Scan key = evt->scancode;

            //When the key is released, find the id of the update script for this key hold
            auto iterator = args.caller->keyHoldUpdateIds.find(key);
            if (iterator != args.caller->keyHoldUpdateIds.end()) {
                optional<id_t> updateId = (*iterator).second;
                if (updateId.has_value()) {
                    //Erase the key hold update script
                    args.caller->eraseUpdateScript(updateId.value(), true);
                    //Remove this key release actuator
                    args.caller->removeListener(InputCondition((int)key, InputType::Key, InputState::Released), args.script->id.value());
                    //Add a key press actuator
                    args.caller->addKeyPressScript(args.caller->KeyHoldPressedEvent, key);
                }
            }
        };

        ScriptEvent KeyHoldPressedEvent = [this](ScArgs args) {
            //Get the key press event from the script input data
            KeyPressInput* evt = args.script->getInput().getDataPtr<KeyPressInput>("evt");
            if (evt == nullptr) return;
            Keyboard::Scan key = evt->scancode;

            //When the key is pressed, add an update script to call keyHold+key domain scripts each tick
            args.caller->keyHoldUpdateIds[key] = args.caller->addUpdateScript(new Script([key](ScArgs args) {
                //Call the keyHold+key script each tick
                args.caller->scripts.callDomain("keyHold_" + to_string((int)key), {});
            }));

            //Remove this key press actuator
            args.caller->removeListener(InputCondition((int)key, InputType::Key, InputState::Pressed), args.script->id.value());
            //Add a key release actuator
            args.caller->addKeyReleaseScript(args.caller->KeyHoldReleasedEvent, key);
        };
        map<Keyboard::Scan,optional<id_t>> keyHoldUpdateIds;

        ScriptEvent MouseOverlapEnterEvent = [](ScArgs args) {
            //Get the mouse position from the script input data
            Vector2i pos = args.script->getInput().getData<Vector2i>("evt");

            //If the mouse position (converted from View Space) is contained by the caller's bounds
            if (args.caller->contains(args.caller->viewToGlobal(pos))) {
                //Call any mouseEnter domain scripts with the mouse position as input data
                args.caller->scripts.callDomainWithData("mouseEnter", nullptr, DataMap(map<string, any>({ {"evt",pos} })));
            }
        };

        ScriptEvent MouseOverlapExitEvent = [](ScArgs args) {
            //Get the mouse position from the script input data
            Vector2i pos = args.script->getInput().getData<Vector2i>("evt");
            
            //If the mouse position (converted from View Space) is contained by the caller's bounds
            if (!args.caller->contains(args.caller->viewToGlobal(pos))) {
                //Call any mouseExit domain scripts with the mouse position as input data
                args.caller->scripts.callDomainWithData("mouseExit", nullptr, DataMap(map<string, any>({ {"evt",pos} })));
            }
        };
        /// <summary>
        /// The update Script that checks for intersections and calls intersect domain scripts
        /// </summary>
        static ScriptEvent onIntersectScript;
    };
}