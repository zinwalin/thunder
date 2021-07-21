#include "commandbuffergl.h"

#include "agl.h"

#include "resources/materialgl.h"
#include "resources/meshgl.h"
#include "resources/texturegl.h"
#include "resources/rendertargetgl.h"

#include <log.h>
#include <timer.h>

CommandBufferGL::CommandBufferGL() {
    PROFILE_FUNCTION();

}

CommandBufferGL::~CommandBufferGL() {

}

void CommandBufferGL::clearRenderTarget(bool clearColor, const Vector4 &color, bool clearDepth, float depth) {
    PROFILE_FUNCTION();

    uint32_t flags = 0;
    if(clearColor) {
        flags |= GL_COLOR_BUFFER_BIT;
        glClearColor(color.x, color.y, color.z, color.w);
    }
    if(clearDepth) {
        glDepthMask(GL_TRUE);
        flags |= GL_DEPTH_BUFFER_BIT;
        glClearDepthf(depth);
    }
    glClear(flags);
}

const VariantMap &CommandBufferGL::params() const {
    return m_Uniforms;
}

void CommandBufferGL::drawMesh(const Matrix4 &model, Mesh *mesh, uint32_t sub, uint32_t layer, MaterialInstance *material) {
    PROFILE_FUNCTION();
    A_UNUSED(sub);

    if(mesh && material) {
        MeshGL *m = static_cast<MeshGL *>(mesh);
        uint32_t lod = 0;
        Lod *l = mesh->lod(lod);
        if(l == nullptr) {
            return;
        }

        MaterialInstanceGL *instance = static_cast<MaterialInstanceGL *>(material);

        m_vertex.m_Model = model;

        if(instance->bind(this, m_vertex, m_fragment, layer)) {
            m->bindVao(this, lod);

            Mesh::TriangleTopology topology = static_cast<Mesh::TriangleTopology>(mesh->topology());
            if(topology > Mesh::Lines) {
                uint32_t vert = l->vertices().size();
                int32_t glMode = GL_TRIANGLE_STRIP;
                switch(topology) {
                case Mesh::LineStrip:   glMode = GL_LINE_STRIP; break;
                case Mesh::TriangleFan: glMode = GL_TRIANGLE_FAN; break;
                default: break;
                }
                glDrawArrays(glMode, 0, vert);
                PROFILER_STAT(POLYGONS, vert - 2);
            } else {
                uint32_t index = l->indices().size();
                glDrawElements((topology == Mesh::Triangles) ? GL_TRIANGLES : GL_LINES, index, GL_UNSIGNED_INT, nullptr);
                PROFILER_STAT(POLYGONS, index / 3);
            }
            PROFILER_STAT(DRAWCALLS, 1);

            glBindVertexArray(0);
        }
    }
}

void CommandBufferGL::drawMeshInstanced(const Matrix4 *models, uint32_t count, Mesh *mesh, uint32_t sub, uint32_t layer, MaterialInstance *material) {
    PROFILE_FUNCTION();
    A_UNUSED(sub);

    if(mesh && material) {
        MeshGL *m = static_cast<MeshGL *>(mesh);
        uint32_t lod = 0;
        Lod *l = mesh->lod(lod);
        if(l == nullptr) {
            return;
        }

        MaterialInstanceGL *instance = static_cast<MaterialInstanceGL *>(material);

        m_vertex.m_Model = Matrix4();

        if(instance->bind(this, m_vertex, m_fragment, layer)) {
            glBindBuffer(GL_ARRAY_BUFFER, m->instance());
            glBufferData(GL_ARRAY_BUFFER, count * sizeof(Matrix4), models, GL_DYNAMIC_DRAW);

            m->bindVao(this, lod);

            Mesh::TriangleTopology topology = static_cast<Mesh::TriangleTopology>(mesh->topology());
            if(topology > Mesh::Lines) {
                uint32_t vert = l->vertices().size();
                glDrawArraysInstanced((topology == Mesh::TriangleStrip) ? GL_TRIANGLE_STRIP : GL_LINE_STRIP, 0, vert, count);
                PROFILER_STAT(POLYGONS, index - 2 * count);
            } else {
                uint32_t index = l->indices().size();
                glDrawElementsInstanced((topology == Mesh::Triangles) ? GL_TRIANGLES : GL_LINES, index, GL_UNSIGNED_INT, nullptr, count);
                PROFILER_STAT(POLYGONS, (index / 3) * count);
            }
            PROFILER_STAT(DRAWCALLS, 1);

            glBindVertexArray(0);
        }
    }
}

void CommandBufferGL::setRenderTarget(RenderTarget *target, uint32_t level) {
    PROFILE_FUNCTION();

    RenderTargetGL *t = static_cast<RenderTargetGL *>(target);
    if(t) {
        t->bindBuffer(level);
    }
}

Texture *CommandBufferGL::texture(const char *name) const {
    for(auto &it : m_Textures) {
        if(it.name == name) {
            return it.texture;
        }
    }
    return nullptr;
}

void CommandBufferGL::setColor(const Vector4 &color) {
    m_fragment.m_Color = color;
}

void CommandBufferGL::resetViewProjection() {
    m_vertex.m_View = m_SaveView;
    m_vertex.m_Projection = m_SaveProjection;
}

void CommandBufferGL::setViewProjection(const Matrix4 &view, const Matrix4 &projection) {
    m_SaveView = m_vertex.m_View;
    m_SaveProjection = m_vertex.m_Projection;

    m_vertex.m_View = view;
    m_vertex.m_Projection = projection;
}

void CommandBufferGL::setGlobalValue(const char *name, const Variant &value) {
    m_Uniforms[name] = value;
}

void CommandBufferGL::setGlobalTexture(const char *name, Texture *texture) {
    for(auto &it : m_Textures) {
        if(it.name == name) {
            it.texture = texture;
            return;
        }
    }

    Material::TextureItem item;
    item.name = name;
    item.texture = texture;
    item.binding = -1;
    m_Textures.push_back(item);
}

void CommandBufferGL::setViewport(int32_t x, int32_t y, int32_t width, int32_t height) {
    glViewport(x, y, width, height);
    setGlobalValue("camera.screen", Vector4(1.0f / (float)width, 1.0f / (float)height, width, height));
}

void CommandBufferGL::enableScissor(int32_t x, int32_t y, int32_t width, int32_t height) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);
}

void CommandBufferGL::disableScissor() {
    glDisable(GL_SCISSOR_TEST);
}

void CommandBufferGL::finish() {
    glFinish();
}

Matrix4 CommandBufferGL::projection() const {
    return m_vertex.m_Projection;
}

Matrix4 CommandBufferGL::view() const {
    return m_vertex.m_View;
}
