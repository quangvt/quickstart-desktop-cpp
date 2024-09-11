// BanubaWrapper.cpp
#include "BanubaWrapper.h"
#include "BanubaClientToken.hpp"
#include <GlfwRenderer.hpp>
#include <Utils.hpp>
#include <bnb/player_api/interfaces/player.hpp>
#include <bnb/effect_player/utility.hpp>
#include <bnb/player_api/interfaces/render_target/opengl_render_target.hpp>
#include <bnb/player_api/interfaces/render_target/metal_render_target.hpp>
#include <bnb/player_api/interfaces/player/player.hpp>
#include <bnb/player_api/interfaces/input/live_input.hpp>
#include <bnb/spal/camera/base.hpp>
#include <bnb/player_api/interfaces/output/window_output.hpp>
#include <bnb/recognizer/interfaces/utility_manager.hpp>
#include <bnb/player_api/interfaces/output/opengl_frame_output.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <chrono>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <utility>

#define DEBUG

using namespace bnb::interfaces;

// Function to convert pixel buffer format to string
const char* pixelBufferFormatToString(bnb::pixel_buffer_format format) {
    switch (format) {
        case bnb::pixel_buffer_format::bpc8_rgb: return "bpc8_rgb";
        case bnb::pixel_buffer_format::bpc8_bgr: return "bpc8_bgr";
        case bnb::pixel_buffer_format::bpc8_rgba: return "bpc8_rgba";
        case bnb::pixel_buffer_format::bpc8_bgra: return "bpc8_bgra";
        case bnb::pixel_buffer_format::bpc8_argb: return "bpc8_argb";
        case bnb::pixel_buffer_format::nv12: return "nv12";
        case bnb::pixel_buffer_format::i420: return "i420";
        default: return "unknown_format";
    }
}

void printPixelBufferFormat(const bnb::full_image_t& pb) {
    std::printf("Result full_image_t format = %s \n", pixelBufferFormatToString(pb.get_pixel_buffer_format()));
}

namespace {
    render_backend_type render_backend = BNB_APPLE ? render_backend_type::metal : render_backend_type::opengl;
    std::shared_ptr<GLFWRenderer> renderer;
    bnb::player_api::player_sptr player;
    std::shared_ptr<bnb::player_api::live_input> input;
    std::shared_ptr<bnb::player_api::window_output> window_output;
    std::shared_ptr<bnb::utility> utility;
    bnb::camera_sptr bnb_camera;
    std::shared_ptr<bnb::player_api::opengl_frame_output> frame_output;
    // Global variable to store the callback
    ImageCallbackType g_image_callback = nullptr;
}

void initializeBanuba(const char* sdkPath, const char* resourcesFolder, const char* clientToken) {
    // Initialize BanubaSDK with token and paths to resources
    //utility = std::make_shared< bnb::utility >({ bnb::sdk_resources_path(), resourcesFolder }, clientToken);
    bnb::interfaces::utility_manager::initialize(std::initializer_list<std::string>{bnb::sdk_resources_path(), resourcesFolder}, clientToken);
    // Create render delegate based on GLFW
    renderer = std::make_shared<GLFWRenderer>(render_backend);
    // Create render target
    bnb::player_api::render_target_sptr render_target;
    render_target = bnb::player_api::opengl_render_target::create();
    // Create player
    player = bnb::player_api::player::create(30, render_target, renderer);
    // Create live input, for realtime camera
    input = bnb::player_api::live_input::create();
}

void loadEffect(const char* effectName) 
{
  if (player) {
    player->load_async(effectName);
  }
  // On-screen output
  /*window_output = bnb::player_api::window_output::create(renderer->get_native_surface());
  player->use(input).use(window_output);
    if (player) {
        player->load_async(effectName);
    }*/
}

void startRenderingGLFW() {
  window_output = bnb::player_api::window_output::create(renderer->get_native_surface());
  player->use(input).use(window_output);
    if (renderer) {
      // Setup callbacks for glfw window
      renderer->get_window()->set_callbacks([](uint32_t w, uint32_t h) {
        window_output->set_frame_layout(0, 0, w, h);
        }, nullptr);
        renderer->get_window()->show_window_and_run_events_loop();
    }
}

// Function to register the callback
void RegisterImageCallback(ImageCallbackType callback) {
    g_image_callback = callback;
}

void startRenderingBuffer() {

    // Create frame output with callback
    frame_output = bnb::player_api::opengl_frame_output::create(
    [](const bnb::full_image_t& pb)
    {
        // Get image dimensions and bytes per pixel
        int width = pb.get_width();
        int height = pb.get_height();
        int bytes_per_pixel = pb.get_bytes_per_pixel(); // Should be 4 for RGBA
        int bytes_per_row = pb.get_bytes_per_row();
        printPixelBufferFormat(pb);

        // Ensure we're working with a 4-channel image (RGBA)
        if (bytes_per_pixel == 4)
        {
            // Allocate a new buffer to store the converted RGB image
            auto output_image = new unsigned char[width * height * bytes_per_pixel];

            // Copy and convert the image data from BGR to RGB
            const unsigned char* input_ptr = pb.get_base_ptr();
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    int index = y * bytes_per_row + x * bytes_per_pixel;

                    // ? << ?
                    // Swap Red and Blue channels
                    output_image[index]     = input_ptr[index];         // R
                    output_image[index + 1] = input_ptr[index + 1];     // G
                    output_image[index + 2] = input_ptr[index + 2];     // B
                    output_image[index + 3] = input_ptr[index + 3];     // A
                }
            }

#if defined(DEBUG)
            // Generate a timestamp for the file name
            auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()).count();

            // Create a unique file name
            std::stringstream ss;
            ss << "image_" << now_us << "_bnb_result";
            const auto result_path = (std::filesystem::current_path() / ss.str()).string();

            // Save the converted image as PNG and JPG
            stbi_write_png(
                (result_path + ".png").c_str(),
                width,
                height,
                bytes_per_pixel,
                output_image,
                bytes_per_row
            );

            stbi_write_jpg(
                (result_path + ".jpg").c_str(),
                width,
                height,
                bytes_per_pixel,
                output_image,
                100
            );
#endif

            // Call the callback with the converted image
            if (g_image_callback) {
                g_image_callback(output_image, width, height);
            }
        }
        else {
            std::printf("Unsupported bytes_per_pixel: %d. Only 4 (RGBA) is supported.\n", bytes_per_pixel);
        }
    }, 
    bnb::pixel_buffer_format::bpc8_rgba
    );

  player->use(input).use(frame_output);
}

void stopRendering() {
    if (player) {
        //renderer->get_window()->close_window();
      player->pause();
    }
}
void playRendering() {
  if (player) {
    //renderer->get_window()->close_window();
    player->play();
  }
}

void attachCamera() {
  bnb_camera = bnb::create_camera_device([](bnb::full_image_t image) {
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
    input->push(image, now_us);
    }, 0);
}

void saveImageToFile(const bnb::full_image_t& image) {
    // Get image properties
    int width = image.get_width();
    int height = image.get_height();
    int bytes_per_pixel = image.get_bytes_per_pixel(); // Should be 3 for RGB
    const uint8_t* imageData = image.get_base_ptr();   // Get the raw image data pointer

    // Calculate stride (width * bytes per pixel)
    int stride_in_bytes = width * bytes_per_pixel;

    // Generate a timestamp for the file name
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    // Create a unique file name
    std::stringstream ss;
    ss << "image_" << now_us << "_full_image_t.jpg";
    std::string filename = ss.str();

    stbi_write_jpg(filename.c_str(), width, height, bytes_per_pixel, imageData, 100);
}

void pushImageFromByteArray(const unsigned char* imageData, int stride, int width, int height) {

    // Create a modifiable copy of the image data
    unsigned char* modifiableData = new unsigned char[width * height * 3];
    std::memcpy(modifiableData, imageData, width * height * 3);

    // Swap Red and Blue channels
    for (int i = 0; i < width * height * 3; i += 3)
    {
        std::swap(modifiableData[i], modifiableData[i + 2]); // Swap Red and Blue
    }

    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();


#if defined(DEBUG)
    // Create the filename with the timestamp
    std::stringstream ss;
    ss << "image_" << now_us << "_origin.jpg";

    // Save the image as a JPG file
    // Assuming imageData is in RGB format
    stbi_write_jpg(ss.str().c_str(), width, height, 3, modifiableData, 100);
#endif

    //// Construct bnb::full_image_t from the byte array
    auto image = bnb::full_image_t::create_bpc8(
      modifiableData,
      stride,
      width,
      height,
      bnb::pixel_buffer_format::bpc8_rgb,
      bnb::camera_orientation::deg_0,
      false,
      [](uint8_t* data) { /* deleting the image data */ }
    );

#if defined(DEBUG)
    saveImageToFile(image);
#endif

    // Push the image to the input
    input->push(image, now_us);
}

void releaseBanuba() {
    // Clean up resources
    if (player) {
        //player->stop();  // Ensure the player is stopped
        player.reset();
    }

    if (renderer) {
        //renderer->get_window()->close_window();  // Close the window if not already done
        renderer.reset();
    }

    input.reset();  // Clear the input
}

void ReleaseImage(const unsigned char* image_data)
{
  delete[] image_data;
}

