#pragma version

#pragma flags

#include "Common.vert"

layout(location = 0) in vec4 _vertex;
layout(location = 1) in vec2 _uv0;
layout(location = 2) in vec2 _uv1;
layout(location = 3) in vec3 _n;
layout(location = 4) in vec3 _t;
layout(location = 5) in vec3 _b;
layout(location = 6) in vec4 _color;

layout(location = 7) in vec3 _view;

#pragma material

layout(location = 0) out vec4 gbuffer1;
layout(location = 1) out vec4 gbuffer2;
layout(location = 2) out vec4 gbuffer3;
layout(location = 3) out vec4 gbuffer4;

void simpleMode(Params params) {
    float alpha = getOpacity ( params );
    if(g.clip >= alpha) {
        discard;
    }
    gbuffer1 = l.color;
}

void passMode(Params params) {
    vec3 albd   = getDiffuse ( params ) * l.color.xyz;
    vec3 emit   = getEmissive( params ) * l.color.xyz;
    float alpha = getOpacity ( params ) * l.color.w;
    float spec  = 1.0;
#ifdef BLEND_OPAQUE
    if(g.clip >= alpha) {
        discard;
    }
    vec3 norm   = vec3(1.0);
    vec3 matv   = vec3(0.0, 0.0, getMetallic( params ));
    float model = 0.0;
    #ifdef MODEL_LIT
    model  = 0.34;
    norm   = params.normal * 0.5 + 0.5;
    matv.x = max(0.01, getRoughness( params ));
    #endif
    gbuffer1 = vec4( norm, model );
    gbuffer2 = vec4( albd, 1.0   );
    gbuffer3 = vec4( matv, spec  );
    gbuffer4 = vec4( emit, 0.0   );
#else
    gbuffer1 = vec4( emit, alpha );
#endif
}

void lightMode(Params params) {
    gbuffer1 = vec4( getEmissive( params ), 1.0 );
}

void main(void) {
#ifdef SIMPLE
    simpleMode(params);
#elif LIGHT
    lightMode(params);
#else
    params.reflect = vec3(0.0);
    params.normal  = _n;
    params.normal  = getNormal( params ) * 2.0 - 1.0;
    params.normal  = normalize( params.normal.x * _t + params.normal.y * _b + params.normal.z * _n );
    params.reflect = reflect( _view, params.normal );

    passMode(params);
#endif
}
