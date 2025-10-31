// Hand Example
// Author: Yi Kangrui <yikangrui@pku.edu.cn>

//#define DIFFUSE_TEXTURE_MAPPING

#include "gl_env.h"

#include <cstdlib>
#include <cstdio>
#include <config.h>

#ifndef M_PI
#define M_PI (3.1415926535897932)
#endif

#include <iostream>

#include "skeletal_mesh.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace SkeletalAnimation {
    const char *vertex_shader_330 =
            "#version 330 core\n"
            "const int MAX_BONES = 100;\n"
            "uniform mat4 u_bone_transf[MAX_BONES];\n"
            "uniform mat4 u_mvp;\n"
            "layout(location = 0) in vec3 in_position;\n"
            "layout(location = 1) in vec2 in_texcoord;\n"
            "layout(location = 2) in vec3 in_normal;\n"
            "layout(location = 3) in ivec4 in_bone_index;\n"
            "layout(location = 4) in vec4 in_bone_weight;\n"
            "out vec2 pass_texcoord;\n"
            "void main() {\n"
            "    float adjust_factor = 0.0;\n"
            "    for (int i = 0; i < 4; i++) adjust_factor += in_bone_weight[i] * 0.25;\n"
            "    mat4 bone_transform = mat4(1.0);\n"
            "    if (adjust_factor > 1e-3) {\n"
            "        bone_transform -= bone_transform;\n"
            "        for (int i = 0; i < 4; i++)\n"
            "            bone_transform += u_bone_transf[in_bone_index[i]] * in_bone_weight[i] / adjust_factor;\n"
            "	 }\n"
            "    gl_Position = u_mvp * bone_transform * vec4(in_position, 1.0);\n"
            "    pass_texcoord = in_texcoord;\n"
            "}\n";

    const char *fragment_shader_330 =
            "#version 330 core\n"
            "uniform sampler2D u_diffuse;\n"
            "in vec2 pass_texcoord;\n"
            "out vec4 out_color;\n"
            "void main() {\n"
            #ifdef DIFFUSE_TEXTURE_MAPPING
            "    out_color = vec4(texture(u_diffuse, pass_texcoord).xyz, 1.0);\n"
            #else
            "    out_color = vec4(pass_texcoord, 0.0, 1.0);\n"
            #endif
            "}\n";
}

static double last_mouse_x = 400, last_mouse_y = 400;
static bool first_mouse = true;

struct CameraState {
    glm::vec3 position;
    glm::quat orientation;
    float fov;

    CameraState() : position(0.0f), orientation(1.0f, 0.0f, 0.0f, 0.0f), fov(45.0f) {}
    CameraState(const glm::vec3& pos, const glm::quat& orient, float f)
        : position(pos), orientation(orient), fov(f) {}
};

// Quaterion controlled camera
class QuaternionCamera {
public:
    QuaternionCamera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 15.0f)) 
        : position(position), 
          worldUp(glm::vec3(0.0f, 1.0f, 0.0f)),
          movementSpeed(25.0f),
          mouseSensitivity(0.1f),
          fov(45.0f) {
        orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        targetOrientation = orientation;
        updateVectors();
    }
    
    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }
    
    glm::mat4 getProjectionMatrix(float aspectRatio, bool usePerspective = true) const {
        if (usePerspective) {
            return glm::perspective(glm::radians(fov), aspectRatio, 0.1f, 100.0f);
        } else {
            return glm::ortho(-12.5f * aspectRatio, 12.5f * aspectRatio, -5.f, 20.f, -20.f, 20.f);
        }
    }
    
    void processKeyboard(bool w, bool a, bool s, bool d, bool space, bool shift, float deltaTime) {
        float velocity = movementSpeed * deltaTime;
        if (w) position += front * velocity;
        if (s) position -= front * velocity;
        if (a) position -= right * velocity;
        if (d) position += right * velocity;
        if (space) position += worldUp * velocity;
        if (shift) position -= worldUp * velocity;
    }
    
    void processMouseMovement(double xoffset, double yoffset, bool constrainPitch = true) {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;
        yaw += xoffset;
        pitch += yoffset;

        if (constrainPitch) {
            if (pitch > 89.0f) pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;
        }
        
        // Create quaterion from eular angle (Yaw -> Pitch)
        glm::quat qYaw = glm::angleAxis(glm::radians(yaw), worldUp);
        glm::quat qPitch = glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        targetOrientation = qYaw * qPitch;
        targetOrientation = glm::normalize(targetOrientation);
    }
    
    void updateCameraOrientation(float deltaTime) {
        // Quaterion slerp
        float slerpFactor = glm::clamp(10.0f * deltaTime, 0.01f, 0.5f);
        orientation = glm::slerp(orientation, targetOrientation, slerpFactor);
        orientation = glm::normalize(orientation);
        
        updateVectors();
    }
    
    void processMouseScroll(double yoffset) {
        fov -= (float)yoffset;
        if (fov < 1.0f) fov = 1.0f;
        if (fov > 45.0f) fov = 45.0f;
    }
    
    void setPosition(const glm::vec3& newPosition) { position = newPosition; }
    glm::vec3 getPosition() const { return position; }
    void reportPosition() const {
        std::cout << "Position: " << "[" << position[0] << ", " << position[1] << ", " << position[2] << "]" << std::endl;
    }
    void reportOrientation() const {
        std::cout << "Orientation: " << "[" << orientation[0] << ", " << orientation[1] << ", " << orientation[2] << "]" << std::endl;
    }
    glm::vec3 getFront() const { return front; }
    glm::vec3 getUp() const { return up; }
    glm::vec3 getRight() const { return right; }
    
    void setMovementSpeed(float speed) { movementSpeed = speed; }
    void incMovementSpeed() { 
        movementSpeed = movementSpeed > 50.0 ? movementSpeed : movementSpeed + 1.0;
        std::cout << "Camera movement speed: " << movementSpeed << std::endl;
    }
    void decMovementSpeed() {
        movementSpeed = movementSpeed == 0.0 ? movementSpeed : movementSpeed - 1.0;
        std::cout << "Camera movement speed: " << movementSpeed << std::endl;
    }
    void setMouseSensitivity(float sensitivity) { mouseSensitivity = sensitivity; }

    void resetOrientation(double current_x, double current_y) {
            last_mouse_x = current_x;
            last_mouse_y = current_y;

            glm::vec3 euler = glm::eulerAngles(orientation);
            yaw = glm::degrees(euler.y);
            pitch = glm::degrees(euler.x);

            targetOrientation = orientation;
        // yaw = -90.0f;
        // pitch = 0.0f;
        
        // glm::quat qYaw = glm::angleAxis(glm::radians(yaw), worldUp);
        // glm::quat qPitch = glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        // orientation = qYaw * qPitch;
        // targetOrientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        
        updateVectors();
    }

    void lookAt(const glm::vec3& target) {
        glm::vec3 direction = glm::normalize(target - position);
        
        pitch = glm::degrees(asin(direction.y));
        yaw = glm::degrees(atan2(direction.z, direction.x)) - 90.0f;
        
        glm::quat qYaw = glm::angleAxis(glm::radians(yaw), worldUp);
        glm::quat qPitch = glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
        orientation = qYaw * qPitch;
        targetOrientation = orientation;
        
        updateVectors();
    }

    CameraState getCurrentState() const {
        return CameraState(position, orientation, fov);
    }

    void setState(const CameraState& state) {
        position = state.position;
        orientation = state.orientation;
        targetOrientation = state.orientation;
        fov = state.fov;
        updateVectors();
    }
    
    CameraState getTransitionState(const CameraState& start, const CameraState& end, float progress) {
        // Smooth step function for smoother transition
        float smoothProgress = progress * progress * (3.0f - 2.0f * progress);
        
        // Linear interpolation for position
        glm::vec3 transPosition = glm::mix(start.position, end.position, smoothProgress);
        
        // Spherical linear interpolation for orientation
        glm::quat transOrientation = glm::slerp(start.orientation, end.orientation, smoothProgress);
        
        // Linear interpolation for FOV
        float transFov = glm::mix(start.fov, end.fov, smoothProgress);
        
        return CameraState(transPosition, transOrientation, transFov);
    }

private:
    void updateVectors() {
        // Extra orientation vector from quaterion
        glm::mat4 rotation = glm::mat4_cast(orientation);
        front = -glm::vec3(rotation[2]);
        right = glm::vec3(rotation[0]);
        up = glm::vec3(rotation[1]);
    }

    glm::vec3 position;
    glm::vec3 front, right, up, worldUp;
    glm::quat orientation;
    glm::quat targetOrientation;
    float yaw = -90.0f;
    float pitch = 0.0f;
    float movementSpeed;
    float mouseSensitivity;
    float fov;
};

static QuaternionCamera camera;

static CameraState stateA, stateB;
static bool stateARecorded = false;
static bool stateBRecorded = false;
static bool isTransitioning = false;
static CameraState transitionStart, transitionEnd;
static float transitionProgress = 0.0f;
static float transitionDuration = 3.0f; // NOTE: here
static bool reverseTransition = false;

enum DisplayMode {
    KeyboardMouseControl = 0,
    Completion1 = 1,
    Completion2 = 2,
    Completion3 = 3,
};

static DisplayMode current_mode = KeyboardMouseControl;
static bool keyboard_mouse_enabled = false;

// Finger status for KeyboardMouseControl
static bool thumb_bent = false;
static bool index_bent = false; 
static bool middle_bent = false;
static bool ring_bent = false;
static bool pinky_bent = false;

static void error_callback(int error, const char *description) {
    fprintf(stderr, "Error: %s\n", description);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);

    // F: switch KeyboardMouseControl
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        keyboard_mouse_enabled = !keyboard_mouse_enabled;
        if (keyboard_mouse_enabled) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            first_mouse = true;

            double current_x, current_y;
            glfwGetCursorPos(window, &current_x, &current_y);
            camera.setPosition(glm::vec3(0.0f, 0.0f, 15.0f));
            camera.resetOrientation(current_x, current_y);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            camera.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            camera.lookAt(glm::vec3(0.0f, 0.0f, -1.0f));
        }
        std::cout << "Keyboard/mouse control: " << (keyboard_mouse_enabled ? "ENABLED" : "DISABLED") << std::endl;
    }

    // Only when KeyboardMouseControl is disabled, 1, 2 and 3 will display
    if (!keyboard_mouse_enabled) {
        if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
            current_mode = Completion1;
            std::cout << "Mode: Completion 1" << std::endl;
        }
        if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
            current_mode = Completion2;
            std::cout << "Mode: Completion 2" << std::endl;
        }
        if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
            current_mode = Completion3;
            std::cout << "Mode: Completion 3" << std::endl;
        }
    }

    if (keyboard_mouse_enabled && action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_E:
                camera.incMovementSpeed();
                break;
            case GLFW_KEY_Q:
                camera.decMovementSpeed();
                break;
            case GLFW_KEY_G:
                // Record camera state A or B
                if (!stateARecorded) {
                    stateA = camera.getCurrentState();
                    stateARecorded = true;
                    std::cout << "State A recorded at position: ["
                          << stateA.position.x << ", "
                          << stateA.position.y << ", "
                          << stateA.position.z << "]" << std::endl;
                } else if (!stateBRecorded) {
                    stateB = camera.getCurrentState();
                    stateBRecorded = true;
                    std::cout << "State B recorded at position: ["
                            << stateB.position.x << ", "
                            << stateB.position.y << ", "
                            << stateB.position.z << "]" << std::endl;
                    std::cout << "Both states recorded. Press P for A->B transition, O for B->A transition." << std::endl;
                } else {
                    std::cout << "Both states already recorded. Press K to clear." << std::endl;
                }
                break;
            case GLFW_KEY_P:
                // Play transition from A to B
                if (stateARecorded && stateBRecorded) {
                    isTransitioning = true;
                    transitionStart = stateA;
                    transitionEnd = stateB;
                    transitionProgress = 0.0f;
                    reverseTransition = false;
                    std::cout << "Starting transition from A to B" << std::endl;
                } else {
                    std::cout << "Please record both states A and B first (press G twice)" << std::endl;
                }
                break;
            case GLFW_KEY_O:
                // Play transition from B to A
                if (stateARecorded && stateBRecorded) {
                    isTransitioning = true;
                    transitionStart = stateB;
                    transitionEnd = stateA;
                    transitionProgress = 0.0f;
                    reverseTransition = true;
                    std::cout << "Starting transition from B to A" << std::endl;
                } else {
                    std::cout << "Please record both states A and B first (press G twice)" << std::endl;
                }
                break;
            case GLFW_KEY_K:
                // Clear recorded states
                stateARecorded = false;
                stateBRecorded = false;
                isTransitioning = false;
                std::cout << "Cleared recorded camera states" << std::endl;
                break;
            case GLFW_KEY_Z:
                thumb_bent = !thumb_bent;
                std::cout << "Thumb: " << (thumb_bent ? "BENT" : "STRAIGHT") << std::endl;
                break;
            case GLFW_KEY_X:
                index_bent = !index_bent;
                std::cout << "Index: " << (index_bent ? "BENT" : "STRAIGHT") << std::endl;
                break;
            case GLFW_KEY_C:
                middle_bent = !middle_bent;
                std::cout << "Middle: " << (middle_bent ? "BENT" : "STRAIGHT") << std::endl;
                break;
            case GLFW_KEY_V:
                ring_bent = !ring_bent;
                std::cout << "Ring: " << (ring_bent ? "BENT" : "STRAIGHT") << std::endl;
                break;
            case GLFW_KEY_B:
                pinky_bent = !pinky_bent;
                std::cout << "Pinky: " << (pinky_bent ? "BENT" : "STRAIGHT") << std::endl;
                break;
        }
    }
}

static void cursor_position_callback(GLFWwindow *window, double xpos, double ypos) {
    if (!keyboard_mouse_enabled)
        return;

    if (first_mouse) {
        last_mouse_x = xpos;
        last_mouse_y = ypos;
        first_mouse = false;
        return;
    }

    // double xoffset = xpos - last_mouse_x;
    double xoffset = last_mouse_x - xpos; // flip X axis // -xoffset, TODO: add flip
    double yoffset = last_mouse_y - ypos; // flip Y axis
    
    last_mouse_x = xpos;
    last_mouse_y = ypos;

    camera.processMouseMovement(xoffset, yoffset);
}

static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    if (keyboard_mouse_enabled) {
        camera.processMouseScroll(yoffset);
    }
}

static void completion_1(SkeletalMesh::SkeletonModifier &modifier, float passed_time);
static void completion_2(SkeletalMesh::SkeletonModifier &modifier, float passed_time);
static void completion_3(SkeletalMesh::SkeletonModifier &modifier, float passed_time);
static void keyboard_mouse_control(SkeletalMesh::SkeletonModifier &modifier);

int main(int argc, char *argv[]) {
    GLFWwindow *window;
    GLuint vertex_shader, fragment_shader, program;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit())
        exit(EXIT_FAILURE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__ // for macos
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    window = glfwCreateWindow(800, 800, "OpenGL output", NULL, NULL);
    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    if (glewInit() != GLEW_OK)
        exit(EXIT_FAILURE);

    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &SkeletalAnimation::vertex_shader_330, NULL);
    glCompileShader(vertex_shader);

    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &SkeletalAnimation::fragment_shader_330, NULL);
    glCompileShader(fragment_shader);

    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    int linkStatus;
    if (glGetProgramiv(program, GL_LINK_STATUS, &linkStatus), linkStatus == GL_FALSE)
        std::cout << "Error occured in glLinkProgram()" << std::endl;

    SkeletalMesh::Scene &sr = SkeletalMesh::Scene::loadScene("Hand", DATA_DIR"/Hand.fbx");
    if (&sr == &SkeletalMesh::Scene::error)
        std::cout << "Error occured in loadMesh()" << std::endl;

    sr.setShaderInput(program, "in_position", "in_texcoord", "in_normal", "in_bone_index", "in_bone_weight");

    float passed_time;
    SkeletalMesh::SkeletonModifier modifier;

    glEnable(GL_DEPTH_TEST);
    
    std::cout << "\n=== Camera Controls ===" << std::endl;
    std::cout << "Press F to toggle camera control mode" << std::endl;
    std::cout << "When enabled:" << std::endl;
    std::cout << "  WASD: Move camera forward/left/backward/right" << std::endl;
    std::cout << "  Space: Move up" << std::endl;
    std::cout << "  Shift: Move down" << std::endl;
    std::cout << "  Mouse: Rotate view" << std::endl;
    std::cout << "  Scroll: Zoom in/out" << std::endl;
    std::cout << "  Q/E: Adjust movement speed" << std::endl;
    std::cout << "  Z/X/C/V/B: Control fingers" << std::endl;
    std::cout << "=== Transition Controls (when camera control mode enabled) ===" << std::endl;
    std::cout << "  G: Record camera state (first press for A, second for B)" << std::endl;
    std::cout << "  P: Play transition from A to B" << std::endl;
    std::cout << "  O: Play transition from B to A" << std::endl;
    std::cout << "  K: Clear recorded states" << std::endl;
    std::cout << "======================\n" << std::endl;

    static int ticked_time_sec = 0;

    while (!glfwWindowShouldClose(window)) {
        passed_time = (float) glfwGetTime();

        static float last_frame = 0.0f;
        float current_frame = passed_time;
        float delta_time = current_frame - last_frame;
        last_frame = current_frame;

        if (passed_time >= ticked_time_sec) {
            std::cout << "At " << passed_time << std::endl;
            ticked_time_sec += 1;
            camera.reportPosition();
            camera.reportOrientation();
        }

        // --- You may edit below ---
        // // Update camera position based on keyboard input
        // update_camera_position();

        // // Smooth quaternion interpolation (SLERP)
        // camera_orientation = glm::slerp(camera_orientation, target_orientation, slerp_factor);
        
        if (isTransitioning) {
            transitionProgress += delta_time / transitionDuration;
            if (transitionProgress >= 1.0f) {
                // Transition complete
                transitionProgress = 1.0f;
                isTransitioning = false;
                camera.setState(transitionEnd);
                std::cout << "Transition complete" << std::endl;
            } else {
                // Update camera during transition
                CameraState currentState = camera.getTransitionState(transitionStart, transitionEnd, transitionProgress);
                camera.setState(currentState);
            }
        } else {
            camera.processKeyboard(
                glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS,
                glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS,
                glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS,
                glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS,
                glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS,
                glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS,
                delta_time
            );
            camera.updateCameraOrientation(delta_time);
        }

        // Example: Rotate the hand
        // * turn around every 4 seconds
        // float metacarpals_angle = passed_time * (M_PI / 4.0f);
        // * target = metacarpals
        // * rotation axis = (1, 0, 0)
        // modifier["metacarpals"] = glm::rotate(glm::identity<glm::mat4>(), metacarpals_angle, glm::fvec3(1.0, 0.0, 0.0));

        if (keyboard_mouse_enabled) {
            current_mode = KeyboardMouseControl;
            keyboard_mouse_control(modifier);
        } else {

        /**********************************************************************************\
        *
        * To animate fingers, modify modifier["HAND_SECTION"] each frame,
        * where HAND_SECTION can only be one of the bone names in the Hand's Hierarchy.
        *
        * A virtual hand's structure is like this: (slightly DIFFERENT from the real world)
        *    5432 1
        *    ....        1 = thumb           . = fingertip
        *    |||| .      2 = index finger    | = distal phalange
        *    $$$$ |      3 = middle finger   $ = intermediate phalange
        *    #### $      4 = ring finger     # = proximal phalange
        *    OOOO#       5 = pinky           O = metacarpals
        *     OOO
        * (Hand in the real world -> https://en.wikipedia.org/wiki/Hand)
        *
        * From the structure we can infer the Hand's Hierarchy:
        *	- metacarpals
        *		- thumb_proximal_phalange
        *			- thumb_intermediate_phalange
        *				- thumb_distal_phalange
        *					- thumb_fingertip
        *		- index_proximal_phalange
        *			- index_intermediate_phalange
        *				- index_distal_phalange
        *					- index_fingertip
        *		- middle_proximal_phalange
        *			- middle_intermediate_phalange
        *				- middle_distal_phalange
        *					- middle_fingertip
        *		- ring_proximal_phalange
        *			- ring_intermediate_phalange
        *				- ring_distal_phalange
        *					- ring_fingertip
        *		- pinky_proximal_phalange
        *			- pinky_intermediate_phalange
        *				- pinky_distal_phalange
        *					- pinky_fingertip
        *
        * Notice that modifier["HAND_SECTION"] is a local transformation matrix,
        * where (1, 0, 0) is the bone's direction, and apparently (0, 1, 0) / (0, 0, 1)
        * is perpendicular to the bone.
        * Particularly, (0, 0, 1) is the rotation axis of the nearer joint.
        *
        \**********************************************************************************/

// #define EXAMPLE_CODE
#ifdef EXAMPLE_CODE
        // Example: Animate the index finger
        // * period = 2.4 seconds
        float period = 2.4f;
        float time_in_period = fmod(passed_time, period);
        // * angle: 0 -> PI/3 -> 0
        float thumb_angle = abs(time_in_period / (period * 0.5f) - 1.0f) * (M_PI / 3.0);
        // * target = proximal phalange of the index
        // * rotation axis = (0, 0, 1)
        modifier["index_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), thumb_angle,
                                                          glm::fvec3(0.0, 0.0, 1.0));
#endif // EXAMPLE_CODE

            switch (current_mode) {
                case Completion1:
                    completion_1(modifier, passed_time);
                    break;
                case Completion2:
                    completion_2(modifier, passed_time);
                    break;
                case Completion3:
                    completion_3(modifier, passed_time);
                    break;
                default:
                    break;
            }
        }

        // --- You may edit above ---

        float ratio;
        int width, height;

        glfwGetFramebufferSize(window, &width, &height);
        ratio = width / (float) height;

        glClearColor(0.5, 0.5, 0.5, 1.0);

        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);

        // glm::fmat4 mvp;
        // if (!keyboard_mouse_enabled) {
        //     // Original orthographic view
        //     mvp = glm::ortho(-12.5f * ratio, 12.5f * ratio, -5.f, 20.f, -20.f, 20.f)
        //                  *
        //                  glm::lookAt(glm::fvec3(.0f, .0f, -1.f), glm::fvec3(.0f, .0f, .0f), glm::fvec3(.0f, 1.f, .0f));
        // } else {
        //     // Quaternion-based camera view
        //     glm::mat4 rotation_matrix = glm::mat4_cast(camera_orientation);
        //     glm::vec3 forward = -glm::vec3(rotation_matrix[2]);
        //     glm::vec3 target = camera_position + forward;
        //     glm::vec3 up = glm::vec3(rotation_matrix[1]);
        //     glm::mat4 view = glm::lookAt(camera_position, target, up);
        //     glm::mat4 projection = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 100.0f);
        //     mvp = projection * view;
        // }

        glm::fmat4 mvp;
        if (!keyboard_mouse_enabled) {
            // Non keyboard-mouse mode: using ortho
            glm::mat4 view = camera.getViewMatrix();
            glm::mat4 projection = camera.getProjectionMatrix(ratio, false);
            mvp = projection * view;
        } else {
            // Keyboard-mouse mode: using perspective
            glm::mat4 view = camera.getViewMatrix();
            glm::mat4 projection = camera.getProjectionMatrix(ratio, true);
            mvp = projection * view;
        }

        glUniformMatrix4fv(glGetUniformLocation(program, "u_mvp"), 1, GL_FALSE, (const GLfloat *) &mvp);
        glUniform1i(glGetUniformLocation(program, "u_diffuse"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
        SkeletalMesh::Scene::SkeletonTransf bonesTransf;
        sr.getSkeletonTransform(bonesTransf, modifier);
        if (!bonesTransf.empty())
            glUniformMatrix4fv(glGetUniformLocation(program, "u_bone_transf"), bonesTransf.size(), GL_FALSE,
                               (float *) bonesTransf.data());
        sr.render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    SkeletalMesh::Scene::unloadScene("Hand");

    glfwDestroyWindow(window);

    glfwTerminate();
    exit(EXIT_SUCCESS);
}

static void finger_move_clear(SkeletalMesh::SkeletonModifier &modifier) {
    modifier["thumb_proximal_phalange"] = glm::identity<glm::mat4>();
    modifier["thumb_intermediate_phalange"] = glm::identity<glm::mat4>();
    modifier["thumb_distal_phalange"] = glm::identity<glm::mat4>();
    modifier["thumb_fingertip"] = glm::identity<glm::mat4>();

    modifier["index_proximal_phalange"] = glm::identity<glm::mat4>();
    modifier["index_intermediate_phalange"] = glm::identity<glm::mat4>();
    modifier["index_distal_phalange"] = glm::identity<glm::mat4>();
    modifier["index_fingertip"] = glm::identity<glm::mat4>();

    modifier["middle_proximal_phalange"] = glm::identity<glm::mat4>();
    modifier["middle_intermediate_phalange"] = glm::identity<glm::mat4>();
    modifier["middle_distal_phalange"] = glm::identity<glm::mat4>();
    modifier["middle_fingertip"] = glm::identity<glm::mat4>();
    
    modifier["ring_proximal_phalange"] = glm::identity<glm::mat4>();
    modifier["ring_intermediate_phalange"] = glm::identity<glm::mat4>();
    modifier["ring_distal_phalange"] = glm::identity<glm::mat4>();
    modifier["ring_fingertip"] = glm::identity<glm::mat4>();

    modifier["pinky_proximal_phalange"] = glm::identity<glm::mat4>();
    modifier["pinky_intermediate_phalange"] = glm::identity<glm::mat4>();
    modifier["pinky_distal_phalange"] = glm::identity<glm::mat4>();
    modifier["pinky_fingertip"] = glm::identity<glm::mat4>();
}

static void finger_move(SkeletalMesh::SkeletonModifier &modifier,
                        std::string finger, float time_in_period, float period,
                        float proximal_frac, float intermediate_frac,
                        float distal_frac, float fingertip_frac) {
  if (proximal_frac != 0.0) {
    float proximal_angle =
        abs(time_in_period / (period * 0.5f) - 1.0f) * (M_PI / proximal_frac);
    std::string proximal_s = finger + "_proximal_phalange";
    modifier[proximal_s] = glm::rotate(
        glm::identity<glm::mat4>(), proximal_angle, glm::fvec3(0.0, 0.0, 1.0));
  }

  if (intermediate_frac != 0.0) {
    float intermediate_angle = abs(time_in_period / (period * 0.5f) - 1.0f) *
                               (M_PI / intermediate_frac);
    std::string intermediate_s = finger + "_intermediate_phalange";
    modifier[intermediate_s] =
        glm::rotate(glm::identity<glm::mat4>(), intermediate_angle,
                    glm::fvec3(0.0, 0.0, 1.0));
  }

  if (distal_frac != 0.0) {
    float distal_angle =
        abs(time_in_period / (period * 0.5f) - 1.0f) * (M_PI / distal_frac);
    std::string distal_s = finger + "_distal_phalange";
    modifier[distal_s] = glm::rotate(glm::identity<glm::mat4>(), distal_angle,
                                     glm::fvec3(0.0, 0.0, 1.0));
  }

  if (fingertip_frac != 0.0) {
    float fingertip_angle =
        abs(time_in_period / (period * 0.5f) - 1.0f) * (M_PI / fingertip_frac);
    std::string fingertip_s = finger + "_fingertip_phalange";
    modifier[fingertip_s] = glm::rotate(
        glm::identity<glm::mat4>(), fingertip_angle, glm::fvec3(0.0, 0.0, 1.0));
  }
}

// Completion 1: grabing with 5 fingers
static void completion_1(SkeletalMesh::SkeletonModifier &modifier, float passed_time) {
    float period = 2.4f;
    float time_in_period = fmod(passed_time, period);

    finger_move_clear(modifier);
    finger_move(modifier, "thumb", time_in_period, period, 6.0, 12.0, 12.0, 0.0);
    finger_move(modifier, "index", time_in_period, period, 3.0, 3.0, 2.0, 0.0);
    finger_move(modifier, "middle", time_in_period, period, 3.0, 3.0, 2.0, 0.0);
    finger_move(modifier, "ring", time_in_period, period, 3.0, 3.0, 2.0, 0.0);
    finger_move(modifier, "pinky", time_in_period, period, 3.0, 3.0, 2.0, 0.0);
}

// Completion 2: OK
static void completion_2(SkeletalMesh::SkeletonModifier &modifier, float passed_time) {
    float period = 2.4f;
    float time_in_period = fmod(passed_time, period);

    finger_move_clear(modifier);
    finger_move(modifier, "thumb", time_in_period, period, 6.0, 12.0, 12.0, 12.0);
    finger_move(modifier, "index", time_in_period, period, 6.0, 6.0, 2.0, 0.0);
}

static void completion_3(SkeletalMesh::SkeletonModifier &modifier, float passed_time) {
    float period = 2.4f;
    float time_in_period = fmod(passed_time, period);

    float metacarpals_angle = abs(time_in_period / (period * 0.5f) - 1.0f) * (M_PI / 2.3f);
    // * target = metacarpals
    // * rotation axis = (1, 0, 0)
    modifier["metacarpals"] = glm::rotate(glm::identity<glm::mat4>(), metacarpals_angle, glm::fvec3(0.0, 1.0, 0.0));

    finger_move_clear(modifier);
    finger_move(modifier, "thumb", time_in_period, period, 0.0, -6.0, -4.0, 0.0);
    finger_move(modifier, "index", time_in_period, period, 3.0, 3.0, 2.0, 0.0);
    finger_move(modifier, "middle", time_in_period, period, 3.0, 3.0, 2.0, 0.0);
    finger_move(modifier, "ring", time_in_period, period, 3.0, 3.0, 2.0, 0.0);
    finger_move(modifier, "pinky", time_in_period, period, 3.0, 3.0, 2.0, 0.0);
}

static void km_finger_move(SkeletalMesh::SkeletonModifier &modifier,
                           std::string finger,
                           bool should_bent,
                           float proximal_angle,
                           float intermediate_angle,
                           float distal_angle) {
    std::string proximal_s = finger + "_proximal_phalange";
    std::string intermediate_s = finger + "_intermediate_phalange";
    std::string distal_s = finger + "_distal_phalange";

    if (should_bent) {
        modifier[proximal_s] = glm::rotate(glm::identity<glm::mat4>(), proximal_angle, glm::fvec3(0.0, 0.0, 1.0));
        modifier[intermediate_s] = glm::rotate(glm::identity<glm::mat4>(), intermediate_angle, glm::fvec3(0.0, 0.0, 1.0));
        modifier[distal_s] = glm::rotate(glm::identity<glm::mat4>(), distal_angle, glm::fvec3(0.0, 0.0, 1.0));
    } else {
        modifier[proximal_s] = glm::identity<glm::mat4>();
        modifier[intermediate_s] = glm::identity<glm::mat4>();
        modifier[distal_s] = glm::identity<glm::mat4>();
    }
}

// Control when KeyboardMouseControl
static void keyboard_mouse_control(SkeletalMesh::SkeletonModifier &modifier) {
    float bend_angle = M_PI / 3.0f;
    km_finger_move(modifier, "thumb", thumb_bent, bend_angle * 0.2, bend_angle * 0.3, bend_angle * 0.5);
    km_finger_move(modifier, "index", index_bent, bend_angle, bend_angle * 0.9, bend_angle * 0.8);
    km_finger_move(modifier, "middle", middle_bent, bend_angle, bend_angle * 0.9, bend_angle * 0.8);
    km_finger_move(modifier, "ring", ring_bent, bend_angle * 0.9, bend_angle * 0.8, bend_angle * 0.7);
    km_finger_move(modifier, "pinky", pinky_bent, bend_angle * 0.9, bend_angle * 0.8, bend_angle * 0.7);
}
