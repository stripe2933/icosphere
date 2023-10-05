#include <OpenGLApp/Window.hpp>
#include <OpenGLApp/Program.hpp>
#include <OpenGLApp/Camera.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <imgui_variant_selector.hpp>
#include <dirty_property.hpp>
#include <visitor_helper.hpp>

#include "icosphere.hpp"
#include "vertex.hpp"

struct Shading{
    struct Flat {
        GLsizei num_icosphere_vertices = 0;
    };

    struct Phong {
        std::size_t num_icosphere_positions = 0;
        GLsizei num_icosphere_indices = 0;
    };

    using Type = std::variant<Flat, Phong>;
};

IMGUI_LABEL(Shading::Flat, "Flat shading");
IMGUI_LABEL(Shading::Phong, "Phong shading");

struct MvpMatrixUniform{
    glm::mat4 model;
    glm::mat4 inv_model;
    glm::mat4 projection_view;
};

struct LightingUniform{
    alignas(16) glm::vec3 view_pos;  // std140: base alignment must be 16 for vec3.
    alignas(16) glm::vec3 light_pos; // std140: base alignment must be 16 for vec3.
};

class Viewer final : public OpenGL::Window {
private:
    DirtyProperty<int> subdivision_level { 0 };
    DirtyProperty<Shading::Type> shading { Shading::Phong{} };
    DirtyProperty<bool> fix_light_position { false }; // true -> light is fixed at (5, 0, 0), false -> light is at camera position.
    float generation_elapsed = 0.f;

    std::optional<glm::vec2> previous_mouse_position;
    const struct{
        float scroll_sensitivity = 0.1f;
        float pan_sensitivity = 3e-3f;
    } input;
    OpenGL::PerspectiveCamera camera;

    OpenGL::Program flat_program, phong_program;
    DirtyProperty<MvpMatrixUniform> mvp_matrix;
    DirtyProperty<LightingUniform> lighting;

    GLuint vao;
    std::array<GLuint, 4> buffer_objects;
    GLuint &vbo            = buffer_objects[0],
           &ebo            = buffer_objects[1],
           &mvp_matrix_ubo = buffer_objects[2],
           &lighting_ubo   = buffer_objects[3];

    void onFramebufferSizeChanged(int width, int height) override {
        OpenGL::Window::onFramebufferSizeChanged(width, height);
        onCameraChanged();
    }

    void onScrollChanged(double xoffset, double yoffset) override {
        ImGuiIO &io = ImGui::GetIO();
        io.AddMouseWheelEvent(static_cast<float>(xoffset), static_cast<float>(yoffset));
        if (io.WantCaptureMouse) {
            return;
        }

        constexpr float min_distance = 1.2f;
        camera.view.distance = std::fmax(
            std::exp(input.scroll_sensitivity * static_cast<float>(-yoffset)) * camera.view.distance,
            min_distance
        );
        onCameraChanged();
    }

    void onMouseButtonChanged(int button, int action, int mods) override {
        ImGuiIO &io = ImGui::GetIO();
        io.AddMouseButtonEvent(button, action);
        if (io.WantCaptureMouse) {
            return;
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS){
            double mouse_x, mouse_y;
            glfwGetCursorPos(window, &mouse_x, &mouse_y);

            previous_mouse_position = glm::vec2(mouse_x, mouse_y);
        }
        else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE){
            previous_mouse_position = std::nullopt;
        }
        onCameraChanged();
    }

    void onCursorPosChanged(double xpos, double ypos) override {
        const glm::vec2 position { xpos, ypos };

        ImGuiIO &io = ImGui::GetIO();
        io.AddMousePosEvent(position.x, position.y);
        if (io.WantCaptureMouse) {
            return;
        }

        if (previous_mouse_position.has_value()) {
            const glm::vec2 offset = input.pan_sensitivity * (position - previous_mouse_position.value());
            previous_mouse_position = position;

            camera.view.addYaw(offset.x);
            camera.view.addPitch(-offset.y);

            onCameraChanged();
        }
    }

    void update(float time_delta) override {
        // If either subdivision_level or shading is changed, the vertices should be recalculated.
        DirtyPropertyHelper::clean([&](std::uint8_t new_subdivision_level, Shading::Type &new_shading){
            std::visit(overload {
                [&](Shading::Flat &flat_shading){
                    const auto start = std::chrono::high_resolution_clock::now();

                    const auto triangles = Icosphere<unsigned int>::generate(new_subdivision_level) /* new icosphere */
                            .getTriangles();

                    std::vector<Vertex> vertices;
                    vertices.reserve(3 * triangles.size());

                    for (const Triangle &triangle : triangles){
                        const glm::vec3 normal = triangle.normal();
                        vertices.emplace_back(triangle.p1, normal);
                        vertices.emplace_back(triangle.p2, normal);
                        vertices.emplace_back(triangle.p3, normal);
                    }

                    const auto end = std::chrono::high_resolution_clock::now();
                    generation_elapsed = std::chrono::duration<float, std::milli>(end - start).count();

                    flat_shading.num_icosphere_vertices = static_cast<GLsizei>(vertices.size());

                    glBindVertexArray(vao);

                    glBindBuffer(GL_ARRAY_BUFFER, vbo);
                    glBufferData(GL_ARRAY_BUFFER,
                                 static_cast<GLsizei>(vertices.size() * sizeof(Vertex)),
                                 vertices.data(),
                                 GL_STATIC_DRAW);

                    glVertexAttribPointer(0,
                                          3,
                                          GL_FLOAT,
                                          GL_FALSE,
                                          sizeof(Vertex),
                                          reinterpret_cast<GLint*>(offsetof(Vertex, position)));
                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(1,
                                          3,
                                          GL_FLOAT,
                                          GL_FALSE,
                                          sizeof(Vertex),
                                          reinterpret_cast<GLint*>(offsetof(Vertex, normal)));
                    glEnableVertexAttribArray(1);
                },
                [&](Shading::Phong &phong_shading){
                    const auto start = std::chrono::high_resolution_clock::now();
                    const Mesh<unsigned int> new_icosphere = Icosphere<unsigned int>::generate(new_subdivision_level);
                    const auto end = std::chrono::high_resolution_clock::now();
                    generation_elapsed = std::chrono::duration<float, std::milli>(end - start).count();

                    phong_shading.num_icosphere_positions = new_icosphere.positions.size();
                    phong_shading.num_icosphere_indices = 3 * static_cast<GLsizei>(new_icosphere.triangle_indices.size());

                    glBindVertexArray(vao);

                    glBindBuffer(GL_ARRAY_BUFFER, vbo);
                    glBufferData(GL_ARRAY_BUFFER,
                                 static_cast<GLsizei>(new_icosphere.positions.size() * sizeof(glm::vec3)),
                                 new_icosphere.positions.data(),
                                 GL_STATIC_DRAW);

                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
                    glEnableVertexAttribArray(1); // for sphere, all vertex position is also normal.

                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                                 static_cast<GLsizei>(new_icosphere.triangle_indices.size() * sizeof(Mesh<unsigned int>::triangle_index_t)),
                                 new_icosphere.triangle_indices.data(), GL_STATIC_DRAW);
                }
            }, new_shading);
        }, subdivision_level, shading);

        // MVP Matrix UBO should be updated when it changed.
        mvp_matrix.clean([&](const MvpMatrixUniform &value){
            glBindBuffer(GL_UNIFORM_BUFFER, mvp_matrix_ubo);
            glBufferData(GL_UNIFORM_BUFFER, sizeof(MvpMatrixUniform), &value, GL_DYNAMIC_DRAW);
        });

        // If fix_light_position is updated, lighting should be updated.
        fix_light_position.clean([&](bool value){
            lighting.mutableValue().light_pos = getLightPosition();
            lighting.makeDirty();
        });

        // Lighting UBO should be updated when it changed.
        lighting.clean([&](const LightingUniform &value){
            glBindBuffer(GL_UNIFORM_BUFFER, lighting_ubo);
            glBufferData(GL_UNIFORM_BUFFER, sizeof(LightingUniform), &value, GL_DYNAMIC_DRAW);
        });

        updateImGui(time_delta);
    }

    void draw() const override {
        glClear(GL_COLOR_BUFFER_BIT);

        std::visit(overload{
            [&](const Shading::Flat &flat_shading){
                flat_program.use();

                glBindVertexArray(vao);
                glDrawArrays(GL_TRIANGLES, 0, flat_shading.num_icosphere_vertices);
            },
            [&](const Shading::Phong &phong_shading){
                phong_program.use();

                glBindVertexArray(vao);
                glDrawElements(GL_TRIANGLES, phong_shading.num_icosphere_indices, GL_UNSIGNED_INT, nullptr);
            }
        }, shading.value());

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    glm::vec3 getLightPosition() const{
        return fix_light_position.value() ? glm::vec3 { 5.f, 0.f, 0.f } : camera.view.getPosition();
    }

    void onCameraChanged(){
        mvp_matrix.mutableValue().projection_view = camera.projection.getMatrix(getFramebufferAspectRatio()) * camera.view.getMatrix();
        mvp_matrix.makeDirty();

        lighting = LightingUniform { .view_pos = camera.view.getPosition(), .light_pos = getLightPosition() };
    }

    void initImGui() {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void) io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }

    void updateImGui(float) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Viewer");

        ImGui::Text("FPS: %d", static_cast<int>(ImGui::GetIO().Framerate));

        if (bool input = fix_light_position.value();
            ImGui::Checkbox("Fix light position", &input))
        {
            fix_light_position = input;
        }

        if (int subdivision_level_input = subdivision_level.value();
            ImGui::InputInt("Subdivision level", &subdivision_level_input, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            subdivision_level = std::clamp(subdivision_level_input, 0, 8);
        }

        ImGui::VariantSelector::radio(
            shading.mutableValue(),
            std::pair {
                [&](const Shading::Flat &flat_shading){
                    ImGui::Text("# of vertices: %d", flat_shading.num_icosphere_vertices);
                },
                [&]{
                    // This lambda executed when user select shading mode as flat shading, and VariantSelector assign
                    // the Shading::Flat{} to mutable reference of shading property's stored value. Therefore, it should
                    // be explicitly make the property dirty to indicate the property value is modified.
                    shading.makeDirty();
                    return Shading::Flat{};
                }
            },
            std::pair {
                [&](const Shading::Phong &phong_shading){
                    ImGui::Text("# of positions: %zu", phong_shading.num_icosphere_positions);
                    ImGui::Text("# of indices: %d", phong_shading.num_icosphere_indices);
                },
                [&]{
                    // This lambda executed when user select shading mode as phong shading, and VariantSelector assign
                    // the Shading::Phong{} to mutable reference of shading property's stored value. Therefore, it should
                    // be explicitly make the property dirty to indicate the property value is modified.
                    shading.makeDirty();
                    return Shading::Phong{};
                }
            }
        );
        ImGui::Text("Generation time: %.3f ms", generation_elapsed);

        ImGui::End();

        ImGui::Render();
    }

public:
    Viewer() : OpenGL::Window { 800, 480, "Viewer" },
               flat_program { "shaders/flat.vert", "shaders/flat.frag" },
               phong_program { "shaders/phong.vert", "shaders/phong.frag" }
    {
        camera.view.distance = 5.f;
        camera.view.addYaw(glm::radians(180.f));

        const auto model = glm::identity<glm::mat4>(); // You can use your own transform matrix here.
        mvp_matrix.mutableValue() = MvpMatrixUniform {
            .model = model,
            .inv_model = glm::inverse(model),
            .projection_view = camera.projection.getMatrix(getAspectRatio()) * camera.view.getMatrix(),
        };
        mvp_matrix.makeDirty();

        lighting = LightingUniform { .view_pos = camera.view.getPosition(), .light_pos = getLightPosition() };

        // VAO for icosphere.
        glGenVertexArrays(1, &vao);
        glGenBuffers(static_cast<GLsizei>(buffer_objects.size()), buffer_objects.data());

        // Set uniform buffer objects.
        // MVP matrix UBO.
        {
            glBindBuffer(GL_UNIFORM_BUFFER, mvp_matrix_ubo);
            glBufferData(GL_UNIFORM_BUFFER, sizeof(MvpMatrixUniform), nullptr, GL_DYNAMIC_DRAW);

            OpenGL::Program::setUniformBlockBindings("MvpMatrix", 0, flat_program, phong_program);
            glBindBufferBase(GL_UNIFORM_BUFFER, 0, mvp_matrix_ubo);
        }

        // Lighting UBO.
        {
            glBindBuffer(GL_UNIFORM_BUFFER, lighting_ubo);
            glBufferData(GL_UNIFORM_BUFFER, sizeof(LightingUniform), nullptr, GL_DYNAMIC_DRAW);

            OpenGL::Program::setUniformBlockBindings("Lighting", 1, flat_program, phong_program);
            glBindBufferBase(GL_UNIFORM_BUFFER, 1, lighting_ubo);
        }

        // Front face of triangles are counter-clockwise.
        glEnable(GL_CULL_FACE);

        initImGui();
    }

    ~Viewer() noexcept override{
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(static_cast<GLsizei>(buffer_objects.size()), buffer_objects.data());

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};

int main() {
    Viewer{}.run();
}