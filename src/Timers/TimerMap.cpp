#include "TimerMap.h"
#include "../World/WorldInstance.h"

namespace CGEngine {
    timerId_t TimerMap::setTimer(Body* body, sec_t duration, Script* onCompleteEvent, int loopCount, string timerDisplayName) {
        if (body == nullptr) return nullopt;
        if (duration <= 0) {
            if (logging.willLog(LogLevel::LogWarn)) {
                string namePrompt = (body->getName() != "") ? "(" + body->getName() + ")" : "";
                string idPrompt = body->getId().has_value() ? "[" + to_string(body->getId().value()) + "]" : "";
                logging(LogLevel::LogWarn, "Body" + idPrompt + namePrompt, "Timer not set. Duration must be > 0 but it was [" + to_string(duration) + "]");
            }
            return nullopt;
        }

        //Get world time
        sec_t expiration = time.getElapsedSec() + duration;
        //Create new timer
        Timer* timer = new Timer(timerDisplayName);
        //Take a unique id for the timer and assign it to the timer
        id_t id = timers.add(timer);
        timer->id = id;
        log(timer->name, id, "SET(" + to_string(duration) + " sec)");
        //Get the timer domain name by its timer id
        string timerDomain = "timer" + to_string(id);
        //Add the onComplete event to the body's timer domain by timer id
        id_t onCompleteEventId = body->addScript(timerDomain, onCompleteEvent);
        //Loop duration is used to check if this timer should loop as well as for setting the next loop duration
        sec_t loopDuration = loopCount != 0 ? duration : 0;
        //Add th timer update script to this body's update scripts
        timer->eventId = body->addUpdateScript(new Script([this, id, expiration, loopDuration, loopCount, onCompleteEvent](ScArgs args) {
            //OnUpdate: Check if the world time >= expiration time
            if (time.getElapsedSec() >= expiration) {
                //LOGGING
                log(timers.get(id)->name, id, "DONE");
                //Get the timer domain name by its timer id
                string timerDomainById = "timer" + to_string(id);
                string timerName = timers.get(id)->name;
                //Call all the scripts with this timer's domain by its id
                args.caller->callScripts(timerDomainById);
                //Check if the caller was deleted by the timer domain scripts
                bool deleted = world->isDeleted(args.caller);
                //If it was not deleted
                if (!deleted) {
                    if ((loopCount == 0 || loopCount == 1) || loopDuration <= 0) {
                        //If not looping, delete the domain and its scripts
                        args.caller->deleteDomain(timerDomainById);
                    }
                    else {
                        //If looping, clear the domain without deleting the scripts
                        args.caller->clearDomain(timerDomainById);
                    }
                    //Delete the timer
                    size_t updateEventId = timers.get(id)->eventId;
                    deleteTimer(id);
                    //Start the next loop, if looping
                    if ((loopCount < 0 || loopCount > 1) && loopDuration > 0) {
                        setTimer(args.caller, loopDuration, onCompleteEvent, loopCount > 0 ? loopCount - 1 : loopCount, timerName);
                    }
                    //Delete this timer's update script
                    args.caller->eraseUpdateScript(updateEventId, false);
                }
            }
        }));
        //LOGGING
        log(timer->name, id, "START(" + to_string(duration) + " sec)");
        return id;
    }

    void TimerMap::cancelTimer(Body* body, size_t timerId) {
        Timer* timer = timers.get(timerId);
        //LOGGING
        log(timer->name, timerId, "STOP");
        //Delete the domain for the timer id
        body->deleteDomain("timer" + to_string(timerId));
        //Remove this timer's update script
        body->eraseUpdateScript(timer->eventId);
        //Refund the timer id, delete the timer and erase it from the timer map
        timers.remove(timerId);
        delete timer;
    }

    void TimerMap::cancelTimer(Body* body, timerId_t* timerId) {
        if (timerId->has_value()) {
            id_t id = timerId->value();
            Timer* timer = timers.get(id);
            //LOGGING
            log(timer->name, id, "STOP");
            //Delete the domain for the timer id
            body->deleteDomain("timer" + to_string(id));
            //Remove this timer's update script
            body->eraseUpdateScript(timer->eventId);
            //Refund the timer id, delete the timer and erase it from the timer map and set the timerId optional to nullopt
            timers.remove(id);
            delete timer;
            *timerId = nullopt;
        }
    }

    void TimerMap::deleteTimers(Body* body) {
        timers.forEach([this, &body](Timer* timer) {
            if (logging.willLog(logLevel)) {
                string namePrompt = (body->getName() != "") ? "(" + body->getName() + ")" : "";
                string idPrompt = body->getId().has_value() ? "[" + to_string(body->getId().value()) + "]" : "";
                logging(logLevel, "Body" + idPrompt + namePrompt, "Deleting Timer(" + timer->name + ")");
            }
            delete timer;
        });
    }

    void TimerMap::deleteTimer(size_t timerId) {
        Timer* timer = timers.get(timerId);
        timers.remove(timerId);
        delete timer;
    }

    void TimerMap::log(string timerName, size_t timerId, string msg) {
        if (!logging.willLog(logLevel)) return;

        string name = (timerName != "") ? "(" + timerName + ")" : "";
        string caller = "Timer[" + to_string(timerId) + "]" + name;
        logging(logLevel, caller, msg);
    }

    void TimerMap::clear() {
        timers.clear();
    }
}