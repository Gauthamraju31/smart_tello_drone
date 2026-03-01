#include "MapViewer.h"
#include <iostream>
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <imgui.h>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

// Include GL extension handler since we need glGenFramebuffers, etc.
// GLFW + ImGui backends usually load these. For a standalone class, we 
// rely on the GL context being active and functions loaded. 
// We use a minimal subset of OpenGL 1.1 / 2.0+ for the FBO.
#if defined(__linux__)
#include <GL/glx.h>
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_RENDERBUFFER 0x8D41
#define GL_DEPTH_COMPONENT24 0x81A6
// Function pointers for FBOs (normally handled by GLEW/GLAD, but we'll manually grab them to keep dependencies low)
typedef void (*glGenFramebuffersProc)(GLsizei n, GLuint *ids);
typedef void (*glBindFramebufferProc)(GLenum target, GLuint framebuffer);
typedef void (*glFramebufferTexture2DProc)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (*glGenRenderbuffersProc)(GLsizei n, GLuint *renderbuffers);
typedef void (*glBindRenderbufferProc)(GLenum target, GLuint renderbuffer);
typedef void (*glRenderbufferStorageProc)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (*glFramebufferRenderbufferProc)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (*glDeleteFramebuffersProc)(GLsizei n, const GLuint* framebuffers);
typedef void (*glDeleteRenderbuffersProc)(GLsizei n, const GLuint* renderbuffers);

static glGenFramebuffersProc glGenFramebuffers = nullptr;
static glBindFramebufferProc glBindFramebuffer = nullptr;
static glFramebufferTexture2DProc glFramebufferTexture2D = nullptr;
static glGenRenderbuffersProc glGenRenderbuffers = nullptr;
static glBindRenderbufferProc glBindRenderbuffer = nullptr;
static glRenderbufferStorageProc glRenderbufferStorage = nullptr;
static glFramebufferRenderbufferProc glFramebufferRenderbuffer = nullptr;
static glDeleteFramebuffersProc glDeleteFramebuffers = nullptr;
static glDeleteRenderbuffersProc glDeleteRenderbuffers = nullptr;

static void loadGLExtensions() {
    static bool loaded = false;
    if (loaded) return;
    glGenFramebuffers = (glGenFramebuffersProc)glXGetProcAddress((const GLubyte*)"glGenFramebuffers");
    glBindFramebuffer = (glBindFramebufferProc)glXGetProcAddress((const GLubyte*)"glBindFramebuffer");
    glFramebufferTexture2D = (glFramebufferTexture2DProc)glXGetProcAddress((const GLubyte*)"glFramebufferTexture2D");
    glGenRenderbuffers = (glGenRenderbuffersProc)glXGetProcAddress((const GLubyte*)"glGenRenderbuffers");
    glBindRenderbuffer = (glBindRenderbufferProc)glXGetProcAddress((const GLubyte*)"glBindRenderbuffer");
    glRenderbufferStorage = (glRenderbufferStorageProc)glXGetProcAddress((const GLubyte*)"glRenderbufferStorage");
    glFramebufferRenderbuffer = (glFramebufferRenderbufferProc)glXGetProcAddress((const GLubyte*)"glFramebufferRenderbuffer");
    glDeleteFramebuffers = (glDeleteFramebuffersProc)glXGetProcAddress((const GLubyte*)"glDeleteFramebuffers");
    glDeleteRenderbuffers = (glDeleteRenderbuffersProc)glXGetProcAddress((const GLubyte*)"glDeleteRenderbuffers");
    loaded = true;
}
#endif

MapViewer::MapViewer() {
#if defined(__linux__)
    loadGLExtensions();
#endif
}

MapViewer::~MapViewer() {
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    if (m_rbo) glDeleteRenderbuffers(1, &m_rbo);
    if (m_texture) glDeleteTextures(1, &m_texture);
}

// Helper: multiply a 4x4 matrix (column-major) by a vec3 position (w=1)
static void transformPoint(const float m[16], const float in[3], float out[3]) {
    out[0] = m[0]*in[0] + m[4]*in[1] + m[8]*in[2]  + m[12];
    out[1] = m[1]*in[0] + m[5]*in[1] + m[9]*in[2]  + m[13];
    out[2] = m[2]*in[0] + m[6]*in[1] + m[10]*in[2] + m[14];
}

// Helper: multiply upper-left 3x3 of a 4x4 matrix (column-major) by a vec3 direction (w=0)
static void transformNormal(const float m[16], const float in[3], float out[3]) {
    out[0] = m[0]*in[0] + m[4]*in[1] + m[8]*in[2];
    out[1] = m[1]*in[0] + m[5]*in[1] + m[9]*in[2];
    out[2] = m[2]*in[0] + m[6]*in[1] + m[10]*in[2];
    // Normalize
    float len = std::sqrt(out[0]*out[0] + out[1]*out[1] + out[2]*out[2]);
    if (len > 0.0001f) { out[0] /= len; out[1] /= len; out[2] /= len; }
}

// Recursive node processing
static void processNode(cgltf_node* node,
    std::vector<ModelVertex>& vertices, std::vector<unsigned int>& indices) 
{
    if (node->mesh) {
        // Get the world transform for this node
        float worldMatrix[16];
        cgltf_node_transform_world(node, worldMatrix);
        
        cgltf_mesh* mesh = node->mesh;
        for (cgltf_size j = 0; j < mesh->primitives_count; ++j) {
            cgltf_primitive* primitive = &mesh->primitives[j];
            
            cgltf_accessor* posAccessor = nullptr;
            cgltf_accessor* normAccessor = nullptr;
            cgltf_accessor* colorAccessor = nullptr;
            
            for (cgltf_size k = 0; k < primitive->attributes_count; ++k) {
                if (primitive->attributes[k].type == cgltf_attribute_type_position) {
                    posAccessor = primitive->attributes[k].data;
                } else if (primitive->attributes[k].type == cgltf_attribute_type_normal) {
                    normAccessor = primitive->attributes[k].data;
                } else if (primitive->attributes[k].type == cgltf_attribute_type_color) {
                    colorAccessor = primitive->attributes[k].data;
                }
            }
            
            if (!posAccessor) continue;

            // Extract material base color for this primitive
            float matR = 0.8f, matG = 0.8f, matB = 0.8f;
            if (primitive->material) {
                if (primitive->material->has_pbr_metallic_roughness) {
                    matR = primitive->material->pbr_metallic_roughness.base_color_factor[0];
                    matG = primitive->material->pbr_metallic_roughness.base_color_factor[1];
                    matB = primitive->material->pbr_metallic_roughness.base_color_factor[2];
                }
            }
            
            size_t vertexOffset = vertices.size();
            
            for (cgltf_size v = 0; v < posAccessor->count; ++v) {
                ModelVertex vertex = {0,0,0, 0,0,0, matR, matG, matB};
                float p[3] = {0,0,0};
                float n[3] = {0,0,1};
                
                cgltf_accessor_read_float(posAccessor, v, p, 3);
                
                // Apply world transform to position
                float wp[3];
                transformPoint(worldMatrix, p, wp);
                vertex.x = wp[0]; vertex.y = wp[1]; vertex.z = wp[2];
                
                if (normAccessor) {
                    cgltf_accessor_read_float(normAccessor, v, n, 3);
                }
                // Apply world transform to normal (direction only)
                float wn[3];
                transformNormal(worldMatrix, n, wn);
                vertex.nx = wn[0]; vertex.ny = wn[1]; vertex.nz = wn[2];

                // Per-vertex color overrides material color if present
                if (colorAccessor) {
                    float c[4] = {1,1,1,1};
                    cgltf_accessor_read_float(colorAccessor, v, c, 4);
                    vertex.r = c[0]; vertex.g = c[1]; vertex.b = c[2];
                }

                vertices.push_back(vertex);
            }
            
            cgltf_accessor* indexAccessor = primitive->indices;
            if (indexAccessor) {
                for (cgltf_size idx = 0; idx < indexAccessor->count; ++idx) {
                    cgltf_uint iVal = cgltf_accessor_read_index(indexAccessor, idx);
                    indices.push_back(vertexOffset + iVal);
                }
            } else {
                for (cgltf_size v = 0; v < posAccessor->count; ++v) {
                    indices.push_back(vertexOffset + v);
                }
            }
        }
    }
    
    // Process children recursively
    for (cgltf_size i = 0; i < node->children_count; ++i) {
        processNode(node->children[i], vertices, indices);
    }
}

bool MapViewer::loadModel(const std::string& path) {
    cgltf_options options = {};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) return false;

    result = cgltf_load_buffers(&options, data, path.c_str());
    if (result != cgltf_result_success) {
        cgltf_free(data);
        return false;
    }
    
    m_modelVertices.clear();
    m_modelIndices.clear();

    // Walk the scene node tree to apply all transforms
    if (data->scene) {
        for (cgltf_size i = 0; i < data->scene->nodes_count; ++i) {
            processNode(data->scene->nodes[i], m_modelVertices, m_modelIndices);
        }
    }

    // Auto-compute scale: normalize model to fit in ~1 unit
    if (!m_modelVertices.empty()) {
        float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
        float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
        for (const auto& v : m_modelVertices) {
            if (v.x < minX) minX = v.x; if (v.x > maxX) maxX = v.x;
            if (v.y < minY) minY = v.y; if (v.y > maxY) maxY = v.y;
            if (v.z < minZ) minZ = v.z; if (v.z > maxZ) maxZ = v.z;
        }
        float cx = (minX + maxX) * 0.5f;
        float cy = (minY + maxY) * 0.5f;
        float cz = (minZ + maxZ) * 0.5f;
        float sx = maxX - minX, sy = maxY - minY, sz = maxZ - minZ;
        float maxDim = std::max(sx, std::max(sy, sz));
        float normScale = (maxDim > 0.0001f) ? (1.5f / maxDim) : 1.0f;
        
        // Center and normalize all vertices
        for (auto& v : m_modelVertices) {
            v.x = (v.x - cx) * normScale;
            v.y = (v.y - cy) * normScale;
            v.z = (v.z - cz) * normScale;
        }
    }

    cgltf_free(data);
    m_modelLoaded = !m_modelVertices.empty();
    return m_modelLoaded;
}

void MapViewer::updateFBO(int width, int height) {
    if (m_width == width && m_height == height) return;
    m_width = width;
    m_height = height;

    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    if (m_rbo) glDeleteRenderbuffers(1, &m_rbo);
    if (m_texture) glDeleteTextures(1, &m_texture);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);

    glGenRenderbuffers(1, &m_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rbo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLuint MapViewer::renderToTexture(int width, int height, const SLAMEngine& slam) {
    if (width <= 0 || height <= 0) return 0;
    updateFBO(width, height);

    // Render to FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, width, height);
    
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // Simple legacy OpenGL rendering for the placeholder MapViewer
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)width / (float)height;
    // Simple perspective: fov=60, near=0.1, far=100
    float fH = tan(60.0f / 360.0f * 3.14159f) * 0.1f;
    float fW = fH * aspect;
    glFrustum(-fW, fW, -fH, fH, 0.1f, 100.0f);

    // Handle Mouse Input
    if (ImGui::IsWindowHovered()) {
        ImGuiIO& io = ImGui::GetIO();
        m_camDist -= io.MouseWheel * 1.5f;
        if (m_camDist < 1.0f) m_camDist = 1.0f;
        
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            m_camYaw += io.MouseDelta.x * 0.5f;
            m_camPitch += io.MouseDelta.y * 0.5f;
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            m_camTargetX -= io.MouseDelta.x * 0.01f;
            m_camTargetY += io.MouseDelta.y * 0.01f;
        }
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Position Camera via target tracking
    glTranslatef(0.0f, 0.0f, -m_camDist);
    glRotatef(m_camPitch, 1.0f, 0.0f, 0.0f);
    glRotatef(m_camYaw, 0.0f, 1.0f, 0.0f);
    glTranslatef(-m_camTargetX, -m_camTargetY, -m_camTargetZ);

    // Lighting setup — ambient + two directional lights for good shading
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Global ambient
    float globalAmb[] = {0.3f, 0.3f, 0.3f, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmb);

    // Light 0: key light from upper-right-front
    float light0Pos[]     = {5.0f, -8.0f, 5.0f, 0.0f};
    float light0Diffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
    float light0Amb[]     = {0.15f, 0.15f, 0.15f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light0Pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light0Diffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, light0Amb);

    // Light 1: fill light from lower-left-back
    float light1Pos[]     = {-3.0f, 4.0f, -5.0f, 0.0f};
    float light1Diffuse[] = {0.4f, 0.4f, 0.45f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, light1Pos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1Diffuse);
    
    // Draw Grid
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    glColor3f(0.3f, 0.3f, 0.3f);
    for (float i = -10; i <= 10; i += 1.0f) {
        glVertex3f(i, 0, -10); glVertex3f(i, 0, 10);
        glVertex3f(-10, 0, i); glVertex3f(10, 0, i);
    }
    glEnd();

    // Draw Map Points with pixel colors
    // Convert from CV convention (Y-down, Z-forward) to GL (Y-up, Z-backward)
    auto pts = slam.getMapPoints();
    glPointSize(3.0f);
    glBegin(GL_POINTS);
    for (const auto& p : pts) {
        glColor3f(p.r, p.g, p.b);
        glVertex3f(p.position.x(), -p.position.y(), -p.position.z());
    }
    glEnd();

    // Draw Drone Pose
    // Full CV→GL conversion: diag(1,-1,-1) flips Y (up/down) and Z (fixes mirrored yaw).
    // Similarity transform cvToGl * pose * cvToGl converts rotation + translation consistently.
    Eigen::Matrix4f pose = slam.getPose();
    Eigen::Matrix4f cvToGl = Eigen::Matrix4f::Identity();
    cvToGl(1, 1) = -1.0f;  // flip Y
    cvToGl(2, 2) = -1.0f;  // flip Z (fixes yaw mirroring)
    Eigen::Matrix4f glPose = cvToGl * pose * cvToGl;
    
    glPushMatrix();
    glMultMatrixf(glPose.data());
    
    if (m_modelLoaded) {
        glEnable(GL_LIGHTING);
        // Apply configurable scaling and rotation
        glScalef(m_modelScale, m_modelScale, m_modelScale);
        glRotatef(m_modelRotX, 1.0f, 0.0f, 0.0f);
        glRotatef(m_modelRotY + 90.0f, 0.0f, 1.0f, 0.0f); // +90° to align model forward after CV→GL Z-flip
        glRotatef(m_modelRotZ, 0.0f, 0.0f, 1.0f);
        
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_NORMAL_ARRAY);
        glEnableClientState(GL_COLOR_ARRAY);
        glVertexPointer(3, GL_FLOAT, sizeof(ModelVertex), &m_modelVertices[0].x);
        glNormalPointer(GL_FLOAT, sizeof(ModelVertex), &m_modelVertices[0].nx);
        glColorPointer(3, GL_FLOAT, sizeof(ModelVertex), &m_modelVertices[0].r);
        glDrawElements(GL_TRIANGLES, m_modelIndices.size(), GL_UNSIGNED_INT, m_modelIndices.data());
        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_NORMAL_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisable(GL_LIGHTING);
    } else {
        // Draw drone axes fallback
        glBegin(GL_LINES);
        glColor3f(1, 0, 0); glVertex3f(0,0,0); glVertex3f(1,0,0); // X red
        glColor3f(0, 1, 0); glVertex3f(0,0,0); glVertex3f(0,1,0); // Y green
        glColor3f(0, 0, 1); glVertex3f(0,0,0); glVertex3f(0,0,1); // Z blue
        glEnd();
    }
    glPopMatrix();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHT0);
    glDisable(GL_LIGHT1);
    glDisable(GL_COLOR_MATERIAL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return m_texture;
}
