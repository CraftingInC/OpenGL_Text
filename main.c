#include <glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <cglm/cglm.h>
#include <cglm/call.h>
#include <math.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#define YES 1
#define NO 0

int width, height;
int caret;
static const char* fontpath = "FreeMono.ttf";

/**
 * Capture errors from glfw.
 */
static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

/**
 * Capture key callbacks from glfw
 */
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

char* get_file_contents(const char* path)
{
    char* buffer = 0;
    long length = 0;
    FILE* f = fopen(path, "rb");

    if(f)
    {
        fseek (f, 0, SEEK_END);
        length = ftell(f);
        fseek (f, 0, SEEK_SET);
        buffer = malloc(length);

        if(buffer)
        {
            fread(buffer, 1, length, f);
        }
    }
    buffer[length] = '\0';
    fclose(f);

    return buffer;
}

typedef struct CHARACTER_STRUCT
{
    GLuint texture;   // ID handle of the glyph texture
    vec2 size;    // Size of glyph
    float width;
    float height;
    float bearing_left;
    float bearing_top;
    GLuint advance;    // Horizontal offset to advance to next glyph
    char value;
} character_T;

character_T* caret_char;

typedef struct CHARACTER_LIST_STRUCT
{
    size_t size;
    character_T** items;
} character_list_T;

character_T* get_character(char c, const char* fontpath, int size)
{
    // FreeType
    FT_Library ft;
    // All functions return a value different than 0 whenever an error occurred
    if (FT_Init_FreeType(&ft))
        perror("ERROR::FREETYPE: Could not init FreeType Library");

    // Load font as face
    FT_Face face;
    if (FT_New_Face(ft, fontpath, 0, &face))
        perror("ERROR::FREETYPE: Failed to load font");

    // Set size to load glyphs as
    FT_Set_Pixel_Sizes(face, 0, size);

    // Disable byte-alignment restriction
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Load character glyph
    if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        perror("ERROR::FREETYTPE: Failed to load Glyph");

    // Generate texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width, face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
    // Set texture options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Now store character for later use
    character_T* character = calloc(1, sizeof(struct CHARACTER_STRUCT));
    character->texture = texture;
    character->width = face->glyph->bitmap.width;
    character->height = face->glyph->bitmap.rows;
    character->bearing_left = face->glyph->bitmap_left;
    character->bearing_top = face->glyph->bitmap_top;
    character->advance = face->glyph->advance.x;
    character->value = c;
    glBindTexture(GL_TEXTURE_2D, 0);
    // Destroy FreeType once we're finished
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    return character;
}

static character_list_T get_characters(const char* text, const char* fontpath, int size)
{
    character_list_T list;
    list.size = 0;
    list.items = (void*)0;
    for (int i = 0; i < strlen(text); i++)
    {
        character_T* character = get_character(text[i], fontpath, size);
        list.size += 1;
        if (list.items == (void*)0)
        {
            list.items = calloc(list.size, sizeof(struct CHARACTER_STRUCT*));
        } else {
            list.items = realloc(list.items, sizeof(struct CHARACTER_STRUCT*) * list.size);
        }

        list.items[list.size - 1] = character;
    }
    return list;
}

unsigned int draw_mesh(unsigned int program, unsigned int texture, float x, float y, float w, float h, unsigned int VBO, unsigned int new_vbo)
{
    if (new_vbo)
        glGenBuffers(1, &VBO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glUseProgram(program);

    GLuint vertex_location = glGetAttribLocation(program, "thevertex");
    GLuint model_location = glGetUniformLocation(program, "model");

    GLfloat vertices[6][4] = {
        { 0,     0 + h,   0.0, 0.0 },
        { 0,     0,       0.0, 1.0 },
        { 0 + w, 0,       1.0, 1.0 },

        { 0,     0 + h,   0.0, 0.0 },
        { 0 + w, 0,       1.0, 1.0 },
        { 0 + w, 0 + h,   1.0, 0.0 }
    };

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    mat4 m = GLM_MAT4_IDENTITY_INIT;

    glm_translate(m, (vec3){ x, y, 0 });

    glUniformMatrix4fv(model_location, 1, GL_FALSE, (const GLfloat*) m);

    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(vertex_location);
    glVertexAttribPointer(vertex_location, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    if (new_vbo)
        glDeleteBuffers(1, &VBO);

    return program;
}

void draw_character_list(unsigned int program, unsigned int VAO, int font_size, int scale, character_list_T character_list)
{
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    /**
     * Draw every character
     */
    float x = 10;
    float y = height - font_size;
    character_T* character2;

    for (int i = 0; i < character_list.size; i++)
    {
        character_T* character = character_list.items[i];
        if(i - 1 >= 0) {character2 = character_list.items[i - 1];}

        if(character->value == '\n')
        {
            y -= font_size;
            x = 10;
            continue;
        } else if(i - 1 >= 0 && character2->value == ' ' && x > (width - (font_size * 8)))
        {
            y -= font_size;
            x = 10;
        }

        unsigned int texture = character->texture;

        GLfloat xpos = x + character->bearing_left * scale;
        GLfloat ypos = y - (character->height - character->bearing_top) * scale;

        GLfloat w = character->width * scale;
        GLfloat h = character->height * scale;

        draw_mesh(program, texture, xpos, ypos, w, h, VBO, NO);

        if (i == caret)
            draw_mesh(program, caret_char->texture, xpos+(font_size*2), ypos, w, h, 0, YES);

        x += (character->advance >> 6) * scale;
    }

    glDeleteBuffers(1, &VBO);
}

unsigned int create_shader_program(const char* vertex_shader_text, const char* fragment_shader_text)
{
    GLuint vertex_shader, fragment_shader, program;

    int success;
    char infoLog[512];

    /**
     * Compile vertex shader and check for errors
     */
    vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        printf("Vertex Shader Error\n");
        glGetShaderInfoLog(vertex_shader, 512, NULL, infoLog);
        perror(infoLog);
    }

    /**
     * Compile fragment shader and check for errors
     */
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
    if(!success)
    {
        printf("Fragment Shader Error\n");
        glGetShaderInfoLog(fragment_shader, 512, NULL, infoLog);
        perror(infoLog);
    }

    /**
     * Create shader program and check for errors
     */
    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if(!success)
    {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        perror(infoLog);
    }

    return program;
}

int main(int argc, char* argv[])
{
    caret = 0;

    glfwSetErrorCallback(error_callback);

    /**
     * Initialize glfw to be able to use it.
     */
    if (!glfwInit())
        perror("Failed to initialize glfw.\n");

    /**
     * Setting some parameters to the window,
     * using OpenGL 3.3
     */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    /**
     * Creating our window
     */
    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL Text Test", NULL, NULL);
    if (!window)
        perror("Failed to create window.\n");

    glfwSetKeyCallback(window, key_callback);

    /**
     * Enable OpenGL as current context
     */
    glfwMakeContextCurrent(window);

    /**
     * Initialize glad and check if it works
     */
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwTerminate();
        return 0;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    caret_char = get_character('|', fontpath, 16);

    unsigned int VAO;
    glGenVertexArrays(1, &VAO);

    GLuint program_textured;
    GLint view_location, projection_location;

    /**
     * Vertex Shader
     */
    static const char* vertex_shader_text =
        "#version 330 core\n"
        "uniform mat4 model;\n"
        "uniform mat4 view;\n"
        "uniform mat4 projection;\n"
        "attribute vec4 thevertex;\n"
        "out vec2 TexCoord;\n"
        "void main()\n"
        "{\n"
        "    gl_Position = projection * view * model * vec4(thevertex.xy, 0.0, 1.0);\n"
        "    TexCoord = thevertex.zw;"
        "}\n";

    /**
     * Fragment Shader
     */
    static const char* fragment_shader_text =
        "#version 330 core\n"
        "varying vec3 color;\n"
        "in vec2 TexCoord;\n"
        "uniform sampler2D ourTexture;\n"
        "void main()\n"
        "{\n"
        "    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(ourTexture, TexCoord).r);\n"
        "    gl_FragColor = vec4(vec3(1, 1, 1), 1.0) * sampled;\n"
        "}\n";

    program_textured = create_shader_program(vertex_shader_text, fragment_shader_text);

    /**
     * Grab locations from shader
     */
    view_location = glGetUniformLocation(program_textured, "view");
    projection_location = glGetUniformLocation(program_textured, "projection");

    int font_size1 = 16;
    char* str = get_file_contents("text.txt");
    character_list_T character_list = get_characters(str, fontpath, font_size1);

    mat4 v = GLM_MAT4_IDENTITY_INIT;
    glm_translate(v, (vec3){ 0, 0, 0 });

    /**
     * Main loop
     */
    int timer = 0;

    glClearColor(0.1f, 0.2f, 0.2f, 1.0f);

    while (!glfwWindowShouldClose(window))
    {
        glBindVertexArray(VAO);

        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 p;
        glm_ortho(0.0f, width, 0, height, -10.0f, 100.0f, p);

        /**
         * Draw text
         */
        draw_character_list(program_textured, VAO, font_size1, 1.0f, character_list);

        glUniformMatrix4fv(view_location, 1, GL_FALSE, (const GLfloat*) v);
        glUniformMatrix4fv(projection_location, 1, GL_FALSE, (const GLfloat*) p);

        if (timer < 10)
        {
            timer += 1;
        }
        else
        {
            caret = caret < strlen(str) ? caret+1 : 0;
            timer = 0;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    free(str);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
