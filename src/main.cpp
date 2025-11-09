#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "shader.h"
#include "camera.h"
#include "animator.h"
#include "model_animation.h"
#include "filesystem.h"

#include <iostream>
#ifdef _WIN32
#include <direct.h>
#endif

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 1.0f, 5.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window, Animator &animator, glm::vec3 &characterPos, float &characterRotation);

// Global animation pointers
Animation* runAnim_ptr = nullptr;
Animation* jumpAnim_ptr = nullptr;
Animation* slideAnim_ptr = nullptr;
Animation* g_currentAnimation = nullptr;  // Track current animation to prevent restarting

enum class AnimationState
{
    Running,
    Jumping,
    Sliding
};

AnimationState g_currentState = AnimationState::Running;

void SwitchAnimation(Animator& animator, AnimationState newState);

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH,SCR_HEIGHT,"Character Animation Control - WASD to move, Space to jump, Shift to slide",NULL,NULL);
    if(!window){
        std::cout<<"Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window,framebuffer_size_callback);
    glfwSetCursorPosCallback(window,mouse_callback);
    glfwSetScrollCallback(window,scroll_callback);

    glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_DISABLED);

    if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)){
        std::cout<<"Failed to initialize GLAD\n";
        return -1;
    }

    // Tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
    stbi_set_flip_vertically_on_load(true);

    glEnable(GL_DEPTH_TEST);

    // Resolve the animation assets from the new assets directory
    const std::string runAnimationPath   = FileSystem::getPath("assets/Run.dae");
    const std::string jumpAnimationPath  = FileSystem::getPath("assets/Jump.dae");
    const std::string slideAnimationPath = FileSystem::getPath("assets/Slide.dae");
    std::string shaderVSPath = FileSystem::getPath("shaders/anim_model.vs");
    std::string shaderFSPath = FileSystem::getPath("shaders/anim_model.fs");
    
    // Load shader first
    Shader ourShader(shaderVSPath.c_str(), shaderFSPath.c_str());

    // Load models using relative paths

    Model ourModel(runAnimationPath);
    Animation runAnimation(runAnimationPath, &ourModel);
    Animation jumpAnimation(jumpAnimationPath, &ourModel);
    Animation slideAnimation(slideAnimationPath, &ourModel);
    
    // Set global pointers
    runAnim_ptr = &runAnimation;
    jumpAnim_ptr = &jumpAnimation;
    slideAnim_ptr = &slideAnimation;
    
    Animator animator(runAnim_ptr);
    
    // Character position and rotation
    glm::vec3 characterPos(0.0f, -0.5f, 0.0f);
    float characterRotation = 0.0f;
    
    // Initialize with idle animation
    animator.PlayAnimation(runAnim_ptr);
    g_currentAnimation = runAnim_ptr;
    g_currentState = AnimationState::Running;

    while(!glfwWindowShouldClose(window)){
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window, animator, characterPos, characterRotation);
        animator.UpdateAnimation(deltaTime);

        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        ourShader.use();

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH/(float)SCR_HEIGHT, 0.1f,100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection",projection);
        ourShader.setMat4("view",view);

        auto transforms = animator.GetFinalBoneMatrices();
        for(int i=0;i<transforms.size();++i)
            ourShader.setMat4("finalBonesMatrices["+std::to_string(i)+"]",transforms[i]);

        glm::mat4 model=glm::mat4(1.0f);
        model=glm::translate(model, characterPos);
        model=glm::rotate(model, characterRotation, glm::vec3(0.0f, 1.0f, 0.0f));
        model=glm::scale(model,glm::vec3(0.5f));
        ourShader.setMat4("model",model);

        ourModel.Draw(ourShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window, Animator &animator, glm::vec3 &characterPos, float &characterRotation)
{
    if(glfwGetKey(window,GLFW_KEY_ESCAPE)==GLFW_PRESS)
        glfwSetWindowShouldClose(window,true);

    // Character movement controls
    float moveSpeed = 2.0f * deltaTime;
    
    if(glfwGetKey(window,GLFW_KEY_W)==GLFW_PRESS) {
        // Move forward
        characterPos.z -= moveSpeed * cos(characterRotation);
        characterPos.x -= moveSpeed * sin(characterRotation);
    }
    if(glfwGetKey(window,GLFW_KEY_S)==GLFW_PRESS) {
        // Move backward  
        characterPos.z += moveSpeed * cos(characterRotation);
        characterPos.x += moveSpeed * sin(characterRotation);
    }
    if(glfwGetKey(window,GLFW_KEY_A)==GLFW_PRESS) {
        // Turn left
        characterRotation += 2.0f * deltaTime;
    }
    if(glfwGetKey(window,GLFW_KEY_D)==GLFW_PRESS) {
        // Turn right
        characterRotation -= 2.0f * deltaTime;
    }
    
    AnimationState desiredState = AnimationState::Running;

    if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        desiredState = AnimationState::Jumping;
    }
    else if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
    {
        desiredState = AnimationState::Sliding;
    }

    SwitchAnimation(animator, desiredState);
    
    // Camera controls (keep original camera movement for better viewing)
    if(glfwGetKey(window,GLFW_KEY_UP)==GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if(glfwGetKey(window,GLFW_KEY_DOWN)==GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if(glfwGetKey(window,GLFW_KEY_LEFT)==GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if(glfwGetKey(window,GLFW_KEY_RIGHT)==GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow* window,int width,int height){ glViewport(0,0,width,height);}
void mouse_callback(GLFWwindow* window,double xpos,double ypos){
    if(firstMouse){ lastX=xpos; lastY=ypos; firstMouse=false; }
    float xoffset=xpos-lastX;
    float yoffset=lastY-ypos;
    lastX=xpos; lastY=ypos;
    camera.ProcessMouseMovement(xoffset,yoffset);
}
void scroll_callback(GLFWwindow* window,double xoffset,double yoffset){ camera.ProcessMouseScroll(yoffset);}

void SwitchAnimation(Animator& animator, AnimationState newState)
{
    if(newState == g_currentState)
        return;

    Animation* targetAnimation = nullptr;
    switch(newState)
    {
        case AnimationState::Running:
            targetAnimation = runAnim_ptr;
            break;
        case AnimationState::Jumping:
            targetAnimation = jumpAnim_ptr;
            break;
        case AnimationState::Sliding:
            targetAnimation = slideAnim_ptr;
            break;
    }

    if(targetAnimation && targetAnimation != g_currentAnimation)
    {
        animator.PlayAnimation(targetAnimation);
        g_currentAnimation = targetAnimation;
        g_currentState = newState;
    }
}
