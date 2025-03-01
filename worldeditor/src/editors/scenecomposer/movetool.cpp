#include "movetool.h"

#include <components/actor.h>
#include <components/transform.h>

#include <editor/viewport/handles.h>

#include "objectctrl.h"
#include "undomanager.h"

MoveTool::MoveTool(ObjectCtrl *controller, SelectList &selection) :
    SelectTool(controller, selection) {

}

void MoveTool::update(bool pivot, bool local, float snap) {
    bool isDrag = m_pController->isDrag();

    Transform *t = m_Selected.back().object->transform();

    m_World = Handles::moveTool(objectPosition(), local ? t->worldQuaternion() : Quaternion(), isDrag);
    if(isDrag) {
        Vector3 delta(m_World - m_SavedWorld);
        if(snap > 0.0f) {
            for(int32_t i = 0; i < 3; i++) {
                delta[i] = snap * int(delta[i] / snap);
            }
        }
        for(const auto &it : m_Selected) {
            Vector3 dt(local ? t->worldQuaternion() * delta : delta);
            Actor *a = dynamic_cast<Actor *>(it.object->parent());
            if(!local && a && a->transform()) {
                dt = a->transform()->worldTransform().rotation().inverse() * delta;
            }
            if(it.object->transform()) {
                it.object->transform()->setPosition(it.position + dt);
            }
        }
        m_pController->objectsUpdated();
        m_pController->objectsChanged(m_pController->selected(), "Position");
    }
}

QString MoveTool::icon() const {
    return ":/Images/editor/Move.png";
}

QString MoveTool::name() const {
    return "Move";
}

QString MoveTool::toolTip() const {
    return QObject::tr("Select and Translate objects");
}

QString MoveTool::shortcut() const {
    return "Shift+T";
}
