// std
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <string>

// opengl
#include <glad/glad.h>

// glfw
#include <GLFW/glfw3.h>

// glm
#include <glm/glm.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// threading
#include <tbb/parallel_for.h>

// include
#include "input.h"
#include "particle.h"

// other
Input input;

// buffers
std::vector<std::vector<int>> neighbor_buffer;
std::vector<float> position_buffer;
std::vector<float> pressure_buffer;

// opengl
GLuint vao;
GLuint position_vbo;
GLuint pressure_vbo;
GLuint program;
glm::mat4 projection;

// solver parameters
float GRAVITY = -10;
float REST_DENSITY = 300;
float GAS_CONSTANT = 2000;
float KERNEL_RADIUS = 16;
float KERNEL_RADIUS_SQR = KERNEL_RADIUS * KERNEL_RADIUS;
float PARTICLE_MASS = 2.5f;
float VISCOSITY = 200;
float INTIGRATION_TIMESTEP = 0.0007f;

// smoothing kernels and gradients
float POLY6 = 4.f / (glm::pi<float>() * pow(KERNEL_RADIUS, 8.f));
float SPIKY_GRAD = -10.f / (glm::pi<float>() * pow(KERNEL_RADIUS, 5.f));
float VISC_LAP = 40.f / (glm::pi<float>() * pow(KERNEL_RADIUS, 5.f));

// simulation boundary
float BOUNDARY_EPSILON = KERNEL_RADIUS;
float BOUND_DAMPING = -0.5f;

// neighbour constants
int MAX_NEIGHBORS = 10;



// particles
std::vector<Particle> particles;
int MAX_PARTICLES = 4000;

// projection
int WINDOW_WIDTH = 1400;
int WINDOW_HEIGHT = 800;

float RandomValue()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(gen);
}

// grid for spatial hashing
float CELL_SIZE = KERNEL_RADIUS;
int GRID_WIDTH = int(WINDOW_WIDTH / CELL_SIZE) + 1;
int GRID_HEIGHT = int(WINDOW_HEIGHT / CELL_SIZE) + 1;
std::vector<std::vector<int>> grid;

inline int GetCellIndex(int x, int y)
{
    return y * GRID_WIDTH + x;
}

inline glm::ivec2 GetCell(const glm::vec2& pos)
{
    return glm::ivec2(int(pos.x / CELL_SIZE), int(pos.y / CELL_SIZE));
}

void BuildGrid()
{
    grid.clear();
    grid.resize(GRID_WIDTH * GRID_HEIGHT);
    for (int i = 0; i < particles.size(); ++i)
    {
        glm::ivec2 cell = GetCell(particles[i].position);
        cell.x = glm::clamp(cell.x, 0, GRID_WIDTH - 1);
        cell.y = glm::clamp(cell.y, 0, GRID_HEIGHT - 1);
        grid[GetCellIndex(cell.x, cell.y)].push_back(i);
    }
}

inline std::vector<int>& GetNearNeighborsMTBF(Particle& particle, int index)
{
    auto& neighbors = neighbor_buffer[index];
    neighbors.clear();
    
    glm::ivec2 particle_cell = GetCell(particle.position);

    int range = int(ceil(KERNEL_RADIUS / CELL_SIZE));

    for (int offset_x = -range; offset_x <= range; ++offset_x)
    {
        for (int offset_y = -range; offset_y <= range; ++offset_y)
        {
            // calculate cell index
            int cell_x = glm::clamp(particle_cell.x + offset_x, 0, GRID_WIDTH - 1);
            int cell_y = glm::clamp(particle_cell.y + offset_y, 0, GRID_HEIGHT - 1);
            int cell_index = GetCellIndex(cell_x, cell_y);
            
            // for each particle in cell
            for (int j : grid[cell_index])
            {
                // calculate if effected by kernel
                glm::vec2 diff = particles[j].position - particle.position;
                if (glm::dot(diff, diff) < KERNEL_RADIUS_SQR) neighbors.push_back(j);
            }
        }
    }

    return neighbors;
}

void SpawnParticles()
{
    float radius = 160;
    glm::vec2 center = glm::vec2(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
    float spacing = KERNEL_RADIUS;

    for (float y = center.y - radius; y <= center.y + radius; y += spacing)
    {
        for (float x = center.x - radius; x <= center.x + radius; x += spacing)
        {
            glm::vec2 offset = glm::vec2(RandomValue() - 0.5f, RandomValue() - 0.5f);
            glm::vec2 position = glm::vec2(x, y);
            bool inside = distance(center, position) <= radius;
            if (inside && particles.size() < MAX_PARTICLES) particles.push_back(Particle(x + offset.x, y + offset.y));
        }
    }
}

void ResetParticles()
{
    particles.clear();
    particles.shrink_to_fit();
}

void ComputeDensityPressureMTBF()
{
    BuildGrid();

    tbb::parallel_for(0, (int)particles.size(), [](int i)
    {
        Particle& particle_a = particles[i];
        particle_a.density = 0.f;

        auto& neighbors = GetNearNeighborsMTBF(particle_a, i);
        for (int j : neighbors)
        {
            Particle& particle_b = particles[j];
            glm::vec2 rij = particle_b.position - particle_a.position;
            float r2 = glm::dot(rij, rij);
            particle_a.density += PARTICLE_MASS * POLY6 * pow(KERNEL_RADIUS_SQR - r2, 3.f);
        }

        particle_a.pressure = GAS_CONSTANT * (particle_a.density - REST_DENSITY);
    });
}

void ComputeForcesMTBF()
{
    tbb::parallel_for(0, (int)particles.size(), [](int i)
    {
        Particle& particle_a = particles[i];
        glm::vec2 pressure_force(0.f);
        glm::vec2 viscosity_force(0.f);

        auto& neighbors = neighbor_buffer[i];
        for (int j : neighbors)
        {
            if (i == j) continue;

            Particle& particle_b = particles[j];
            glm::vec2 diff = particle_b.position - particle_a.position;
            float dist = glm::length(diff);

            if (dist < 1e-6f)
            {
                diff = glm::vec2((RandomValue() - 0.5f) * 0.0001f, (RandomValue() - 0.5f) * 0.0001f);
                dist = glm::length(diff);
            }

            if (dist < KERNEL_RADIUS)
            {
                pressure_force += -normalize(diff) * PARTICLE_MASS * (particle_a.pressure + particle_b.pressure) / (2.f * particle_b.density) * SPIKY_GRAD * std::pow(KERNEL_RADIUS - dist, 3.f);
                viscosity_force += VISCOSITY * PARTICLE_MASS * (particle_b.velocity - particle_a.velocity) / particle_b.density * VISC_LAP * (KERNEL_RADIUS - dist);
            }
        }

        glm::vec2 mouse_pos = glm::vec2(input.GetMousePosition().x, WINDOW_HEIGHT - input.GetMousePosition().y);
        glm::vec2 mouse_dir = glm::normalize(mouse_pos - particle_a.position);
        float mouse_dist = glm::distance(mouse_pos, particle_a.position);
        bool mouse_pressing = input.IsMouseButtonHeldDown(0);
        glm::vec2 mouse_force = (mouse_pressing && mouse_dist < 320) ? mouse_dir * PARTICLE_MASS / particle_a.density * 20.f : glm::vec2(0.f);

        glm::vec2 gravity_force = glm::vec2(0.f, GRAVITY) * PARTICLE_MASS / particle_a.density;

        particle_a.force = pressure_force + viscosity_force + gravity_force + mouse_force;
    });
}

void IntegrateMTBF()
{
    tbb::parallel_for(0, (int)particles.size(), [](int i)
    {
        Particle& particle = particles[i];

        // forward Euler integration
        particle.velocity += INTIGRATION_TIMESTEP * particle.force / particle.density;
        particle.position += INTIGRATION_TIMESTEP * particle.velocity;

        // enforce boundary conditions
        if (particle.position.x - BOUNDARY_EPSILON < 0.f)
        {
            particle.velocity.x *= BOUND_DAMPING;
            particle.position.x = BOUNDARY_EPSILON;
        }
        if (particle.position.x + BOUNDARY_EPSILON > WINDOW_WIDTH)
        {
            particle.velocity.x *= BOUND_DAMPING;
            particle.position.x = WINDOW_WIDTH - BOUNDARY_EPSILON;
        }
        if (particle.position.y - BOUNDARY_EPSILON < 0.f)
        {
            particle.velocity.y *= BOUND_DAMPING;
            particle.position.y = BOUNDARY_EPSILON;
        }
        if (particle.position.y + BOUNDARY_EPSILON > WINDOW_HEIGHT)
        {
            particle.velocity.y *= BOUND_DAMPING;
            particle.position.y = WINDOW_HEIGHT - BOUNDARY_EPSILON;
        }
    });
}

void UpdateMTBF()
{
    ComputeDensityPressureMTBF();
    ComputeForcesMTBF();
    IntegrateMTBF();
}

void RenderMTBF()
{
    glClear(GL_COLOR_BUFFER_BIT);

    if (particles.empty()) return;

    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, false, value_ptr(projection));

    // pressure
    float pressureOffset = GAS_CONSTANT * -REST_DENSITY;
    glUniform1f(glGetUniformLocation(program, "minPressure"), pressureOffset + 0);
    glUniform1f(glGetUniformLocation(program, "maxPressure"), pressureOffset + 100);
    glUniform1f(glGetUniformLocation(program, "kernelSize"), KERNEL_RADIUS);

    // prepare buffers
    for (int i = 0; i < particles.size(); i++)
    {
        position_buffer[i * 2] = particles[i].position.x;
        position_buffer[i * 2 + 1] = particles[i].position.y;
        pressure_buffer[i] = particles[i].pressure;
    }

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, position_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, particles.size() * 2 * sizeof(float), position_buffer.data());

    glBindBuffer(GL_ARRAY_BUFFER, pressure_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, particles.size() * sizeof(float), pressure_buffer.data());

    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(particles.size()));

    glBindVertexArray(0);
}

GLuint CompileShader(std::string source, GLenum type)
{
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();

    glShaderSource(shader, 1, &src, nullptr);

    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string infoLog(length, ' ');
        glGetShaderInfoLog(shader, length, nullptr, &infoLog[0]);
        glDeleteShader(shader);
        std::string log = infoLog;
    }

    return shader;
}

GLuint CompileProgram(std::string vertCode, std::string fragCode)
{
    GLuint vertex = CompileShader(vertCode, GL_VERTEX_SHADER);
    GLuint fragment = CompileShader(fragCode, GL_FRAGMENT_SHADER);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string infoLog(length, ' ');
        glGetProgramInfoLog(program, length, nullptr, &infoLog[0]);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        std::string log = infoLog;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return program;
}

std::string VertShaderGLSL()
{
    std::string temp = 
    R"(
        #version 330 core

        layout(location = 0) in vec2 aPos;
        layout(location = 1) in float aPressure;

        out vec3 VertColor;

        uniform mat4 projection;
        uniform float minPressure;
        uniform float maxPressure;
        uniform float kernelSize;

        void main()
        {
            gl_Position = projection * vec4(aPos, 0.0, 1.0);
            gl_PointSize = kernelSize / 2.0;

            float clamped_pressure = clamp((aPressure - minPressure) / (maxPressure - minPressure), 0.0, 1.0);
            VertColor = mix(vec3(0.0, 0.4, 1.0), vec3(1.0, 1.0, 1.0), clamped_pressure);
        }
    )";
    return temp;
}

std::string FragShaderGLSL()
{
    std::string temp = 
    R"(
        #version 330 core

        in vec3 VertColor;
        out vec4 FragColor;

        void main()
        {
            // discard if outside radius
            vec2 coord = gl_PointCoord - vec2(0.5);
            if (length(coord) > 0.5) discard;
            
            FragColor = vec4(VertColor, 1.0);
        }
    )";
    return temp;
}

void SetupBuffers()
{
    // bind and gen vao
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // position buffer
    glGenBuffers(1, &position_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, position_vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    // pressure buffer
    glGenBuffers(1, &pressure_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, pressure_vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_PARTICLES * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0);

    // unbind vao
    glBindVertexArray(0);
}

int main()
{
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // create glfw window
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "opengl", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    // set glfw context
    glfwMakeContextCurrent(window);

    // init glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    // init opengl
    glClearColor(0, 0, 0, 1);
    glEnable(GL_PROGRAM_POINT_SIZE);
    program = CompileProgram(VertShaderGLSL(), FragShaderGLSL());
    projection = glm::ortho(0.0f, float(WINDOW_WIDTH), 0.0f, float(WINDOW_HEIGHT), -1.0f, 1.0f);
    SetupBuffers();

    // allocate particle buffers
    neighbor_buffer.resize(MAX_PARTICLES);
    position_buffer.resize(MAX_PARTICLES * 2);
    pressure_buffer.resize(MAX_PARTICLES);

    // init simulation
    SpawnParticles();

    // init input
    input.SetContext(window);

    // main loop
    while (!glfwWindowShouldClose(window))
    {
        input.Update();
        if (input.IsKeyDownThisFrame(GLFW_KEY_SPACE)) SpawnParticles();
        if (input.IsKeyDownThisFrame(GLFW_KEY_R)) ResetParticles();

        UpdateMTBF();
        RenderMTBF();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}