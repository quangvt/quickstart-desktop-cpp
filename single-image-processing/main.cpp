#include "BanubaClientToken.hpp"
#include <GlfwRenderer.hpp>
#include <Utils.hpp>

#include <bnb/effect_player/utility.hpp>
#include <bnb/player_api/interfaces/player/player.hpp>
#include <bnb/player_api/interfaces/input/live_input.hpp>
#include <bnb/player_api/interfaces/input/photo_input.hpp>
#include <bnb/player_api/interfaces/output/opengl_frame_output.hpp>
#include <bnb/player_api/interfaces/render_target/opengl_render_target.hpp>
#include <bnb/player_api/interfaces/render_target/metal_render_target.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <filesystem>

using namespace bnb::interfaces;

namespace
{
    render_backend_type render_backend = render_backend_type::opengl;
}

int main(int argc, char* argv[])
{
    if (argc != 5) {
        std::printf("Usage: <api_key> <effect> <input_file> <output_file>");
        return 1;
    }

    int argIndex = 1;
    std::string apiKey = argv[argIndex++];
    std::string effectName = argv[argIndex++];
    std::string inputFilePath = argv[argIndex++];
    std::string outputFilePath = argv[argIndex++];

    std::string resourcesDir = (std::filesystem::current_path() / "resources").string();

    // Initialize BanubaSDK with token and paths to resources
    bnb::utility utility({ bnb::sdk_resources_path(), resourcesDir }, apiKey);
    // Create render delegate based on GLFW
    auto renderer = std::make_shared<GLFWRenderer>(render_backend);
    // Create render target
    auto render_target = bnb::player_api::opengl_render_target::create();
    // Create player
    auto player = bnb::player_api::player::create(30, render_target, renderer);
    // Create photo input
    auto input = bnb::player_api::photo_input::create();
    // Create frame output with callback t
    // that is called when frames are received and saves it to a file.
    auto frame_output = bnb::player_api::opengl_frame_output::create([player, outputFilePath](const bnb::full_image_t& pb) {
        stbi_write_png(
            (outputFilePath).c_str(),
            pb.get_width(),
            pb.get_height(),
            pb.get_bytes_per_pixel(),
            pb.get_base_ptr(),
            pb.get_bytes_per_row()
        );
        std::printf("Processing result was written to `%s`. \n", outputFilePath.c_str());
        }, bnb::pixel_buffer_format::bpc8_rgba);
    // Sync effect loading
    player->load(effectName);
    player->use(input).use(frame_output);
    // Switch to manual render mode
    player->set_render_mode(bnb::player_api::player::render_mode::manual);
    // Load image into input
    input->load(inputFilePath);
    // Render once
    player->render();
}
