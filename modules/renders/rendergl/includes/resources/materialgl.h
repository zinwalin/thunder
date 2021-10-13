#ifndef MATERIALGL_H
#define MATERIALGL_H

#include <unordered_map>
#include <list>

#include <resources/material.h>

#include <engine.h>

class CommandBufferGL;

struct VertexBufferObject {
    Matrix4 m_Model;
    Matrix4 m_View;
    Matrix4 m_Projection;
};

struct FragmentBufferObject {
    Vector4 m_Color;
    float m_Clip;
    float m_Time;
};

class MaterialInstanceGL : public MaterialInstance {
public:
    MaterialInstanceGL(Material *material);

    bool bind(CommandBufferGL *buffer, const VertexBufferObject &vertex, const FragmentBufferObject &fragment, uint32_t layer);

};

class MaterialGL : public Material {
    A_OVERRIDE(MaterialGL, Material, Resources)

    A_NOPROPERTIES()
    A_NOMETHODS()

    enum ShaderType {
        Static      = 1,
        Instanced,
        Skinned,
        Particle,
        LastVertex,

        Default     = 20,
        Simple,
        LastFragment
    };

    typedef unordered_map<uint32_t, uint32_t> ObjectMap;

public:
    void loadUserData(const VariantMap &data) override;

    uint32_t bind(uint32_t layer, uint16_t vertex);

    uint32_t getProgram(uint16_t type);

    TextureList textures() const { return m_Textures; }

protected:
    uint32_t buildShader(uint16_t type, const string &src = string());

    uint32_t buildProgram(uint32_t vertex, uint32_t fragment);

    bool checkShader(uint32_t shader, const string &path, bool link = false);

    MaterialInstance *createInstance(SurfaceType type = SurfaceType::Static) override;

private:
    ObjectMap m_Programs;

    map<uint16_t, string> m_ShaderSources;

};

#endif // MATERIALGL_H
