// Defined before OpenGL and GLUT includes to avoid deprecation message in OSX
#define GL_SILENCE_DEPRECATION
#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "cyCodeBase/cyTriMesh.h"
#include "cyCodeBase/cyMatrix.h"
#include "cyCodeBase/cyGL.h"
#include "cyCodeBase/cyPoint.h"
#include "lodepng.h"

int width = 1280, height = 720;
GLFWwindow* window;
cy::TriMesh teapotMesh;
cy::GLSLProgram prog;
bool doRotate = false, doZoom = false, doCameraRotate = false;
double rotX = 5.23599, rotY = 0, distZ = 40;
double lastX, lastY;
double lightX = 0, lightY = 0, lightZ = 150;
unsigned int textureWidth = 512, textureHeight = 512;
std::vector<cy::Vec3f> processedVertices, processedNormals;
std::vector<cy::Vec2f> processedTexCoords;
std::vector<unsigned char> diffuseTextureData;
std::vector<unsigned char> specularTextureData;

GLclampf Red = 0.0f, Green = 0.0f, Blue = 0.0f, Alpha = 1.0f;

void renderScene()
{
    // Clear Color buffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(Red, Green, Blue, Alpha);
    glDrawArrays(GL_TRIANGLES, 0, processedVertices.size());
}

bool compileShaders() {
    bool shaderSuccess = prog.BuildFiles("shader.vert", "shader.frag");
    return shaderSuccess;
}

void processNormalKeyCB(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GL_TRUE);
            break;
        case GLFW_KEY_F6:
            std::cout << "Recompiling shaders..." << std::endl;
            if (!compileShaders()) {
                std::cout << "Error Recompiling shaders" << std::endl;
            }
            break;
        case GLFW_KEY_LEFT_CONTROL:
        case GLFW_KEY_RIGHT_CONTROL:
            doCameraRotate = true;
            break;
        }
    } else if (action == GLFW_RELEASE) {
        switch (key) {
        case GLFW_KEY_LEFT_CONTROL:
        case GLFW_KEY_RIGHT_CONTROL:
            doCameraRotate = false;
            break;
        }
    }
}

static void error_callback(int error, const char* description)
{
    std::cerr << "Error: " << description << std::endl;
}
double deg2rad (double degrees) {
    return degrees * 4.0 * atan (1.0) / 180.0;
}

void processMouseButtonCB(GLFWwindow* window, int button, int action, int mods)
{
    double xpos, ypos;
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        doZoom = true;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        doZoom = false;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
    } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        doRotate = true;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
    } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        doRotate = false;
        glfwGetCursorPos(window, &xpos, &ypos);
        lastX = xpos;
        lastY = ypos;
    }
}

void processMousePosCB(GLFWwindow* window, double xpos, double ypos)
{
    double xDiff = lastX - xpos;
    double yDiff = lastY - ypos;
    if (doCameraRotate && doRotate) {
        lightX -= xDiff * 0.1;
        lightY += yDiff * 0.1;
    }
    // Calculate camera zoom based on mouse movement in y direction
    if (doZoom) {
        distZ += yDiff * 0.05;
    }

    // Calculate camera rotation based on mouse movement in x direction
    if (doRotate && !doCameraRotate) {
        rotX -= yDiff * 0.005;
        rotY -= xDiff * 0.005;
    }

    lastX  = xpos;
    lastY = ypos;
}

int main(int argc, char** argv)
{
    glfwSetErrorCallback(error_callback);
    
    // Initialize GLFW
    if (!glfwInit())
        exit(EXIT_FAILURE);
        
    // Create a windowed mode window and its OpenGL context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(width, height, "Texture", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window);

    //Initialize GLEW
    glewExperimental = GL_TRUE; 
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        std::cout << "Error: " << glewGetErrorString(err) << std::endl;
        return 1;
    }

    // Print and test OpenGL context infos
    std::cout << glGetString(GL_VERSION) << std::endl;
    std::cout << glGetString(GL_RENDERER) << std::endl;

    // Setup GLFW callbacks
    glfwSetKeyCallback(window, processNormalKeyCB);
    glfwSetMouseButtonCallback(window, processMouseButtonCB);
    glfwSetCursorPosCallback(window, processMousePosCB);
    glfwSwapInterval(1);

    //OpenGL initializations
    // glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    // CY_GL_REGISTER_DEBUG_CALLBACK;

    //Load object mesh and texture mapping
    bool meshSuccess = teapotMesh.LoadFromFileObj(argv[1]);
    if (!meshSuccess)
    {
        std::cout << "Error loading mesh" << std::endl;
        return 1;
    }

    //Load vertices for object faces
    for (int i = 0; i < teapotMesh.NF(); i++) 
    {
        cy::TriMesh::TriFace face = teapotMesh.F(i);
        cy::TriMesh::TriFace textureface = teapotMesh.FT(i);
        for (int j = 0; j < 3; j++) 
        {
            cy::Vec3f pos = teapotMesh.V(face.v[j]);
            processedVertices.push_back(pos);

            cy::Vec3f norm = teapotMesh.VN(face.v[j]);
            processedNormals.push_back(norm);

            cy::Vec3f texcoord = teapotMesh.VT(textureface.v[j]);
            processedTexCoords.push_back(cy::Vec2f(texcoord.x, texcoord.y));
        }
    }

    // Load diffuse texture data from png file
    bool ambientTextureError = lodepng::decode(diffuseTextureData, textureWidth, textureHeight, teapotMesh.M(0).map_Kd.data);
    if (ambientTextureError)
    {
        std::cout << "Error loading texture" << std::endl;
        return 1;
    }

    // Load sepcular texture data from png file
    bool specularTextureError = lodepng::decode(specularTextureData, textureWidth, textureHeight, teapotMesh.M(0).map_Ks.data);
    if (specularTextureError)
    {
        std::cout << "Error loading texture" << std::endl;
        return 1;
    }

    //Compute bounding box
    teapotMesh.ComputeBoundingBox();
    cy::Vec3f teapotCenter = (teapotMesh.GetBoundMax() - teapotMesh.GetBoundMin())/2;
    cy::Vec3f teapotOrigin = teapotMesh.GetBoundMin() + teapotCenter;

    //Compute normals
    teapotMesh.ComputeNormals();

    //Create vertex array object
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    //Create VBO for vertex data
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedVertices.size(), &processedVertices[0], GL_STATIC_DRAW);
    
    //Create VBO for normal data
    GLuint nbo;
    glGenBuffers(1, &nbo);
    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec3f)*processedNormals.size(), &processedNormals[0], GL_STATIC_DRAW);

    //Create VBO for texture data
    GLuint tbo;
    glGenBuffers(1, &tbo);
    glBindBuffer(GL_ARRAY_BUFFER, tbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cy::Vec2f)*processedTexCoords.size(), &processedTexCoords[0], GL_STATIC_DRAW);

    //Send diffuse texture data to GPU
    GLuint diffuseTexID;
    glGenTextures(1, &diffuseTexID);
    glBindTexture(GL_TEXTURE_2D, diffuseTexID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, &diffuseTextureData[0]);
    
    //Set diffuse texture parameters
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    //Send specular texture data to GPU
    GLuint specularTexID;
    glGenTextures(1, &specularTexID);
    glBindTexture(GL_TEXTURE_2D, specularTexID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, &specularTextureData[0]);

    //Set ambient texture parameters
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);


    //Setup Shader program
    std::cout << "Compiling shaders..." << std::endl;
    bool shaderSuccess = compileShaders();
    if (!shaderSuccess)
    {
        std::cout << "Error compiling shaders" << std::endl;
        return 1;
    }

    //Loop until the user closes the window
    while (!glfwWindowShouldClose(window))
    {
        // Calculate camera transformations
        cy::Matrix3f rotXMatrix = cy::Matrix3f::RotationX(rotX);
        cy::Matrix3f rotYMatrix = cy::Matrix3f::RotationY(rotY);
        cy::Matrix4f transMatrix = cy::Matrix4f::Translation(-teapotOrigin);
        cy::Matrix4f view = cy::Matrix4f::View(cy::Vec3f(0, 0, distZ), cy::Vec3f(0, 0, 0), cy::Vec3f(0, 1, 0));
        cy::Matrix4f projMatrix = cy::Matrix4f::Perspective(deg2rad(60), float(width)/float(height), 0.1f, 1000.0f);
        cy::Matrix4f mvp = projMatrix * view * rotXMatrix * rotYMatrix * transMatrix;
        cy::Matrix4f mv = view * rotXMatrix * rotYMatrix * transMatrix;
        cy::Matrix3f mv_3x3_it = mv.GetSubMatrix3().GetInverse().GetTranspose();
        
        //Set Program and Program Attributes
        prog.Bind();
        prog["mvp"] = mvp;
        prog["mv"] = mv;
        prog["mv_3x3_it"] = mv_3x3_it;
        prog["camera_pos"] = cy::Vec3f(0, 0, distZ);
        prog["light_pos"] =  view * cy::Vec4f(lightX, lightY, lightZ, 1);
        prog.SetAttribBuffer("pos", vbo, 3);
        prog.SetAttribBuffer("norm", nbo, 3);
        prog.SetAttribBuffer("txc", tbo, 2);
        GLint diffuseTexLoc = glGetUniformLocation(prog.GetID(), "texture_diffuse");
        GLint specularTexLoc = glGetUniformLocation(prog.GetID(), "texture_specular");
        glUniform1i(diffuseTexLoc, 0);
        glUniform1i(specularTexLoc, 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseTexID);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, specularTexID);
        
        renderScene();

        // Swap front and back buffers
        glfwSwapBuffers(window);

        // Poll for and process events
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}