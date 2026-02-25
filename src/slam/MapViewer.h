#pragma once

#include "SLAMEngine.h"
#include <GL/gl.h>

class MapViewer {
public:
    MapViewer();
    ~MapViewer();

    // Renders the SLAM map into an FBO, returns the texture ID for ImGui
    GLuint renderToTexture(int width, int height, const SLAMEngine& slam);

private:
    GLuint m_fbo = 0;
    GLuint m_texture = 0;
    GLuint m_rbo = 0;
    
    int m_width = 0;
    int m_height = 0;

    void updateFBO(int width, int height);
};
