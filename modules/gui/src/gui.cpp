#include <amath.h>
#include <engine.h>

#include "gui.h"

#include "systems/guisystem.h"

static const char *meta = \
"{"
"   \"version\": \"1.0\","
"   \"module\": \"Gui\","
"   \"description\": \"GUI Plugin\","
"   \"systems\": ["
"       \"GuiSystem\""
"   ],"
"   \"extensions\": ["
"       \"AbstractButton\","
"       \"Button\","
"       \"Image\","
"       \"Label\","
"       \"ProgressBar\","
"       \"RectTransform\","
"       \"Switch\","
"       \"Widget\""
"   ]"
"}";

Module *moduleCreate(Engine *) {
    return new Gui();
}

Gui::Gui() :
        Module(),
        m_system(new GuiSystem) {
}

Gui::~Gui() {
    delete m_system;
}

System *Gui::system(const char *) {
    return m_system;
}

const char *Gui::metaInfo() const {
    return meta;
}
