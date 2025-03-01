#include "objectctrl.h"

#include <QMessageBox>
#include <QMimeData>
#include <QMenu>
#include <QDebug>

#include <components/actor.h>
#include <components/transform.h>
#include <components/camera.h>

#include <editor/viewport/handles.h>
#include <editor/viewport/editorpipeline.h>

#include "selecttool.h"
#include "movetool.h"
#include "rotatetool.h"
#include "scaletool.h"
#include "resizetool.h"

#include "config.h"

#include "assetmanager.h"
#include "projectmanager.h"
#include <editor/settingsmanager.h>

namespace  {
    static const char *gBackgroundColor("General/Colors/Background_Color");
    static const char *gIsolationColor("General/Colors/Isolation_Color");
}

string findFreeObjectName(const string &name, Object *parent) {
    string newName = name;
    if(!newName.empty()) {
        Object *o = parent->find(parent->name() + "/" + newName);
        if(o != nullptr) {
            string number;
            while(isdigit(newName.back())) {
                number.insert(0, 1, newName.back());
                newName.pop_back();
            }
            int32_t i = atoi(number.c_str());
            i++;
            while(parent->find(parent->name() + "/" + newName + to_string(i)) != nullptr) {
                i++;
            }
            return (newName + to_string(i));
        }
        return newName;
    }
    return "Object";
}

ObjectCtrl::ObjectCtrl(QWidget *view) :
        CameraCtrl(view),
        m_pipeline(nullptr),
        m_isolatedActor(nullptr),
        m_activeTool(nullptr),
        m_menu(nullptr),
        m_axes(0),
        m_isolatedActorModified(false),
        m_drag(false),
        m_canceled(false),
        m_local(false) {

    connect(view, SIGNAL(drop(QDropEvent*)), this, SLOT(onDrop()));
    connect(view, SIGNAL(dragEnter(QDragEnterEvent*)), this, SLOT(onDragEnter(QDragEnterEvent*)));
    connect(view, SIGNAL(dragMove(QDragMoveEvent*)), this, SLOT(onDragMove(QDragMoveEvent*)));
    connect(view, SIGNAL(dragLeave(QDragLeaveEvent*)), this, SLOT(onDragLeave(QDragLeaveEvent*)));

    connect(SettingsManager::instance(), &SettingsManager::updated, this, &ObjectCtrl::onApplySettings);
    connect(AssetManager::instance(), &AssetManager::prefabCreated, this, &ObjectCtrl::onPrefabCreated);
    connect(this, &ObjectCtrl::mapUpdated, this, &ObjectCtrl::onUpdated);
    connect(this, &ObjectCtrl::objectsUpdated, this, &ObjectCtrl::onUpdated);

    SettingsManager::instance()->registerProperty(gBackgroundColor, QColor(51, 51, 51, 0));
    SettingsManager::instance()->registerProperty(gIsolationColor, QColor(0, 76, 140, 0));

    m_tools = {
        new SelectTool(this, m_selected),
        new MoveTool(this, m_selected),
        new RotateTool(this, m_selected),
        new ScaleTool(this, m_selected),
        new ResizeTool(this, m_selected),
    };
}

ObjectCtrl::~ObjectCtrl() {
    delete m_pipeline;
}

void ObjectCtrl::init() {
    m_pipeline = new EditorPipeline;
    m_pipeline->setController(this);
    m_pipeline->createMenu(m_menu);
    m_activeCamera->setPipeline(m_pipeline);
}

void ObjectCtrl::drawHandles() {
    Vector2 position, size;
    selectGeometry(position, size);

    Vector3 screen = Vector3(position.x / m_screenSize.x, position.y / m_screenSize.y, 0.0f);
    Handles::s_Mouse = Vector2(screen.x, screen.y);
    Handles::s_Screen = m_screenSize;

    if(!m_drag) {
        Handles::s_Axes = m_axes;
    }

    m_objectsList.clear();
    if(m_isolatedActor) {
        drawHelpers(*m_isolatedActor);
    } else {
        for(auto it : m_editObjects) {
            drawHelpers(*it.first);
        }
    }

    Handles::cleanDepth();

    if(!m_selected.empty()) {
        if(m_activeTool) {
            m_activeTool->update(false, m_local, 0.0f);
        }
    }

    if(m_pipeline) {
        uint32_t result = 0;
        if(position.x >= 0.0f && position.y >= 0.0f &&
           position.x < m_screenSize.x && position.y < m_screenSize.y) {

            m_pipeline->setMousePosition(int32_t(position.x), int32_t(position.y));
            result = m_pipeline->objectId();
        }

        if(result) {
            if(m_objectsList.empty()) {
                m_objectsList = { result };
            }
        }
        m_mouseWorld = m_pipeline->mouseWorld();
    }
}

void ObjectCtrl::clear(bool signal) {
    m_selected.clear();
    if(signal) {
        emit objectsSelected(selected());
    }
}

void ObjectCtrl::drawHelpers(Object &object) {
    Object::ObjectList list;
    for(auto &it : m_selected) {
        list.push_back(it.object);
    }

    for(auto &it : object.getChildren()) {
        Component *component = dynamic_cast<Component *>(it);
        if(component) {
            if(component->drawHandles(list)) {
                m_objectsList = {object.uuid()};
            }
        } else {
            if(it) {
                drawHelpers(*it);
            }
        }
    }
}

void ObjectCtrl::selectGeometry(Vector2 &pos, Vector2 &size) {
    pos = Vector2(m_mousePosition.x, m_mousePosition.y);
    size = Vector2(1, 1);
}

void ObjectCtrl::setDrag(bool drag) {
    if(drag && m_activeTool) {
        m_activeTool->beginControl();
    }
    m_drag = drag;
}

void ObjectCtrl::onApplySettings() {
    if(m_activeCamera) {
        QColor color = SettingsManager::instance()->property(m_isolatedActor ? gIsolationColor : gBackgroundColor).value<QColor>();
        m_activeCamera->setColor(Vector4(color.redF(), color.greenF(), color.blueF(), color.alphaF()));
    }
}

void ObjectCtrl::onPrefabCreated(uint32_t uuid, uint32_t clone) {
    bool swapped = false;
    for(auto &it : m_selected) {
        if(it.object->uuid() == uuid) {
            Object *object = findObject(clone);
            if(object) {
                it.object = static_cast<Actor *>(object);
                swapped = true;
                break;
            }
        }
    }
    if(swapped) {
        emit objectsSelected(selected());
        emit mapUpdated();
    }
}

Object::ObjectList ObjectCtrl::selected() {
    Object::ObjectList result;
    for(auto &it : m_selected) {
        if(it.object) {
            result.push_back(it.object);
        }
    }
    return result;
}

list<pair<Object *, bool> > ObjectCtrl::objects() const {
    return m_editObjects;
}

void ObjectCtrl::addObject(Object *object) {
    m_editObjects.push_back(make_pair(object, false));
}

void ObjectCtrl::setObject(Object *object) {
    clear();
    for(auto it : m_editObjects) {
        delete it.first;
    }
    m_editObjects.clear();
    if(object) {
        m_editObjects.push_back(make_pair(object, false));
    }
}

void ObjectCtrl::setIsolatedActor(Actor *actor) {
    m_isolatedActor = actor;
    if(actor) {
        m_isolatedActorModified = false;
        m_isolationSelectedBackup = selected();
        onSelectActor({actor});
    } else {
        std::list<uint32_t> local;
        for(auto it : m_isolationSelectedBackup) {
            local.push_back(it->uuid());
        }
        clear(false);
        selectActors(local);
    }

    QColor color;
    if(m_isolatedActor) {
        color = SettingsManager::instance()->property(gIsolationColor).value<QColor>();
    } else {
        color = SettingsManager::instance()->property(gBackgroundColor).value<QColor>();
    }
    m_activeCamera->setColor(Vector4(color.redF(), color.greenF(), color.blueF(), color.alphaF()));
}

void ObjectCtrl::selectActors(const list<uint32_t> &list) {
    for(auto it : list) {
        Actor *actor = static_cast<Actor *>(findObject(it));
        if(actor) {
            EditorTool::Select data;
            data.object = actor;
            m_selected.push_back(data);
        }
    }
    emit objectsSelected(selected());
}

void ObjectCtrl::onSelectActor(const list<uint32_t> &list, bool additive) {
    std::list<uint32_t> local = list;
    if(additive) {
        for(auto &it : m_selected) {
            local.push_back(it.object->uuid());
        }
    }
    UndoManager::instance()->push(new SelectObjects(local, this));
}

void ObjectCtrl::onSelectActor(Object::ObjectList list, bool additive) {
    std::list<uint32_t> local;
    for(auto it : list) {
        local.push_back(it->uuid());
    }
    onSelectActor(local, additive);
}

void ObjectCtrl::onRemoveActor(Object::ObjectList list) {
    UndoManager::instance()->push(new DeleteActors(list, this));
}

void ObjectCtrl::onParentActor(Object::ObjectList objects, Object *parent) {
    UndoManager::instance()->push(new ParentingObjects(objects, parent, this));
}

void ObjectCtrl::onPropertyChanged(Object::ObjectList objects, const QString &property, const Variant &value) {
    UndoManager::instance()->push(new PropertyObject(objects.front(), property, value, this));
}

void ObjectCtrl::onFocusActor(Object *object) {
    float bottom;
    setFocusOn(dynamic_cast<Actor *>(object), bottom);
}

void ObjectCtrl::onChangeTool() {
    QString name(sender()->objectName());
    for(auto &it : m_tools) {
        if(it->name() == name) {
            m_activeTool = it;
            break;
        }
    }
}

void ObjectCtrl::onUpdated() {
    if(m_isolatedActor) {
        m_isolatedActorModified = true;
    } else {
        if(!m_editObjects.empty()) {
            m_editObjects.front().second = true;
        }
    }
}

void ObjectCtrl::onLocal(bool flag) {
    m_local = flag;
}

void ObjectCtrl::onPivot(bool flag) {

}

void ObjectCtrl::onCreateComponent(const QString &name) {
    if(m_selected.size() == 1) {
        Actor *actor = m_selected.begin()->object;
        if(actor) {
            if(actor->component(qPrintable(name)) == nullptr) {
                UndoManager::instance()->push(new CreateObject(name, this));
            } else {
                QMessageBox msgBox;
                msgBox.setIcon(QMessageBox::Warning);
                msgBox.setText(tr("Creation Component Failed"));
                msgBox.setInformativeText(QString(tr("Component with type \"%1\" already defined for this actor.")).arg(name));
                msgBox.setStandardButtons(QMessageBox::Ok);

                msgBox.exec();
            }
        }
    }
}

void ObjectCtrl::onDeleteComponent(const QString &name) {
    if(!name.isEmpty()) {
        Actor *actor = m_selected.begin()->object;
        if(actor) {
            Component *obj = actor->component(name.toStdString());
            if(obj) {
                UndoManager::instance()->push(new RemoveComponent(obj, this));
            }
        }
    }
}

void ObjectCtrl::onUpdateSelected() {
    emit objectsSelected(selected());
}

void ObjectCtrl::onDrop() {
    if(!m_dragObjects.empty()) {
        for(auto it : m_dragObjects) {
            it->setParent(m_isolatedActor ? m_isolatedActor : m_editObjects.front().first);
        }
        if(m_pipeline) {
            m_pipeline->setDragObjects({});
        }
        UndoManager::instance()->push(new CreateObjectSerial(m_dragObjects, this));
    }

    if(!m_dragMap.name.isEmpty()) {
        emit dropMap(ProjectManager::instance()->contentPath() + "/" + m_dragMap.name, m_dragMap.additive);
        m_dragMap.name.clear();
    }
}

void ObjectCtrl::onDragEnter(QDragEnterEvent *event) {
    m_dragObjects.clear();
    m_dragMap.name.clear();

    if(event->mimeData()->hasFormat(gMimeComponent)) {
        string name = event->mimeData()->data(gMimeComponent).toStdString();
        Actor *actor = Engine::composeActor(name, findFreeObjectName(name, m_editObjects.front().first));
        if(actor) {
            actor->transform()->setPosition(Vector3(0.0f));
            m_dragObjects.push_back(actor);
        }
        event->acceptProposedAction();
    } else if(event->mimeData()->hasFormat(gMimeContent)) {
        event->acceptProposedAction();

        QStringList list = QString(event->mimeData()->data(gMimeContent)).split(";");
        AssetManager *mgr = AssetManager::instance();
        foreach(QString str, list) {
            if(!str.isEmpty()) {
                QFileInfo info(str);
                QString type = mgr->assetTypeName(info);
                if(type == "Map") {
                    m_dragMap.name = str;
                    m_dragMap.additive = (event->keyboardModifiers() & Qt::ControlModifier);
                } else {
                    Actor *actor = mgr->createActor(str);
                    if(actor) {
                        actor->setName(findFreeObjectName(info.baseName().toStdString(), m_editObjects.front().first));
                        m_dragObjects.push_back(actor);
                    }
                }
            }
        }
    }
    for(Object *o : m_dragObjects) {
        Actor *a = static_cast<Actor *>(o);
        a->transform()->setPosition(m_mouseWorld);
    }

    if(m_pipeline) {
        m_pipeline->setDragObjects(m_dragObjects);
    }

    if(!m_dragObjects.empty() || !m_dragMap.name.isEmpty()) {
        return;
    }

    event->ignore();
}

void ObjectCtrl::onDragMove(QDragMoveEvent *e) {
    m_mousePosition = Vector2(e->pos().x(), m_screenSize.y - e->pos().y());

    for(Object *o : m_dragObjects) {
        Actor *a = static_cast<Actor *>(o);
        a->transform()->setPosition(m_mouseWorld);
    }
}

void ObjectCtrl::onDragLeave(QDragLeaveEvent * /*event*/) {
    if(m_pipeline) {
        m_pipeline->setDragObjects({});
    }
    for(Object *o : m_dragObjects) {
        delete o;
    }
    m_dragObjects.clear();
}

void ObjectCtrl::onInputEvent(QInputEvent *pe) {
    CameraCtrl::onInputEvent(pe);
    switch(pe->type()) {
        case QEvent::KeyPress: {
            QKeyEvent *e = static_cast<QKeyEvent *>(pe);
            switch(e->key()) {
                case Qt::Key_Delete: {
                    onRemoveActor(selected());
                } break;
                default: break;
            }
        } break;
        case QEvent::MouseButtonPress: {
            QMouseEvent *e = static_cast<QMouseEvent *>(pe);
            if(e->buttons() & Qt::LeftButton) {
                if(Handles::s_Axes) {
                    m_axes = Handles::s_Axes;
                }
                if(m_drag) {
                    if(m_activeTool) {
                        m_activeTool->cancelControl();
                    }

                    setDrag(false);
                    m_canceled = true;
                    emit objectsUpdated();
                }
            }
        } break;
        case QEvent::MouseButtonRelease: {
            QMouseEvent *e = static_cast<QMouseEvent *>(pe);
            if(e->button() == Qt::LeftButton) {
                if(!m_drag) {
                    if(!m_canceled) {
                        onSelectActor(m_objectsList, e->modifiers() & Qt::ControlModifier);
                    } else {
                        m_canceled = false;
                    }
                } else {
                    if(m_activeTool) {
                        m_activeTool->endControl();

                        QUndoCommand *group = new QUndoCommand(m_activeTool->name());

                        bool valid = false;
                        auto cache = m_activeTool->cache().begin();
                        for(auto &it : m_selected) {
                            VariantMap components = (*cache).toMap();
                            for(auto &child : it.object->getChildren()) {
                                Component *component = dynamic_cast<Component *>(child);
                                if(component) {
                                    VariantMap properties = components[to_string(component->uuid())].toMap();
                                    const MetaObject *meta = component->metaObject();
                                    for(int i = 0; i < meta->propertyCount(); i++) {
                                        MetaProperty property = meta->property(i);

                                        Variant value = property.read(component);
                                        Variant data = properties[property.name()];
                                        if(value != data) {
                                            property.write(component, data);

                                            new PropertyObject(component, property.name(), value, this, "", group);
                                            valid = true;
                                        }
                                    }
                                }
                            }

                            ++cache;
                        }

                        if(!valid) {
                            delete group;
                        } else {
                            UndoManager::instance()->push(group);
                        }
                    }
                }
                setDrag(false);
            }
        } break;
        case QEvent::MouseMove: {
            QMouseEvent *e = static_cast<QMouseEvent *>(pe);
            m_mousePosition = Vector2(e->pos().x(), m_screenSize.y - e->pos().y());

            if(e->buttons() & Qt::LeftButton) {
                if(!m_drag) {
                    if(e->modifiers() & Qt::ShiftModifier) {
                        UndoManager::instance()->push(new DuplicateObjects(this));
                    }
                    setDrag(Handles::s_Axes);
                }
            } else {
                setDrag(false);
            }

            if(m_activeTool->cursor() != Qt::ArrowCursor) {
                emit setCursor(QCursor(m_activeTool->cursor()));
            } else if(!m_objectsList.empty()) {
                emit setCursor(QCursor(Qt::CrossCursor));
            } else {
                emit unsetCursor();
            }
        } break;
        default: break;
    }
}

Object *ObjectCtrl::findObject(uint32_t id, Object *parent) {
    Object *result = nullptr;

    if(m_isolatedActor) {
        Object *p = parent;
        if(p == nullptr) {
            p = m_isolatedActor;
        }
        result = ObjectSystem::findObject(id, p);
    } else {
        for(auto it : m_editObjects) {
            Object *p = parent;
            if(p == nullptr) {
                p = it.first;
            }
            result = ObjectSystem::findObject(id, p);
            if(result) {
                break;
            }
        }
    }

    return result;
}

bool ObjectCtrl::isModified() const {
    if(m_isolatedActor) {
        return m_isolatedActorModified;
    } else {
        if(!m_editObjects.empty()) {
            return m_editObjects.front().second;
        }
    }
    return false;
}

void ObjectCtrl::resetModified() {
    if(m_isolatedActor) {
        m_isolatedActorModified = false;
    } else {
        if(!m_editObjects.empty()) {
            m_editObjects.front().second = false;
        }
    }
}

void ObjectCtrl::resetSelection() {
    for(auto &it : m_selected) {
        it.renderable = nullptr;
    }
}

void ObjectCtrl::createMenu(QMenu *menu) {
    CameraCtrl::createMenu(menu);
    menu->addSeparator();
    m_menu = menu;
}

SelectObjects::SelectObjects(const list<uint32_t> &objects, ObjectCtrl *ctrl, const QString &name, QUndoCommand *group) :
        UndoObject(ctrl, name, group),
        m_objects(objects) {

}
void SelectObjects::undo() {
    SelectObjects::redo();
}
void SelectObjects::redo() {
    Object::ObjectList objects = m_controller->selected();

    m_controller->clear(false);
    m_controller->selectActors(m_objects);

    m_objects.clear();
    for(auto it : objects) {
        m_objects.push_back(it->uuid());
    }
}

CreateObject::CreateObject(const QString &type, ObjectCtrl *ctrl, QUndoCommand *group) :
        UndoObject(ctrl, QObject::tr("Create %1").arg(type), group),
        m_type(type) {

}
void CreateObject::undo() {
    for(auto uuid : m_objects) {
        Object *object = m_controller->findObject(uuid);
        if(object) {
            delete object;
        }
    }
    emit m_controller->objectsUpdated();
    emit m_controller->objectsSelected(m_controller->selected());
}
void CreateObject::redo() {
    auto list = m_controller->selected();
    if(list.empty()) {
        list.push_back(m_controller->objects().front().first);
    }

    for(auto &it : list) {
        Object *object;
        if(m_type == "Actor") {
            object = Engine::composeActor("", qPrintable(m_type), it);
        } else {
            object = Engine::objectCreate(qPrintable(m_type), qPrintable(m_type), it);
        }
        m_objects.push_back(object->uuid());
    }

    emit m_controller->objectsUpdated();
    emit m_controller->objectsSelected(m_controller->selected());
}

DuplicateObjects::DuplicateObjects(ObjectCtrl *ctrl, const QString &name, QUndoCommand *group) :
        UndoObject(ctrl, name, group) {

}
void DuplicateObjects::undo() {
    m_dump.clear();
    for(auto it : m_objects) {
        Object *object = m_controller->findObject(it);
        if(object) {
            m_dump.push_back(ObjectSystem::toVariant(object));
            delete object;
        }
    }
    m_objects.clear();
    emit m_controller->mapUpdated();

    m_controller->clear(false);
    m_controller->selectActors(m_selected);
}
void DuplicateObjects::redo() {
    if(m_dump.empty()) {
        for(auto it : m_controller->selected()) {
            m_selected.push_back(it->uuid());
            Actor *actor = dynamic_cast<Actor *>(it->clone(it->parent()));
            if(actor) {
                static_cast<Object *>(actor)->clearCloneRef();
                actor->setName(findFreeObjectName(it->name(), it->parent()));
                m_objects.push_back(actor->uuid());
            }
        }
    } else {
        for(auto &it : m_dump) {
            Object *obj = ObjectSystem::toObject(it);
            m_objects.push_back(obj->uuid());
        }
    }

    emit m_controller->mapUpdated();

    m_controller->clear(false);
    m_controller->selectActors(m_objects);
}

CreateObjectSerial::CreateObjectSerial(Object::ObjectList &list, ObjectCtrl *ctrl, const QString &name, QUndoCommand *group) :
        UndoObject(ctrl, name, group) {

    for(auto it : list) {
        m_dump.push_back(ObjectSystem::toVariant(it));
        m_parents.push_back(it->parent()->uuid());
        delete it;
    }
}
void CreateObjectSerial::undo() {
    for(auto it : m_controller->selected()) {
        delete it;
    }
    m_controller->clear(false);
    m_controller->selectActors(m_objects);
}
void CreateObjectSerial::redo() {
    m_objects.clear();
    for(auto it : m_controller->selected()) {
        m_objects.push_back(it->uuid());
    }
    auto it = m_parents.begin();

    list<uint32_t> objects;
    for(auto &ref : m_dump) {
        Object *object = Engine::toObject(ref);
        if(object) {
            object->setParent(m_controller->findObject(*it));
            objects.push_back(object->uuid());
        } else {
            qWarning() << "Broken object";
        }
        ++it;
    }
    emit m_controller->mapUpdated();

    m_controller->clear(false);
    m_controller->selectActors(objects);
}

DeleteActors::DeleteActors(const Object::ObjectList &objects, ObjectCtrl *ctrl, const QString &name, QUndoCommand *group) :
        UndoObject(ctrl, name, group) {

    for(auto it : objects) {
        m_objects.push_back(it->uuid());
    }
}
void DeleteActors::undo() {
    auto it = m_parents.begin();
    auto index = m_indices.begin();
    for(auto &ref : m_dump) {
        Object *parent = m_controller->findObject(*it);
        Object *object = Engine::toObject(ref, parent);
        if(object) {
            object->setParent(parent, *index);
            m_objects.push_back(object->uuid());
        }
        ++it;
        ++index;
    }
    emit m_controller->mapUpdated();
    if(!m_objects.empty()) {
        auto it = m_objects.begin();
        while(it != m_objects.end()) {
            Component *comp = dynamic_cast<Component *>(m_controller->findObject(*it));
            if(comp) {
                *it = comp->parent()->uuid();
            }
            ++it;
        }
        m_controller->clear(false);
        m_controller->selectActors(m_objects);
    }
}
void DeleteActors::redo() {
    m_parents.clear();
    m_dump.clear();
    for(auto it : m_objects)  {
        Object *object = m_controller->findObject(it);
        if(object) {
            m_dump.push_back(Engine::toVariant(object));
            m_parents.push_back(object->parent()->uuid());

            QList<Object *> children = QList<Object *>::fromStdList(object->parent()->getChildren());
            m_indices.push_back(children.indexOf(object));
        }
    }
    for(auto it : m_objects) {
        Object *object = m_controller->findObject(it);
        if(object) {
            delete object;
        }
    }
    m_objects.clear();

    m_controller->clear();

    emit m_controller->mapUpdated();
}

RemoveComponent::RemoveComponent(const Component *component, ObjectCtrl *ctrl, const QString &name, QUndoCommand *group) :
        UndoObject(ctrl, name + " " + component->typeName().c_str(), group),
        m_parent(0),
        m_uuid(component->uuid()),
        m_index(0) {

}
void RemoveComponent::undo() {
    Object *parent = m_controller->findObject(m_parent);
    Object *object = Engine::toObject(m_dump, parent);
    if(object) {
        object->setParent(parent, m_index);

        emit m_controller->objectsSelected(m_controller->selected());
        emit m_controller->objectsUpdated();
        emit m_controller->mapUpdated();
    }
}
void RemoveComponent::redo() {
    m_dump = Variant();
    m_parent = 0;
    Object *object = m_controller->findObject(m_uuid);
    if(object) {
        m_dump = Engine::toVariant(object, true);
        m_parent = object->parent()->uuid();

        QList<Object *> children = QList<Object *>::fromStdList(object->parent()->getChildren());
        m_index = children.indexOf(object);

        m_controller->resetSelection();

        delete object;
    }

    emit m_controller->objectsSelected(m_controller->selected());
    emit m_controller->objectsUpdated();
    emit m_controller->mapUpdated();
}

ParentingObjects::ParentingObjects(const Object::ObjectList &objects, Object *origin, ObjectCtrl *ctrl, const QString &name, QUndoCommand *group) :
        UndoObject(ctrl, name, group) {
    for(auto it : objects) {
        m_objects.push_back(it->uuid());
    }
    m_parent = origin->uuid();
}
void ParentingObjects::undo() {
    auto ref = m_dump.begin();
    for(auto it : m_objects) {
        Object *object = m_controller->findObject(it);
        if(object) {
            if(object->uuid() == ref->first) {
                object->setParent(m_controller->findObject(ref->second));
            }
        }
        ++ref;
    }
    emit m_controller->objectsUpdated();
    emit m_controller->mapUpdated();
}
void ParentingObjects::redo() {
    m_dump.clear();
    for(auto it : m_objects) {
        Object *object = m_controller->findObject(it);
        if(object) {
            ParentPair pair;
            pair.first =  object->uuid();
            pair.second = object->parent()->uuid();
            m_dump.push_back(pair);

            object->setParent(m_controller->findObject(m_parent));
        }
    }
    emit m_controller->objectsUpdated();
    emit m_controller->mapUpdated();
}

PropertyObject::PropertyObject(Object *object, const QString &property, const Variant &value, ObjectCtrl *ctrl, const QString &name, QUndoCommand *group) :
        UndoObject(ctrl, name, group),
        m_value(value),
        m_property(property),
        m_object(object->uuid()) {

}
void PropertyObject::undo() {
    PropertyObject::redo();
}
void PropertyObject::redo() {
    Variant value = m_value;

    Object *object = m_controller->findObject(m_object);
    if(object) {
        const MetaObject *meta = object->metaObject();
        int index = meta->indexOfProperty(qPrintable(m_property));
        if(index > -1) {
            MetaProperty property = meta->property(index);
            if(property.isValid()) {
                m_value = property.read(object);

                property.write(object, value);
            }
        }
    }
    emit m_controller->objectsUpdated();
    emit m_controller->objectsChanged({object}, m_property);
}
