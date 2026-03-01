#pragma once

#include "SLAMEngine.h"
#include <GL/gl.h>
#include <vector>
#include <string>

struct ModelVertex {
    float x, y, z;
    float nx, ny, nz;
    float r, g, b;
};

class MapViewer {
public:
    MapViewer();
    ~MapViewer();

    // Renders the SLAM map into an FBO, returns the texture ID for ImGui
    GLuint renderToTexture(int width, int height, const SLAMEngine& slam);
    
    // Load GLB model for the drone
    bool loadModel(const std::string& path);

    // Model transform tweaks
    float m_modelScale = 1.0f;
    float m_modelRotX = 0.0f;
    float m_modelRotY = 0.0f;
    float m_modelRotZ = 0.0f;

private:
    GLuint m_fbo = 0;
    GLuint m_texture = 0;
    GLuint m_rbo = 0;
    
    int m_width = 0;
    int m_height = 0;

    void updateFBO(int width, int height);
    
    // Model data
    std::vector<ModelVertex> m_modelVertices;
    std::vector<unsigned int> m_modelIndices;
    bool m_modelLoaded = false;
    
    // Navigation state
    float m_camDist = 5.0f;
    float m_camPitch = 30.0f;
    float m_camYaw = 0.0f;
    float m_camTargetX = 0.0f;
    float m_camTargetY = -2.0f;
    float m_camTargetZ = 0.0f;
};
