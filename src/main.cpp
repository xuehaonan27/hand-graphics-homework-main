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

// View
// Camera quaternion-based control
static glm::vec3 camera_position = glm::vec3(0.0f, 0.0f, 15.0f);
static glm::quat camera_orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
static glm::quat target_orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

// Mouse control
static double last_x = 400, last_y = 400;
static bool first_mouse = true;
static float yaw = -90.0f;   // Initial yaw facing -Z
static float pitch = 0.0f;

// Camera movement speed
static float camera_speed = 0.1f;
static float mouse_sensitivity = 0.1f;
static float slerp_factor = 0.15f; // Smooth interpolation factor

// Keyboard state
static bool key_w_pressed = false;
static bool key_a_pressed = false;
static bool key_s_pressed = false;
static bool key_d_pressed = false;
static bool key_space_pressed = false;
static bool key_shift_pressed = false;

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
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        std::cout << "Keyboard/mouse control: " << (keyboard_mouse_enabled ? "ENABLED" : "DISABLED") << std::endl;
    }

    // Camera movement keys (WASD + Space + Shift)
    if (keyboard_mouse_enabled) {
        if (key == GLFW_KEY_W) key_w_pressed = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_A) key_a_pressed = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_S) key_s_pressed = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_D) key_d_pressed = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_SPACE) key_space_pressed = (action != GLFW_RELEASE);
        if (key == GLFW_KEY_LEFT_SHIFT) key_shift_pressed = (action != GLFW_RELEASE);
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
        last_x = xpos;
        last_y = ypos;
        first_mouse = false;
        return;
    }

    float xoffset = (xpos - last_x) * mouse_sensitivity;
    float yoffset = (last_y - ypos) * mouse_sensitivity; // Reversed: y-coordinates go from bottom to top
    
    last_x = xpos;
    last_y = ypos;

    yaw += xoffset;
    pitch += yoffset;

    // Constrain pitch
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    // Convert yaw and pitch to quaternion
    glm::quat qPitch = glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat qYaw = glm::angleAxis(glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));

    // Combine rotations: yaw first, then pitch
    target_orientation = qYaw * qPitch;
}

static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    if (keyboard_mouse_enabled) {
        // Adjust movement speed with scroll
        camera_speed += yoffset * 0.02f;
        if (camera_speed < 0.01f) camera_speed = 0.01f;
        if (camera_speed > 1.0f) camera_speed = 1.0f;
        std::cout << "Camera speed: " << camera_speed << std::endl;
    }
}

static void update_camera_position() {
    if (!keyboard_mouse_enabled)
        return;

    // Get camera forward, right, and up vectors from quaternion
    glm::mat4 rotation_matrix = glm::mat4_cast(camera_orientation);
    glm::vec3 forward = -glm::vec3(rotation_matrix[2]); // -Z axis
    glm::vec3 right = glm::vec3(rotation_matrix[0]);    // X axis
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);         // World up

    // Process movement
    if (key_w_pressed)
        camera_position += forward * camera_speed;
    if (key_s_pressed)
        camera_position -= forward * camera_speed;
    if (key_a_pressed)
        camera_position -= right * camera_speed;
    if (key_d_pressed)
        camera_position += right * camera_speed;
    if (key_space_pressed)
        camera_position += up * camera_speed;
    if (key_shift_pressed)
        camera_position -= up * camera_speed;
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
    std::cout << "  Scroll: Adjust movement speed" << std::endl;
    std::cout << "  Z/X/C/V/B: Control fingers" << std::endl;
    std::cout << "======================\n" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        passed_time = (float) glfwGetTime();

        // --- You may edit below ---
        // Update camera position based on keyboard input
        update_camera_position();

        // Smooth quaternion interpolation (SLERP)
        camera_orientation = glm::slerp(camera_orientation, target_orientation, slerp_factor);

        // Example: Rotate the hand
        // * turn around every 4 seconds
        float metacarpals_angle = passed_time * (M_PI / 4.0f);
        // * target = metacarpals
        // * rotation axis = (1, 0, 0)
        modifier["metacarpals"] = glm::rotate(glm::identity<glm::mat4>(), metacarpals_angle, glm::fvec3(1.0, 0.0, 0.0));

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

        glm::fmat4 mvp;
        if (!keyboard_mouse_enabled) {
            // Original orthographic view
            mvp = glm::ortho(-12.5f * ratio, 12.5f * ratio, -5.f, 20.f, -20.f, 20.f)
                         *
                         glm::lookAt(glm::fvec3(.0f, .0f, -1.f), glm::fvec3(.0f, .0f, .0f), glm::fvec3(.0f, 1.f, .0f));
        } else {
            // Quaternion-based camera view
            glm::mat4 rotation_matrix = glm::mat4_cast(camera_orientation);
            glm::vec3 forward = -glm::vec3(rotation_matrix[2]);
            glm::vec3 target = camera_position + forward;
            glm::vec3 up = glm::vec3(rotation_matrix[1]);
            glm::mat4 view = glm::lookAt(camera_position, target, up);
            glm::mat4 projection = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 100.0f);
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
