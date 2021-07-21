#include "resources/material.h"
#include "resources/texture.h"

#define PROPERTIES  "Properties"
#define TEXTURES    "Textures"
#define UNIFORMS    "Uniforms"

MaterialInstance::MaterialInstance(Material *material) :
        m_pMaterial(material),
        m_SurfaceType(0) {

}

MaterialInstance::~MaterialInstance() {
    m_Info.clear();
}

Material *MaterialInstance::material() const {
    return m_pMaterial;
}

Texture *MaterialInstance::texture(const char *name) {
    Texture *result = nullptr;
    auto it = m_Info.find(name);
    if(it != m_Info.end()) {
        result = static_cast<Texture *>((*it).second.ptr);
    }
    return result;
}

MaterialInstance::InfoMap &MaterialInstance::params() {
    return m_Info;
}

void MaterialInstance::setInteger(const char *name, int32_t *value, int32_t count) {
    Info info;
    info.count = count;
    info.ptr = value;
    info.type = MetaType::INTEGER;

    m_Info[name] = info;
}
void MaterialInstance::setFloat(const char *name, float *value, int32_t count) {
    Info info;
    info.count = count;
    info.ptr = value;
    info.type = MetaType::FLOAT;

    m_Info[name] = info;
}
void MaterialInstance::setVector2(const char *name, Vector2 *value, int32_t count) {
    Info info;
    info.count = count;
    info.ptr = value;
    info.type = MetaType::VECTOR2;

    m_Info[name] = info;
}
void MaterialInstance::setVector3(const char *name, Vector3 *value, int32_t count) {
    Info info;
    info.count = count;
    info.ptr = value;
    info.type = MetaType::VECTOR3;

    m_Info[name] = info;
}
void MaterialInstance::setVector4(const char *name, Vector4 *value, int32_t count) {
    Info info;
    info.count = count;
    info.ptr = value;
    info.type = MetaType::VECTOR4;

    m_Info[name] = info;
}
void MaterialInstance::setMatrix4(const char *name, Matrix4 *value, int32_t count) {
    Info info;
    info.count = count;
    info.ptr = value;
    info.type = MetaType::MATRIX4;

    m_Info[name] = info;
}

void MaterialInstance::setTexture(const char *name, Texture *value, int32_t count) {
    Info info;
    info.count = count;
    info.ptr = value;
    info.type = 0;

    m_Info[name] = info;
}

uint16_t MaterialInstance::surfaceType() const {
    return m_SurfaceType;
}

void MaterialInstance::setSurfaceType(uint16_t type) {
    m_SurfaceType = type;
}

/*!
    \class Material
    \brief A Material is a resource which can be applied to a Mesh to control the visual look of the scene.
    \inmodule Resources
*/

Material::Material() :
        m_BlendMode(Opaque),
        m_LightModel(Unlit),
        m_MaterialType(Surface),
        m_DoubleSided(true),
        m_DepthTest(true),
        m_DepthWrite(true),
        m_Surfaces(1) {
    clear();
}

Material::~Material() {
    clear();
}
/*!
    Removes all attached textures from the material.
*/
void Material::clear() {
    m_Textures.clear();
}
/*!
    Returns current material type.
    For more detalse please refer to Material::MaterialType enum.
*/
int Material::materialType() const {
    return m_MaterialType;
}
/*!
    Sets new material \a type.
    For more detalse please refer to Material::MaterialType enum.
*/
void Material::setMaterialType(int type) {
    m_MaterialType = type;
}
/*!
    Returns current light model for the material.
    For more detalse please refer to Material::LightModelType enum.
*/
int Material::lightModel() const {
    return m_LightModel;
}
/*!
    Sets a new light \a model for the material.
    For more detalse please refer to Material::LightModelType enum.
*/
void Material::setLightModel(int model) {
    m_LightModel = model;
}
/*!
    Returns current blend mode for the material.
    For more detalse please refer to Material::BlendType enum.
*/
int Material::blendMode() const {
    return m_BlendMode;
}
/*!
    Sets a new blend \a mode for the material.
    For more detalse please refer to Material::BlendType enum.
*/
void Material::setBlendMode(int mode) {
    m_BlendMode = mode;
}
/*!
    Returns true if mas marked as double-sided; otherwise returns false.
*/
bool Material::doubleSided() const {
    return m_DoubleSided;
}
/*!
    Enables or disables the double-sided \a flag for the material.
*/
void Material::setDoubleSided(bool flag) {
    m_DoubleSided = flag;
}
/*!
    Returns true if depth test was enabled; otherwise returns false.
*/
bool Material::depthTest() const {
    return m_DepthTest;
}
/*!
    Enables or disables a depth \a test for the material.
*/
void Material::setDepthTest(bool test) {
    m_DepthTest = test;
}
/*!
    Returns true if write opertaion to the depth buffer was enabled; otherwise returns false.
*/
bool Material::depthWrite() const {
    return m_DepthWrite;
}
/*!
    Enables or disables a \a write operation to the depth buffer.
*/
void Material::setDepthWrite(bool write) {
    m_DepthWrite = write;
}
/*!
    Sets a \a texture with a given \a name for the material.
*/
void Material::setTexture(const string &name, Texture *texture) {
    for(auto &it : m_Textures) {
        if(it.name == name) {
            it.texture = texture;
            return;
        }
    }

    TextureItem item;
    item.name = name;
    item.texture = texture;
    item.binding = -1;
    m_Textures.push_back(item);
}
/*!
    \internal
*/
void Material::loadUserData(const VariantMap &data) {
    clear();
    {
        auto it = data.find(PROPERTIES);
        if(it != data.end()) {
            VariantList list = (*it).second.value<VariantList>();
            auto i = list.begin();
            m_MaterialType = static_cast<MaterialType>((*i).toInt());
            i++;
            m_DoubleSided = (*i).toBool();
            i++;
            m_Surfaces = (*i).toInt();
            i++;
            m_BlendMode = static_cast<BlendType>((*i).toInt());
            i++;
            m_LightModel = static_cast<LightModelType>((*i).toInt());
            i++;
            m_DepthTest = (*i).toBool();
            i++;
            m_DepthWrite = (*i).toBool();
        }
    }
    {
        m_Textures.clear();
        auto it = data.find(TEXTURES);
        if(it != data.end()) {
            for(auto &t : (*it).second.toList()) {
                VariantList list = t.toList();
                auto f = list.begin();
                string path = (*f).toString();
                TextureItem item;
                item.texture = nullptr;
                if(!path.empty()) {
                    item.texture = Engine::loadResource<Texture>(path);
                }
                ++f;
                item.binding = (*f).toInt();
                ++f;
                item.name = (*f).toString();
                ++f;
                item.flags = (*f).toInt();

                m_Textures.push_back(item);

            }
        }
    }
    {
        m_Uniforms.clear();
        auto it = data.find(UNIFORMS);
        if(it != data.end()) {
            for(auto &u : (*it).second.toList()) {
                VariantList list = u.toList();
                auto f = list.begin();

                UniformItem item;
                item.value = (*f); // value
                ++f;
                item.size = (*f).toInt(); // size
                ++f;
                item.name = (*f).toString(); // name

                m_Uniforms.push_back(item);
            }
        }
    }
}
/*!
    Returns a new instance for the material with the provided surface \a type.
*/
MaterialInstance *Material::createInstance(SurfaceType type) {
    A_UNUSED(type);
    MaterialInstance *result = new MaterialInstance(this);

    initInstance(result);

    return result;
}

/*!
    \internal
*/
void Material::initInstance(MaterialInstance *instance) {
    if(instance) {
        for(auto &it : m_Uniforms) {
            MaterialInstance::Info info;
            info.count = 1;
            info.ptr = it.value.data();
            info.type = it.value.type();

            instance->m_Info[it.name] = info;
        }
    }
}
