#pragma once

//setup the programs and pipeline for points
#define VERSION "#version 150\n"
#define QUOTE(...) #__VA_ARGS__
static const char* point_vertex_shader_text = VERSION QUOTE(
    uniform mat4 mvp;
    in vec3 vcol;
    in vec3 vpos;
    in vec4 vquat;
    in vec3 mesh_pos;
    in float vsize;
    out vec3 color;
    out vec3 normal;
    out vec3 frag_pos;
    out vec3 view_pos;

    mat4 quat_matrix(vec4 vquat) {
		float qxx = vquat.x * vquat.x;
		float qyy = vquat.y * vquat.y;
		float qzz = vquat.z * vquat.z;
		float qxz = vquat.x * vquat.z;
		float qxy = vquat.x * vquat.y;
		float qyz = vquat.y * vquat.z;
		float qwx = vquat.w * vquat.x;
		float qwy = vquat.w * vquat.y;
		float qwz = vquat.w * vquat.z;

        mat4 orientation = mat4(1.0);

		orientation[0][0] = 1 - 2 * (qyy +  qzz);
		orientation[0][1] = 2 * (qxy + qwz);
		orientation[0][2] = 2 * (qxz - qwy);

		orientation[1][0] = 2 * (qxy - qwz);
		orientation[1][1] = 1 - 2 * (qxx +  qzz);
		orientation[1][2] = 2 * (qyz + qwx);

		orientation[2][0] = 2 * (qxz + qwy);
		orientation[2][1] = 2 * (qyz - qwx);
		orientation[2][2] = 1 - 2 * (qxx +  qyy);
        return orientation;
    }

    mat4 translate_matrix(vec3 v) {
        mat4 translated = mat4(1.0);
        translated[3][0] = v.x;
        translated[3][1] = v.y;
        translated[3][2] = v.z;
        return translated;
    }

    void main() {
        mat4 translation = translate_matrix(vpos);
        mat4 mquat = quat_matrix(vquat);
        mat4 orientation = translation * mquat;
        vec4 pos = orientation * vec4(mesh_pos * vsize, 1.0);
        gl_Position = mvp * pos;
        color = vcol;
        normal = vec3(mquat * vec4(mesh_pos, 1.0));
        frag_pos = vec3(pos);
        view_pos = vpos;
    }
);

// https://learnopengl.com/Lighting/Basic-Lighting
static const char* point_fragment_shader_text = VERSION QUOTE(
    in vec3 color;
    out vec4 out_color;
    in vec3 normal;
    in vec3 frag_pos;
    in vec3 view_pos;
    void main() {
        vec3 norm = normalize(normal);
        vec3 light_pos = vec3(0.0, 0.0, 0.0);
        vec3 light_dir = normalize(light_pos - frag_pos);
        float diffuse = max(dot(norm, light_dir), 0.0);
        float ambient = 0.6;
        float specular_strength = 0.8;
        vec3 view_dir = normalize(view_pos - frag_pos);
        vec3 reflect_dir = reflect(- light_dir, norm);
        float spec = pow(max(dot(- view_dir, reflect_dir), 0.0), 32);
        float specular = specular_strength * spec;
        vec3 point_color = (ambient + diffuse + spec) * color;
        out_color = vec4(point_color, 1.0);
    }
);

//setup the programs and pipeline for lines
static const char* line_vertex_shader_text = VERSION QUOTE(

    vec3 hsv2rgb(vec3 c) {
        vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
        vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
        return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
    }

    uniform uint pid;
    uniform mat4 mvp;
    in vec3 vpos;
    out vec4 tcolor;
    void main() {
        vec4 pos = mvp * vec4(vpos, 1.0);
        gl_Position = pos;
        tcolor = vec4(hsv2rgb(vec3(fract(float(pid % uint(10)) / 10.0), 0.5, 1)), 1);
        //tcolor = vec4(0, 1, 0, 1);
    }
);

static const char* line_fragment_shader_text = VERSION QUOTE(
    in vec4 color;
    out vec4 out_color;
    void main() {
        out_color = vec4(color.xyzw);
    }
);

static const char* line_geometry_text = VERSION QUOTE(
    layout(lines) in;
    layout(triangle_strip, max_vertices = 4) out;
    in gl_PerVertex {
        vec4 gl_Position;
    } gl_in[];
    out gl_PerVertex {
        vec4 gl_Position;
    };
    in vec4 tcolor[];
    out vec4 color;
    void main() {
        const float direction[4] = float[4](-1, 1, -1, 1);
        vec2 ndc[2];
        ndc[0] = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
        ndc[1] = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;
        vec2 ndcv = ndc[0] - ndc[1];
        vec2 p = vec2(ndcv.y, -ndcv.x);
        vec2 np = p / sqrt(dot(p, p));

        for (int i = 0; i < 4; i++) {
            vec2 outndc = ndc[i/2] + np * direction[i] / gl_in[i/2].gl_Position.w / 4;
            gl_Position = vec4(outndc * gl_in[i/2].gl_Position.w, gl_in[i/2].gl_Position.zw);
            color = tcolor[i/2];
            EmitVertex();
        }
        EndPrimitive();
    }
);

static const char* line2d_fragment_shader_text = VERSION QUOTE(
    in vec4 tcolor;
    out vec4 out_color;
    void main() {
        out_color = vec4(tcolor.rgb * 0.5, 0.5);
    }
);
