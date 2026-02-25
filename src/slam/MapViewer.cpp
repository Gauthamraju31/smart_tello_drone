#include "MapViewer.h"
#include <iostream>

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

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Camera looking down slightly at the origin
    glTranslatef(0.0f, -2.0f, -5.0f);
    glRotatef(30.0f, 1.0f, 0.0f, 0.0f); // tilt down

    // Draw Grid
    glBegin(GL_LINES);
    glColor3f(0.3f, 0.3f, 0.3f);
    for (float i = -10; i <= 10; i += 1.0f) {
        glVertex3f(i, 0, -10); glVertex3f(i, 0, 10);
        glVertex3f(-10, 0, i); glVertex3f(10, 0, i);
    }
    glEnd();

    // Draw Map Points
    auto pts = slam.getMapPoints();
    glPointSize(3.0f);
    glBegin(GL_POINTS);
    glColor3f(1.0f, 1.0f, 1.0f);
    for (const auto& p : pts) {
        glVertex3f(p.x(), -p.y(), p.z()); // Invert Y for rendering
    }
    glEnd();

    // Draw Drone Pose
    Eigen::Matrix4f pose = slam.getPose();
    glPushMatrix();
    // Multiply by pose (Eigen is column major, GL is column major)
    glMultMatrixf(pose.data());
    
    // Draw drone axes
    glBegin(GL_LINES);
    glColor3f(1, 0, 0); glVertex3f(0,0,0); glVertex3f(1,0,0); // X red
    glColor3f(0, 1, 0); glVertex3f(0,0,0); glVertex3f(0,1,0); // Y green
    glColor3f(0, 0, 1); glVertex3f(0,0,0); glVertex3f(0,0,1); // Z blue
    glEnd();
    glPopMatrix();

    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return m_texture;
}
