#include "resources/materialgl.h"

#include "agl.h"
#include "commandbuffergl.h"

#include "resources/text.h"
#include "resources/texturegl.h"

#include <file.h>
#include <log.h>

#define MODEL_UNIFORM   0
#define VIEW_UNIFORM    1
#define PROJ_UNIFORM    2

#define COLOR_BIND  3
#define CLIP_BIND   4
#define TIMER_BIND  5

void MaterialGL::loadUserData(const VariantMap &data) {
    Material::loadUserData(data);

    if(m_MaterialType == Surface) {
        {
            auto it = data.find("Simple");
            if(it != data.end()) {
                m_ShaderSources[Simple] = (*it).second.toString();
            }
        }
        {
            auto it = data.find("StaticInst");
            if(it != data.end()) {
                m_ShaderSources[Instanced] = (*it).second.toString();
            }
        }
        {
            auto it = data.find("Particle");
            if(it != data.end()) {
                m_ShaderSources[Particle] = (*it).second.toString();
            }
        }
        {
            auto it = data.find("Skinned");
            if(it != data.end()) {
                m_ShaderSources[Skinned] = (*it).second.toString();
                setTexture("skinMatrices", nullptr);
            }
        }
    }

    {
        auto it = data.find("Shader");
        if(it != data.end()) {
            m_ShaderSources[Default] = (*it).second.toString();
        }
    }
    {
        auto it = data.find("Static");
        if(it != data.end()) {
            m_ShaderSources[Static] = (*it).second.toString();
        }
    }

    setState(ToBeUpdated);
}

uint32_t MaterialGL::getProgram(uint16_t type) {
    switch(state()) {
        case Suspend: {
            for(auto it : m_Programs) {
                glDeleteProgram(it.second);
            }
            m_Programs.clear();

            setState(ToBeDeleted);
        } break;
        case ToBeUpdated: {
            for(auto it : m_Programs) {
                glDeleteProgram(it.second);
            }
            m_Programs.clear();

            for(uint16_t v = Static; v < LastVertex; v++) {
                auto itv = m_ShaderSources.find(v);
                if(itv != m_ShaderSources.end()) {
                    for(uint16_t f = Default; f < LastFragment; f++) {
                        auto itf = m_ShaderSources.find(f);
                        if(itf != m_ShaderSources.end()) {
                            uint32_t vertex = buildShader(itv->first, itv->second);
                            uint32_t fragment = buildShader(itf->first, itf->second);
                            uint32_t index = v * f;
                            m_Programs[index] = buildProgram(vertex, fragment);
                        }
                    }
                }
            }

            setState(Ready);
        } break;
        default: break;
    }

    auto it = m_Programs.find(type);
    if(it != m_Programs.end()) {
        return it->second;
    }
    return 0;
}

uint32_t MaterialGL::bind(uint32_t layer, uint16_t vertex) {
    int32_t b = blendMode();

    if((layer & CommandBuffer::DEFAULT || layer & CommandBuffer::SHADOWCAST) &&
       (b == Material::Additive || b == Material::Translucent)) {
        return 0;
    }
    if(layer & CommandBuffer::TRANSLUCENT && b == Material::Opaque) {
        return 0;
    }

    uint16_t type = MaterialGL::Default;
    if((layer & CommandBuffer::RAYCAST) || (layer & CommandBuffer::SHADOWCAST)) {
        type = MaterialGL::Simple;
    }
    uint32_t program = getProgram(vertex * type);
    if(!program) {
        return 0;
    }

    if(!m_DepthTest) {
        glDisable(GL_DEPTH_TEST);
    } else {
        glEnable(GL_DEPTH_TEST);
        //glDepthFunc((layer & CommandBuffer::DEFAULT) ? GL_EQUAL : GL_LEQUAL);

        glDepthMask((m_DepthWrite) ? GL_TRUE : GL_FALSE);
    }

    if(!doubleSided() && !(layer & CommandBuffer::RAYCAST)) {
        glEnable( GL_CULL_FACE );
        if(m_MaterialType == LightFunction) {
            glCullFace(GL_FRONT);
        } else {
            glCullFace(GL_BACK);
        }
    } else {
        glDisable(GL_CULL_FACE);
    }

    if(b != Material::Opaque && !(layer & CommandBuffer::RAYCAST)) {
        glEnable(GL_BLEND);
        if(b == Material::Translucent) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glBlendFunc(GL_ONE, GL_ONE);
        }
        glBlendEquation(GL_FUNC_ADD);
    } else {
        glDisable(GL_BLEND);
    }

    return program;
}

uint32_t MaterialGL::buildShader(uint16_t type, const string &src) {
    const char *data = src.c_str();

    uint32_t t;
    switch(type) {
        case Default:
        case Simple: {
            t  = GL_FRAGMENT_SHADER;
        } break;
        default: {
            t  = GL_VERTEX_SHADER;
        } break;
    }
    uint32_t shader = glCreateShader(t);
    if(shader) {
        glShaderSource(shader, 1, &data, nullptr);
        glCompileShader(shader);
        checkShader(shader, "");
    }

    return shader;
}

uint32_t MaterialGL::buildProgram(uint32_t vertex, uint32_t fragment) {
    uint32_t result = glCreateProgram();
    if(result) {
        glAttachShader(result, vertex);
        glAttachShader(result, fragment);
        glLinkProgram(result);
        glDetachShader(result, vertex);
        glDetachShader(result, fragment);

        checkShader(result, "", true);

        glDeleteShader(vertex);
        glDeleteShader(fragment);

        glUseProgram(result);
        uint8_t t = 0;
        for(auto &it : m_Textures) {
            if(it.binding > -1) {
                glUniform1i(it.binding, t);
            }
            t++;
        }

        for(auto &it : m_Uniforms) {
            int32_t location = glGetUniformLocation(result, it.name.c_str());
            if(location > -1) {
                switch(it.value.type()) {
                    case MetaType::VECTOR4: {
                        glUniform4fv(location, 1, it.value.toVector4().v);
                    } break;
                    default: {
                        glUniform1f(location, it.value.toFloat());
                    } break;
                }
            }
        }
    }

    return result;
}

bool MaterialGL::checkShader(uint32_t shader, const string &path, bool link) {
    int value   = 0;

    if(!link) {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &value);
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &value);
    }

    if(value != GL_TRUE) {
        if(!link) {
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &value);
        } else {
            glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &value);
        }
        if(value) {
            char *buff  = new char[value + 1];
            if(!link) {
                glGetShaderInfoLog(shader, value, nullptr, buff);
            } else {
                glGetProgramInfoLog(shader, value, nullptr, buff);
            }
            Log(Log::ERR) << "[ Render::ShaderGL ]" << path.c_str() << "\n[ Said ]" << buff;
            delete []buff;
        }
        return false;
    }
    return true;
}

MaterialInstance *MaterialGL::createInstance(SurfaceType type) {
    MaterialInstanceGL *result = new MaterialInstanceGL(this);

    initInstance(result);

    if(result) {
        uint16_t t = Instanced;
        switch(type) {
            case SurfaceType::Static: t = Static; break;
            case SurfaceType::Skinned: t = Skinned; break;
            case SurfaceType::Billboard: t = Particle; break;
            default: break;
        }

        result->setSurfaceType(t);
    }
    return result;
}

MaterialInstanceGL::MaterialInstanceGL(Material *material) :
    MaterialInstance(material) {

}

#include <timer.h>

bool MaterialInstanceGL::bind(CommandBufferGL *buffer, const VertexBufferObject &vertex, const FragmentBufferObject &fragment, uint32_t layer) {
    MaterialGL *material = static_cast<MaterialGL *>(m_pMaterial);
    uint32_t program = material->bind(layer, surfaceType());
    if(program) {
        glUseProgram(program);

        // put uniforms

        int32_t location;

        glUniformMatrix4fv(VIEW_UNIFORM, 1, GL_FALSE, vertex.m_View.mat);
        glUniformMatrix4fv(PROJ_UNIFORM, 1, GL_FALSE, vertex.m_Projection.mat);

        glUniform1f   (TIMER_BIND, Timer::time());
        glUniform1f   (CLIP_BIND,  0.99f);
        glUniform4fv  (COLOR_BIND, 1, fragment.m_Color.v);

        // Push uniform values to shader
        for(const auto &it : buffer->params()) {
            location = glGetUniformLocation(program, it.first.c_str());
            if(location > -1) {
                const Variant &data = it.second;
                switch(data.type()) {
                    case MetaType::VECTOR2: glUniform2fv      (location, 1, data.toVector2().v); break;
                    case MetaType::VECTOR3: glUniform3fv      (location, 1, data.toVector3().v); break;
                    case MetaType::VECTOR4: glUniform4fv      (location, 1, data.toVector4().v); break;
                    case MetaType::MATRIX4: glUniformMatrix4fv(location, 1, GL_FALSE, data.toMatrix4().mat); break;
                    default:                glUniform1f       (location, data.toFloat()); break;
                }
            }
        }

        for(const auto &it : params()) {
            location = glGetUniformLocation(program, it.first.c_str());
            if(location > -1) {
                const MaterialInstance::Info &data = it.second;
                switch(data.type) {
                    case MetaType::INTEGER: glUniform1iv      (location, data.count, static_cast<const int32_t *>(data.ptr)); break;
                    case MetaType::FLOAT:   glUniform1fv      (location, data.count, static_cast<const float *>(data.ptr)); break;
                    case MetaType::VECTOR2: glUniform2fv      (location, data.count, static_cast<const float *>(data.ptr)); break;
                    case MetaType::VECTOR3: glUniform3fv      (location, data.count, static_cast<const float *>(data.ptr)); break;
                    case MetaType::VECTOR4: glUniform4fv      (location, data.count, static_cast<const float *>(data.ptr)); break;
                    case MetaType::MATRIX4: glUniformMatrix4fv(location, data.count, GL_FALSE, static_cast<const float *>(data.ptr)); break;
                    default: break;
                }
            }
        }

        uint8_t i = 0;
        for(auto &it : material->textures()) {
            Texture *tex = it.texture;
            Texture *tmp = texture(it.name.c_str());

            if(tmp) {
                tex = tmp;
            } else {
                tmp = buffer->texture(it.name.c_str());
                if(tmp) {
                    tex = tmp;
                }
            }

            if(tex) {
                glActiveTexture(GL_TEXTURE0 + i);
                uint32_t texture = GL_TEXTURE_2D;
                if(tex->isCubemap()) {
                    texture = GL_TEXTURE_CUBE_MAP;
                }

                glBindTexture(texture, static_cast<TextureGL *>(tex)->nativeHandle());
            }
            i++;
        }

        return true;
    }

    return false;
}
