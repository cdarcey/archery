#include <GLFW/glfw3.h>
#include <stdio.h>

static void error_callback(int error, const char* description);
static void cursor_position_callback(GLFWwindow* window, double xPos, double yPos);
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);


int main(void)
{
    
    glfwSetErrorCallback(error_callback);

    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Monopoly", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);

        /* Buffer swap interval */
        glfwSwapInterval(1);

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();

        
        glfwSetCursorPosCallback(window, cursor_position_callback);

        glfwSetMouseButtonCallback(window, mouse_button_callback);

        /* Press esc to close window */
        glfwSetKeyCallback(window, key_callback);
    }

    glfwTerminate();
    return 0;
}

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

/* Key press input */
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

/* Mouse input */
static void cursor_position_callback(GLFWwindow* window, double xPos, double yPos)
{
    // printf("X pos: %.1f : ", xPos);
    // printf("Y pos: %.1f \n", xPos);
}

/* Handle mouse button press */
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        printf("Mouse click \n");
}