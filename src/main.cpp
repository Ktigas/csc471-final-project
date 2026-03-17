/*
 * Program 4 - Introduction to CG: Virtual Camera
 * Swan Lake Scene - free camera (WASD + scroll look), skybox, texture mapping,
 * Blinn-Phong lighting, hierarchical flower animation, Bezier tour (G key).
 * CPE 471 - based on starter by Z. Wood + S. Sueda + I. Dunn
 */

#include <iostream>
#include <glad/glad.h>

#include "GLSL.h"
#include "Program.h"
#include "Shape.h"
#include "MatrixStack.h"
#include "WindowManager.h"
#include "Texture.h"
#include <float.h>
#include "particleSys.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader/tiny_obj_loader.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace std;
using namespace glm;

// Cubic Bezier helper
static vec3 bezier(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t)
{
    float u  = 1.0f - t;
    float u2 = u  * u;
    float u3 = u2 * u;
    float t2 = t  * t;
    float t3 = t2 * t;
    return u3*p0 + 3.0f*u2*t*p1 + 3.0f*u*t2*p2 + t3*p3;
}

class Application : public EventCallbacks
{
public:

    WindowManager *windowManager = nullptr;

    shared_ptr<Program> prog;    // Blinn-Phong
    shared_ptr<Program> texProg; // texture-mapped

    // Meshes
    shared_ptr<Shape>         bunny;
    shared_ptr<Shape>         ballerina;
    shared_ptr<Shape>         swan;
    shared_ptr<Shape>         stage;
    shared_ptr<Shape>         mountain;
    shared_ptr<Shape>         waterlily;
    vector<shared_ptr<Shape>> treeParts;
    vector<shared_ptr<Shape>> flowerParts;

    // Ground plane (CPU data → GPU, texture mapped)
    GLuint GrndBuffObj, GrndNorBuffObj, GrndTexBuffObj, GIndxBuffObj;
    int    g_GiboLen;
    GLuint GroundVertexArrayID;
    shared_ptr<Texture> texture0;

    // Sky sphere
    shared_ptr<Shape>   skyBox;
    shared_ptr<Texture> skyTex;

    // Stage texture
    shared_ptr<Texture> stageTex;

    // Mountain + tree textures
    shared_ptr<Texture> mountainTex;
    shared_ptr<Texture> treeTex;

    // Particle system
    shared_ptr<Program> partProg;
    particleSys* thePartSystem = nullptr;
    shared_ptr<Texture> particleTex;

    // Particle timing
    float partTime = 0.0f;
    vec3 swanPosition = vec3(0, -2.0f, 0); // Track swan position
    bool prevShowSwan = false; // Track previous swan state for edge detection
    float particleDelayTimer = 0.0f; // Timer for particle delay
    bool particlesActive = false; // Whether particles are active

    // Camera
    float phi   = 0.0f;
    float theta = -glm::pi<float>();

    vec3  eyePosition = vec3(0.0f, 0.0f, 10.0f);
    vec3  front       = vec3(0.0f, 0.0f, -1.0f);
    vec3  right;
    vec3  upCam       = vec3(0.0f, 1.0f, 0.0f);
    vec3  center;

    float moveSpeed = 0.15f;

    // Lighting
    float lightTrans = 0.0f;

    // Animation
    float sTheta = 0.0f;
    float eTheta = 0.0f;
    float hTheta = 0.0f;
    bool  mToggle = false;

    // Bezier tour
    bool  tourActive = false;
    float tourT      = 0.0f;
    float tourSpeed  = 0.06f;
    vec3  tourLookAt = vec3(0, 0, 0);

    vec3 tourPts[7] = {
        vec3( 14,  4,  10),
        vec3(-14,  8,  10),
        vec3(-14,  2, -10),
        vec3(  0,  1,  -6),
        vec3( 14,  0,  -2),
        vec3( 14,  6,   4),
        vec3( 14,  4,  10),
    };

    // Delta-time tracking
    double prevTime = 0.0;

    void keyCallback(GLFWwindow *window, int key, int scancode,
                     int action, int mods) override
    {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GL_TRUE);

        if (key == GLFW_KEY_Q && action == GLFW_PRESS) lightTrans -= 0.5f;
        if (key == GLFW_KEY_E && action == GLFW_PRESS) lightTrans += 0.5f;

        if (key == GLFW_KEY_M && action == GLFW_PRESS) mToggle = !mToggle;

        if (key == GLFW_KEY_Z && action == GLFW_PRESS)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        if (key == GLFW_KEY_Z && action == GLFW_RELEASE)
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        if (key == GLFW_KEY_G && action == GLFW_PRESS) {
            tourActive = !tourActive;
            if (tourActive) tourT = 0.0f;
        }

        if (!tourActive) {
            if (key == GLFW_KEY_W && (action == GLFW_PRESS || action == GLFW_REPEAT))
                eyePosition += moveSpeed * front;
            if (key == GLFW_KEY_S && (action == GLFW_PRESS || action == GLFW_REPEAT))
                eyePosition -= moveSpeed * front;
            if (key == GLFW_KEY_A && (action == GLFW_PRESS || action == GLFW_REPEAT))
                eyePosition -= right * moveSpeed;
            if (key == GLFW_KEY_D && (action == GLFW_PRESS || action == GLFW_REPEAT))
                eyePosition += right * moveSpeed;
        }
    }

    void mouseCallback(GLFWwindow *window, int button,
                       int action, int mods) override {}

    void scrollCallback(GLFWwindow *window, double deltaX, double deltaY) override
    {
        if (tourActive) return;
        float sensitivity = 0.05f;
        theta += (float)deltaX * sensitivity;
        phi   += (float)deltaY * sensitivity;
        phi = glm::clamp(phi, glm::radians(-80.0f), glm::radians(80.0f));
    }

    void resizeCallback(GLFWwindow *window, int width, int height) override
    {
        glViewport(0, 0, width, height);
    }

    void init(const std::string& resourceDirectory)
    {
        GLSL::checkVersion();
        glClearColor(0.05f, 0.05f, 0.15f, 1.0f);
        glEnable(GL_DEPTH_TEST);

        // Blinn-Phong shader
        prog = make_shared<Program>();
        prog->setVerbose(true);
        prog->setShaderNames(
            resourceDirectory + "/simple_vert.glsl",
            resourceDirectory + "/simple_frag.glsl");
        prog->init();
        prog->addUniform("P");
        prog->addUniform("V");
        prog->addUniform("M");
        prog->addUniform("MatAmb");
        prog->addUniform("MatDif");
        prog->addUniform("MatSpec");
        prog->addUniform("MatShine");
        prog->addUniform("lightPos");
        prog->addAttribute("vertPos");
        prog->addAttribute("vertNor");
        prog->addUniform("viewPos");

        // Texture shader
        texProg = make_shared<Program>();
        texProg->setVerbose(true);
        texProg->setShaderNames(
            resourceDirectory + "/tex_vert.glsl",
            resourceDirectory + "/tex_frag0.glsl");
        texProg->init();
        texProg->addUniform("P");
        texProg->addUniform("V");
        texProg->addUniform("M");
        texProg->addUniform("Texture0");
        texProg->addUniform("flip");
        texProg->addAttribute("vertPos");
        texProg->addAttribute("vertNor");
        texProg->addAttribute("vertTex");
        texProg->addUniform("lightPos"); 


        // Textures


        // Ground (lake)
        texture0 = make_shared<Texture>();
        texture0->setFilename(resourceDirectory + "/lake.jpeg");
        texture0->init();
        texture0->setUnit(0);
        texture0->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        // Sky
        skyTex = make_shared<Texture>();
        skyTex->setFilename(resourceDirectory + "/nightsky.jpg");
        skyTex->init();
        skyTex->setUnit(0);
        skyTex->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        // Stage
        stageTex = make_shared<Texture>();
        stageTex->setFilename(resourceDirectory + "/grass.jpeg");
        stageTex->init();
        stageTex->setUnit(0);
        stageTex->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        // Mountain texture 
        mountainTex = make_shared<Texture>();
        mountainTex->setFilename(resourceDirectory + "/hill.jpg");
        mountainTex->init();
        mountainTex->setUnit(0);
        mountainTex->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        // Tree texture 
        treeTex = make_shared<Texture>();
        treeTex->setFilename(resourceDirectory + "/darkgrass.jpg");
        treeTex->init();
        treeTex->setUnit(0);
        treeTex->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

        // Particles
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        partProg = make_shared<Program>();
        partProg->setVerbose(true);
        partProg->setShaderNames(
            resourceDirectory + "/lab10_vert.glsl",
            resourceDirectory + "/lab10_frag.glsl");

        if (partProg->init()) {
            partProg->addUniform("P");
            partProg->addUniform("V");
            partProg->addUniform("M");
            partProg->addUniform("alphaTexture");
            partProg->addUniform("pColor");
            partProg->addAttribute("vertPos");
            partProg->addAttribute("vertColor");

            particleTex = make_shared<Texture>();
            particleTex->setFilename(resourceDirectory + "/alpha.bmp");
            particleTex->init();
            particleTex->setUnit(0);
            particleTex->setWrapModes(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

            thePartSystem = new particleSys(vec3(0, 1.0f, 0));
            thePartSystem->gpuSetup();
        } else {
            cerr << "WARNING: Particle shaders not found — particles disabled." << endl;
        }
    }

    void initGeom(const std::string& resourceDirectory)
    {
        vector<tinyobj::shape_t>    TOshapes;
        vector<tinyobj::material_t> objMaterials;
        string errStr;
        bool   rc;

        // bunny (compute normals if missing)
        TOshapes.clear();
        rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr,
                              (resourceDirectory + "/bunnyNoNorm.obj").c_str());
        if (!rc) {
            cerr << "Failed to load bunnyNoNorm.obj: " << errStr << endl;
        } else {
            if (TOshapes[0].mesh.normals.empty()) {
                const auto& positions = TOshapes[0].mesh.positions;
                const auto& indices   = TOshapes[0].mesh.indices;
                auto&       normals   = TOshapes[0].mesh.normals;
                normals.resize(positions.size(), 0.0f);
                for (size_t i = 0; i < indices.size(); i += 3) {
                    unsigned int i0 = indices[i], i1 = indices[i+1], i2 = indices[i+2];
                    vec3 v0(positions[3*i0], positions[3*i0+1], positions[3*i0+2]);
                    vec3 v1(positions[3*i1], positions[3*i1+1], positions[3*i1+2]);
                    vec3 v2(positions[3*i2], positions[3*i2+1], positions[3*i2+2]);
                    vec3 fn = normalize(cross(v1-v0, v2-v0));
                    for (unsigned int k : {i0, i1, i2}) {
                        normals[3*k]   += fn.x;
                        normals[3*k+1] += fn.y;
                        normals[3*k+2] += fn.z;
                    }
                }
                for (size_t i = 0; i < normals.size(); i += 3) {
                    vec3  n(normals[i], normals[i+1], normals[i+2]);
                    float l = length(n);
                    if (l > 0.0f) { n /= l; normals[i]=n.x; normals[i+1]=n.y; normals[i+2]=n.z; }
                    else          { normals[i+1] = 1.0f; }
                }
            }
            bunny = make_shared<Shape>(false);
            bunny->createShape(TOshapes[0]);
            bunny->measure();
            bunny->init();
        }

        // ballerina
        TOshapes.clear();
        rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr,
                              (resourceDirectory + "/ballerina.obj").c_str());
        if (!rc) cerr << errStr << endl;
        else {
            ballerina = make_shared<Shape>(false);
            ballerina->createShape(TOshapes[0]);
            ballerina->measure();
            ballerina->init();
        }

        // swan
        TOshapes.clear();
        rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr,
                              (resourceDirectory + "/swan.obj").c_str());
        if (!rc) cerr << errStr << endl;
        else {
            swan = make_shared<Shape>(false);
            swan->createShape(TOshapes[0]);
            swan->measure();
            swan->init();
        }

        // waterlily
        TOshapes.clear();
        rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr,
                              (resourceDirectory + "/waterlilyleaf.obj").c_str());
        if (!rc) cerr << errStr << endl;
        else {
            waterlily = make_shared<Shape>(false);
            waterlily->createShape(TOshapes[0]);
            waterlily->measure();
            waterlily->init();
        }

        // stage
        TOshapes.clear();
        rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr,
                              (resourceDirectory + "/stage.obj").c_str());
        if (!rc) cerr << errStr << endl;
        else {
            stage = make_shared<Shape>(true);
            stage->createShape(TOshapes[0]);
            stage->measure();
            stage->init();
        }

        // flower (multi-shape)
        TOshapes.clear();
        rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr,
                              (resourceDirectory + "/cartoon_flower.obj").c_str());
        if (!rc) cerr << errStr << endl;
        else {
            for (size_t i = 0; i < TOshapes.size(); i++) {
                auto part = make_shared<Shape>(false);
                part->createShape(TOshapes[i]);
                part->measure();
                part->init();
                flowerParts.push_back(part);
            }
        }

        // tree 
        TOshapes.clear();
        rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr,
                            (resourceDirectory + "/gentree.obj").c_str());
        if (!rc) cerr << errStr << endl;
        else {
            for (size_t i = 0; i < TOshapes.size(); i++) {
                auto part = make_shared<Shape>(true);
                part->createShape(TOshapes[i]);
                part->measure();
                part->init();
                treeParts.push_back(part);
            }
        }

        // mountain 
        TOshapes.clear();
        rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr,
                              (resourceDirectory + "/mountain.obj").c_str());
        if (!rc) cerr << errStr << endl;
        else {
            mountain = make_shared<Shape>(true);
            mountain->createShape(TOshapes[0]);
            mountain->measure();
            mountain->init();
        }

        initGround();

        // sky sphere
        TOshapes.clear();
        rc = tinyobj::LoadObj(TOshapes, objMaterials, errStr,
                              (resourceDirectory + "/sphereWTex.obj").c_str());
        if (!rc) {
            cerr << "Failed to load sphereWTex.obj: " << errStr << endl;
        } else {
            skyBox = make_shared<Shape>(true);
            skyBox->createShape(TOshapes[0]);
            skyBox->measure();
            skyBox->init();
        }
    }

    void initGround()
    {
        float g_groundSize = 20.0f;
        float g_groundY    = -3.5f;

        float GrndPos[] = {
            -g_groundSize, g_groundY, -g_groundSize,
            -g_groundSize, g_groundY,  g_groundSize,
             g_groundSize, g_groundY,  g_groundSize,
             g_groundSize, g_groundY, -g_groundSize
        };
        float GrndNorm[] = { 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0, 0,1,0 };
        static GLfloat GrndTex[] = { 0,0, 0,1, 1,1, 1,0 };
        unsigned short idx[] = { 0,1,2, 0,2,3 };

        glGenVertexArrays(1, &GroundVertexArrayID);
        glBindVertexArray(GroundVertexArrayID);
        g_GiboLen = 6;

        glGenBuffers(1, &GrndBuffObj);
        glBindBuffer(GL_ARRAY_BUFFER, GrndBuffObj);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GrndPos), GrndPos, GL_STATIC_DRAW);

        glGenBuffers(1, &GrndNorBuffObj);
        glBindBuffer(GL_ARRAY_BUFFER, GrndNorBuffObj);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GrndNorm), GrndNorm, GL_STATIC_DRAW);

        glGenBuffers(1, &GrndTexBuffObj);
        glBindBuffer(GL_ARRAY_BUFFER, GrndTexBuffObj);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GrndTex), GrndTex, GL_STATIC_DRAW);

        glGenBuffers(1, &GIndxBuffObj);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GIndxBuffObj);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    }

    // drawGround 
    void drawGround(shared_ptr<Program> curS)
    {
        glBindVertexArray(GroundVertexArrayID);
        texture0->bind(curS->getUniform("Texture0"));

        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, GrndBuffObj);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, GrndNorBuffObj);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

        glEnableVertexAttribArray(2);
        glBindBuffer(GL_ARRAY_BUFFER, GrndTexBuffObj);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GIndxBuffObj);
        glDrawElements(GL_TRIANGLES, g_GiboLen, GL_UNSIGNED_SHORT, 0);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        texture0->unbind();
    }

    void SetMaterial(shared_ptr<Program> curS, int i)
    {
        switch (i) {
            case 0: // light pink
                glUniform3f(curS->getUniform("MatAmb"),  0.25f, 0.15f, 0.15f);
                glUniform3f(curS->getUniform("MatDif"),  1.00f, 0.70f, 0.75f);
                glUniform3f(curS->getUniform("MatSpec"), 0.50f, 0.40f, 0.45f);
                glUniform1f(curS->getUniform("MatShine"), 32.0f);
                break;
            case 1: // dark pink velvet
                glUniform3f(curS->getUniform("MatAmb"),  0.05f, 0.00f, 0.02f);
                glUniform3f(curS->getUniform("MatDif"), 0.35f, 0.10f, 0.20f);
                glUniform3f(curS->getUniform("MatSpec"), 0.25f, 0.10f, 0.15f);
                glUniform1f(curS->getUniform("MatShine"), 8.0f);
                break;
            case 2: // rose gold
                glUniform3f(curS->getUniform("MatAmb"),  0.25f, 0.12f, 0.10f);
                glUniform3f(curS->getUniform("MatDif"),  0.80f, 0.45f, 0.40f);
                glUniform3f(curS->getUniform("MatSpec"), 0.70f, 0.50f, 0.45f);
                glUniform1f(curS->getUniform("MatShine"), 64.0f);
                break;
            case 4: // pearl
                glUniform3f(curS->getUniform("MatAmb"),  0.25f, 0.20f, 0.20f);
                glUniform3f(curS->getUniform("MatDif"),  1.00f, 0.83f, 0.83f);
                glUniform3f(curS->getUniform("MatSpec"), 0.30f, 0.30f, 0.30f);
                glUniform1f(curS->getUniform("MatShine"), 11.3f);
                break;
            case 5: // ice blue
                glUniform3f(curS->getUniform("MatAmb"),  0.05f, 0.10f, 0.20f);
                glUniform3f(curS->getUniform("MatDif"),  0.50f, 0.75f, 1.00f);
                glUniform3f(curS->getUniform("MatSpec"), 0.70f, 0.85f, 1.00f);
                glUniform1f(curS->getUniform("MatShine"), 96.0f);
                break;
            case 6: // green stem
                glUniform3f(curS->getUniform("MatAmb"),  0.10f, 0.20f, 0.10f);
                glUniform3f(curS->getUniform("MatDif"),  0.20f, 0.50f, 0.20f);
                glUniform3f(curS->getUniform("MatSpec"), 0.30f, 0.30f, 0.30f);
                glUniform1f(curS->getUniform("MatShine"), 16.0f);
                break;
            case 7: // purple petals
                glUniform3f(curS->getUniform("MatAmb"),  0.25f, 0.15f, 0.25f);
                glUniform3f(curS->getUniform("MatDif"),  0.90f, 0.60f, 0.90f);
                glUniform3f(curS->getUniform("MatSpec"), 0.40f, 0.40f, 0.40f);
                glUniform1f(curS->getUniform("MatShine"), 64.0f);
                break;
            case 8: // yellow center
                glUniform3f(curS->getUniform("MatAmb"),  0.30f, 0.20f, 0.10f);
                glUniform3f(curS->getUniform("MatDif"),  0.80f, 0.60f, 0.20f);
                glUniform3f(curS->getUniform("MatSpec"), 0.50f, 0.50f, 0.50f);
                glUniform1f(curS->getUniform("MatShine"), 32.0f);
                break;
        }
    }

    void setModel(shared_ptr<Program> curS, shared_ptr<MatrixStack> M)
    {
        glUniformMatrix4fv(curS->getUniform("M"), 1, GL_FALSE,
                           value_ptr(M->topMatrix()));
    }

    void render()
    {
        int width, height;
        glfwGetFramebufferSize(windowManager->getHandle(), &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        double t  = glfwGetTime();
        partTime = (float)t;
        float  dt = (float)(t - prevTime);
        prevTime  = t;

        float aspect = width / (float)height;

        // Tour update
        if (tourActive) {
            tourT += tourSpeed * dt * 2.0f;
            if (tourT > 2.0f) tourT = 0.0f;

            int   seg  = (int)tourT;
            float segT = tourT - (float)seg;

            vec3 p0 = tourPts[seg * 3];
            vec3 p1 = tourPts[seg * 3 + 1];
            vec3 p2 = tourPts[seg * 3 + 2];
            vec3 p3 = tourPts[seg * 3 + 3];

            eyePosition = bezier(p0, p1, p2, p3, segT);
        }

        // View direction
        vec3 direction;
        direction.x = cos(phi) * sin(theta);
        direction.y = sin(phi);
        direction.z = cos(phi) * cos(theta);
        direction = normalize(direction);

        center = tourActive ? tourLookAt : eyePosition + direction;

        upCam = vec3(0.0f, 1.0f, 0.0f);
        front = normalize(center - eyePosition);
        right = normalize(cross(front, upCam));

        // Matrices
        auto Projection = make_shared<MatrixStack>();
        auto View       = make_shared<MatrixStack>();
        auto Model      = make_shared<MatrixStack>();

        Projection->pushMatrix();
        Projection->perspective(45.0f, aspect, 0.01f, 200.0f);

        View->pushMatrix();
        View->loadIdentity();
        View->lookAt(eyePosition, center, upCam);

        vec3 lightPos = eyePosition + vec3(0.0f, 1.5f + lightTrans, 0.0f) + (front * 0.5f);

        prog->bind();
        glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
        glUniformMatrix4fv(prog->getUniform("V"), 1, GL_FALSE, value_ptr(View->topMatrix()));
        glUniform3fv(prog->getUniform("lightPos"), 1, value_ptr(lightPos));
        glUniform3fv(prog->getUniform("viewPos"),  1, value_ptr(eyePosition));

        // Bunny
        if (bunny) {
            vec3  sc  = (bunny->max + bunny->min) * 0.5f;
            float ext = max({bunny->max.x-bunny->min.x,
                             bunny->max.y-bunny->min.y,
                             bunny->max.z-bunny->min.z});
            Model->pushMatrix();
            Model->translate(vec3(0.0f, -3.0f, 5.0f));
            Model->rotate(radians(90.0f), vec3(0, 1, 0));
            Model->scale(vec3(1.0f / ext));
            Model->translate(-sc);
            SetMaterial(prog, 4);
            setModel(prog, Model);
            bunny->draw(prog);
            Model->popMatrix();
        }

        float cycle = fmod((float)t, 30.0f);
        bool showSwan = cycle < 15.0f;
        bool showBall = cycle >= 15.0f;

        // Center swan - show during first half of cycle
        if (showSwan && swan) {
            vec3 sc = (swan->max + swan->min) * 0.5f;
            float ext = max({swan->max.x - swan->min.x,
                            swan->max.y - swan->min.y,
                            swan->max.z - swan->min.z});
            
            // Update swan position for particles
            swanPosition = vec3(0, -2.5f, 0);
            
            Model->pushMatrix();
            Model->translate(swanPosition);
            Model->rotate(radians(-90.0f), vec3(1, 0, 0));
            Model->scale(vec3(2.0f / ext));
            Model->translate(-sc);
            SetMaterial(prog, 4);
            setModel(prog, Model);
            swan->draw(prog);
            Model->popMatrix();
        }

        // Ballerina - show during second half of cycle (NOT permanent)
        if (showBall && ballerina) {
            // Rise effect each time she appears
            float riseT = glm::clamp((cycle - 15.0f) / 3.0f, 0.0f, 1.0f);
            float scale = glm::mix(0.01f, 1.0f, riseT);
            float yOff = glm::mix(-3.5f, -1.5f, riseT);
            
            vec3 sc = (ballerina->max + ballerina->min) * 0.5f;
            float ext = max({ballerina->max.x - ballerina->min.x,
                            ballerina->max.y - ballerina->min.y,
                            ballerina->max.z - ballerina->min.z});
            
            Model->pushMatrix();
            Model->translate(vec3(0, yOff, 0));
            Model->rotate((float)t * 0.8f, vec3(0, 1, 0));
            Model->rotate(radians(-90.0f), vec3(1, 0, 0));
            Model->scale(vec3(scale * 4.0f / ext));
            Model->translate(-sc);
            SetMaterial(prog, mToggle ? 1 : 0);
            setModel(prog, Model);
            ballerina->draw(prog);
            Model->popMatrix();
        }


        // Side swans
        if (swan) {
            vec3  sc  = (swan->max + swan->min) * 0.5f;
            float ext = max({swan->max.x-swan->min.x,
                             swan->max.y-swan->min.y,
                             swan->max.z-swan->min.z});
            auto drawSwan = [&](vec3 pos, float yRot) {
                Model->pushMatrix();
                Model->translate(pos);
                Model->rotate(yRot, vec3(0, 1, 0));
                Model->rotate(radians(-90.0f), vec3(1, 0, 0));
                Model->scale(vec3(1.8f / ext));
                Model->translate(-sc);
                SetMaterial(prog, 4);
                setModel(prog, Model);
                swan->draw(prog);
                Model->popMatrix();
            };
            drawSwan(vec3( 3.0f, -2.5f, 2.0f), radians(-30.0f));
            drawSwan(vec3(-3.0f, -2.5f, 2.0f), radians( 30.0f));
        }

        // waterlily
        if (waterlily) {
            vec3  sc  = (waterlily->max + waterlily->min) * 0.5f;
            float ext = max({waterlily->max.x-waterlily->min.x,
                             waterlily->max.y-waterlily->min.y,
                             waterlily->max.z-waterlily->min.z});
            auto drawWaterlily = [&](vec3 pos, float yRot) {
                Model->pushMatrix();
                Model->translate(pos);
                Model->rotate(yRot, vec3(0, 1, 0));
                Model->rotate(radians(-90.0f), vec3(1, 0, 0));
                Model->scale(vec3(1.8f / ext));
                Model->translate(-sc);
                SetMaterial(prog, 6);
                setModel(prog, Model);
                waterlily->draw(prog);
                Model->popMatrix();
            };
            // Left cluster
            drawWaterlily(vec3(-10.0f, -3.4f,-10.0f), radians(-30.0f));
            drawWaterlily(vec3(-11.0f, -3.4f, -5.0f), radians( 30.0f));
            drawWaterlily(vec3(-12.0f, -3.4f, -7.5f), radians( 15.0f));
            drawWaterlily(vec3( -8.0f, -3.4f, -9.0f), radians( 45.0f));
            drawWaterlily(vec3(-13.0f, -3.4f, -3.0f), radians(-20.0f));
            // Right cluster
            drawWaterlily(vec3( 10.0f, -3.4f,-10.0f), radians( 30.0f));
            drawWaterlily(vec3( 11.0f, -3.4f, -5.0f), radians(-30.0f));
            drawWaterlily(vec3( 12.0f, -3.4f, -7.5f), radians(-15.0f));
            drawWaterlily(vec3(  8.0f, -3.4f, -9.0f), radians(-45.0f));
            drawWaterlily(vec3( 13.0f, -3.4f, -3.0f), radians( 20.0f));
            // Front scatter
            drawWaterlily(vec3( -6.0f, -3.4f,  10.0f), radians( 10.0f));
            drawWaterlily(vec3(  5.0f, -3.4f,  9.0f), radians(-10.0f));
            drawWaterlily(vec3(  0.0f, -3.4f, 10.0f), radians( 60.0f));
            drawWaterlily(vec3( -8.0f, -3.4f,  6.0f), radians( 80.0f));
            drawWaterlily(vec3(  7.0f, -3.4f,  7.0f), radians(-80.0f));
        }


        // Flowers (hierarchical animation)
        if (!flowerParts.empty()) {
            vec3 fMin(FLT_MAX), fMax(-FLT_MAX);
            for (auto& p : flowerParts) {
                fMin = glm::min(fMin, p->min);
                fMax = glm::max(fMax, p->max);
            }
            vec3  fc       = (fMax + fMin) * 0.5f;
            float fex      = max({fMax.x-fMin.x, fMax.y-fMin.y, fMax.z-fMin.z});
            float stemH    = fMax.y - fMin.y;
            float scaleFac = 1.5f / fex;

            vec3 stemBase = vec3(0, fMin.y,               0);
            vec3 stemTip  = vec3(0, fMin.y + stemH*0.65f, 0);

            float fPos[][3] = {
                {-8,-2.5f,2},{-6,-2.5f,-1},{-4,-2.5f,3},{-2,-3.0f,3},
                { 2,-2.5f,-2},{ 4,-2.5f,3},{ 6,-2.5f,-1},{ 8,-2.5f,2}
            };

            for (int i = 0; i < 8; i++) {
                float bob = 0.15f * (float)sin(t * 2.0f + i * 0.5f);

                Model->pushMatrix();
                Model->translate(vec3(fPos[i][0], fPos[i][1] + bob, fPos[i][2]));
                Model->scale(vec3(scaleFac));

                Model->pushMatrix();
                float stemSway = 0.3f * sTheta * (float)sin(i * 0.9f + 0.3f);
                Model->translate(-stemBase);
                Model->rotate(stemSway, vec3(0, 0, 1));
                Model->translate(stemBase);

                Model->pushMatrix();
                Model->translate(-fc);
                SetMaterial(prog, 6);
                setModel(prog, Model);
                flowerParts[0]->draw(prog);
                Model->popMatrix();

                Model->pushMatrix();
                float headBob  = 0.4f * eTheta * (float)sin(i * 0.7f + 1.0f);
                float headSpin = (float)t * (0.3f + (i % 4) * 0.1f);
                Model->translate(-stemTip);
                Model->rotate(headBob,  vec3(1, 0, 0));
                Model->rotate(headSpin, vec3(0, 1, 0));
                Model->translate(stemTip);

                Model->pushMatrix();
                Model->translate(-fc);
                SetMaterial(prog, 7);
                setModel(prog, Model);
                flowerParts[1]->draw(prog);
                Model->popMatrix();

                if (flowerParts.size() > 2) {
                    Model->pushMatrix();
                    Model->translate(-fc);
                    SetMaterial(prog, 8);
                    setModel(prog, Model);
                    flowerParts[2]->draw(prog);
                    Model->popMatrix();
                }

                Model->popMatrix();
                Model->popMatrix();
                Model->popMatrix();
            }
        }

        prog->unbind();

        texProg->bind();
        glUniformMatrix4fv(texProg->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
        glUniformMatrix4fv(texProg->getUniform("V"), 1, GL_FALSE, value_ptr(View->topMatrix()));
        glUniform3fv(texProg->getUniform("lightPos"), 1, value_ptr(lightPos));


        // Stage
        if (stage) {
            vec3  sc  = (stage->max + stage->min) * 0.5f;
            float ext = max({stage->max.x - stage->min.x,
                             stage->max.y - stage->min.y,
                             stage->max.z - stage->min.z});
            Model->pushMatrix();
            Model->translate(vec3(0.0f, -3.5f, 1.0f));
            Model->rotate(radians(-90.0f), vec3(1, 0, 0));
            Model->scale(vec3(15.0f / ext));
            Model->translate(-sc);
            glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE,
                               value_ptr(Model->topMatrix()));
            stageTex->bind(texProg->getUniform("Texture0"));
            stage->draw(texProg);
            stageTex->unbind();
            Model->popMatrix();
        }

        // Mountain
        if (mountain) {
            vec3  sc  = (mountain->max + mountain->min) * 0.5f;
            float ext = max({mountain->max.x - mountain->min.x,
                             mountain->max.y - mountain->min.y,
                             mountain->max.z - mountain->min.z});
            Model->pushMatrix();
            Model->translate(vec3(5.0f, -2.0f, -10.0f));
            Model->rotate(radians(90.0f), vec3(0, 1, 0));
            Model->scale(vec3(15.0f / ext));
            Model->translate(-sc);
            glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE,
                               value_ptr(Model->topMatrix()));
            mountainTex->bind(texProg->getUniform("Texture0"));
            mountain->draw(texProg);
            mountainTex->unbind();
            Model->popMatrix();
        }

        // Trees
        if (!treeParts.empty()) {
            vec3 tMin(FLT_MAX), tMax(-FLT_MAX);
            for (auto &p : treeParts) {
                tMin = glm::min(tMin, p->min);
                tMax = glm::max(tMax, p->max);
            }
            vec3  sc  = (tMax + tMin) * 0.5f;
            float ext = max({tMax.x-tMin.x, tMax.y-tMin.y, tMax.z-tMin.z});
            float treeScale = 6.0f / ext;

            treeTex->bind(texProg->getUniform("Texture0"));

            auto drawTree = [&](float x, float z) {
                Model->pushMatrix();
                Model->translate(vec3(x, -1.0f, z));
                Model->scale(vec3(treeScale));
                Model->translate(-sc);
                glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE,
                                value_ptr(Model->topMatrix()));
                for (auto &p : treeParts) p->draw(texProg);
                Model->popMatrix();
            };

            // Original trees
            for (float x = -18.0f; x <= 18.0f; x += 6.0f) {
                drawTree(x,  18.0f);
                drawTree(x, -18.0f);
            }
            for (float z = -12.0f; z <= 12.0f; z += 6.0f) {
                drawTree(-18.0f, z);
                drawTree( 18.0f, z);
            }
            
            // Second ring of trees (closer to center)
            for (float x = -15.0f; x <= 15.0f; x += 10.0f) {
                drawTree(x,  12.0f);
                drawTree(x, -12.0f);
            }
            for (float z = -9.0f; z <= 9.0f; z += 2.0f) {
                drawTree(-15.0f, z);
                drawTree( 15.0f, z);
            }
            
            // Corner trees (diagonal positions)
            float corners[] = {-16.0f, -8.0f, 8.0f, 16.0f};
            for (float cx : corners) {
                for (float cz : corners) {
                    if (abs(cx) == 16.0f && abs(cz) == 16.0f) {
                        drawTree(cx, cz);  // Add trees at the far corners
                    }
                }
            }

            treeTex->unbind();
        }

        // Ground 
        glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE,
                           value_ptr(mat4(1.0f)));
        drawGround(texProg);


        // Sky Sphere 
        glDepthMask(GL_FALSE);
        texProg->bind();
        glUniform1i(texProg->getUniform("flip"), 0);
        glUniformMatrix4fv(texProg->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
        glUniformMatrix4fv(texProg->getUniform("V"), 1, GL_FALSE, value_ptr(View->topMatrix()));
        glUniformMatrix4fv(texProg->getUniform("M"), 1, GL_FALSE,
            value_ptr(glm::scale(
                glm::translate(glm::mat4(1.0f), vec3(0.0f, 15.0f, 0.0f)),
                glm::vec3(28.0f)
            )));
        skyTex->bind(texProg->getUniform("Texture0"));
        if (skyBox) skyBox->draw(texProg);
        skyTex->unbind();
        texProg->unbind();
        glDepthMask(GL_TRUE);



        texProg->unbind();


        // Update animation state
        sTheta = (float)sin(t * 1.4);
        eTheta = std::max(0.0f, (float)sin(t * 2.4));
        hTheta = std::max(0.0f, (float)cos(t * 1.2));


        // Particles 
        if (thePartSystem && partProg) {
            // Detect when swan appears (transition from false to true)
            if (showSwan && !prevShowSwan) {
                // Reset particle system when swan appears
                thePartSystem->reSet();
                particleDelayTimer = 0.0f;
            }
            
            // Update timer - particles activate after swan appears
            if (showSwan) {
                particleDelayTimer += dt;
            }
            
            // Particles are active
            bool shouldBeActive = (particleDelayTimer > 1.0f) && (showSwan || showBall);
            
            // Once particles become active, keep them active throughout the cycle
            if (shouldBeActive) {
                particlesActive = true;
            }
    
            prevShowSwan = showSwan;
            
            partProg->bind();
        
            
            glPointSize(particlesActive ? 80.0f : 10.0f);

            glUniformMatrix4fv(partProg->getUniform("P"), 1, GL_FALSE, value_ptr(Projection->topMatrix()));
            glUniformMatrix4fv(partProg->getUniform("V"), 1, GL_FALSE, value_ptr(View->topMatrix()));

            Model->pushMatrix();
            
            // Position particles at swan location (even when ballerina is showing)
            vec3 particlePos = swanPosition + vec3(0.0f, -0.5f, 0.0f);
            if (!particlesActive) {
                particlePos = vec3(0, -10.0f, 0); // Hide when inactive
            }
            
            Model->translate(particlePos);
            Model->scale(vec3(particlesActive ? 5.0f : 1.0f));
            
            glUniformMatrix4fv(partProg->getUniform("M"), 1, GL_FALSE, value_ptr(Model->topMatrix()));

            particleTex->bind(partProg->getUniform("alphaTexture"));
            thePartSystem->setCamera(View->topMatrix());
            thePartSystem->update();

            glDepthMask(GL_FALSE);
            thePartSystem->drawMe(partProg);
            glDepthMask(GL_TRUE);

            particleTex->unbind();
            Model->popMatrix();

            glPointSize(10.0f);
            partProg->unbind();
        }

        Projection->popMatrix();
        View->popMatrix();
    }
};

int main(int argc, char *argv[])
{
    std::string resourceDir = "../resources";
    if (argc >= 2) resourceDir = argv[1];

    Application *application = new Application();
    WindowManager *windowManager = new WindowManager();
    windowManager->init(800, 600);

    glfwSetInputMode(windowManager->getHandle(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    windowManager->setEventCallbacks(application);
    application->windowManager = windowManager;

    application->init(resourceDir);
    application->initGeom(resourceDir);

    application->prevTime = glfwGetTime();

    glfwSetWindowUserPointer(windowManager->getHandle(), application);

    glfwSetScrollCallback(windowManager->getHandle(),
        [](GLFWwindow* w, double dx, double dy) {
            Application* app = static_cast<Application*>(glfwGetWindowUserPointer(w));
            app->scrollCallback(w, dx, dy);
        });

    while (!glfwWindowShouldClose(windowManager->getHandle()))
    {
        application->render();
        glfwSwapBuffers(windowManager->getHandle());
        glfwPollEvents();
    }

    windowManager->shutdown();
    return 0;
}