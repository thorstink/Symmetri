#include <stdio.h>

#include <chrono>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_metal.h"
#include "rpp/rpp.hpp"
#include "rxdispatch.h"
#include "rximgui.h"
using namespace rximgui;
#include "model.h"
#include "util.h"
#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
static void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char **) {
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

  // Setup style
  // ImGui::StyleColorsDark();
  ImGui::StyleColorsLight();

  // Setup window
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) return 1;

  // Create window with graphics context
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *window =
      glfwCreateWindow(1280, 720, "Dear ImGui GLFW+Metal example", nullptr, nullptr);
  if (window == nullptr) return 1;

  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  id<MTLCommandQueue> commandQueue = [device newCommandQueue];

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplMetal_Init(device);

  NSWindow *nswin = glfwGetCocoaWindow(window);
  CAMetalLayer *layer = [CAMetalLayer layer];
  layer.device = device;
  layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  nswin.contentView.layer = layer;
  nswin.contentView.wantsLayer = YES;

  MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor new];

  float clear_color[4] = {0.45f, 0.55f, 0.60f, 1.00f};

  // Flux starts here

  auto reducers = rpp::source::create<model::Reducer>(&rxdispatch::dequeue)
                      .subscribe_on(rpp::schedulers::new_thread{});

  auto models = reducers
                    .scan(model::initializeModel(),
                          [=](model::Model &&m, model::Reducer f) {
                            try {
                              auto r = f(m);
                              r.data->timestamp = std::chrono::steady_clock::now();
                              return r;
                            } catch (const std::exception &e) {
                              std::cerr << e.what() << std::endl;
                              return std::move(m);
                            }
                          })
                    .sample_with_time(std::chrono::milliseconds{200}, rl);

  auto view_models = models
                         .filter([=](const model::Model &m) {
                           return m.data->timestamp <= std::chrono::steady_clock::now();
                         })
                         .start_with(model::initializeModel())
                         .map([](const model::Model &m) { return model::ViewModel{m}; });

  auto draw_frames = frames.with_latest_from(rxu::take_at<1>(), view_models);

  auto subscription = draw_frames.tap([](const model::ViewModel &vm) { draw(vm); }).subscribe();

  // and loop

  // Main loop
  while (!glfwWindowShouldClose(window) && subscription.is_subscribed()) {
    @autoreleasepool {
      glfwPollEvents();

      int width, height;
      glfwGetFramebufferSize(window, &width, &height);
      layer.drawableSize = CGSizeMake(width, height);
      id<CAMetalDrawable> drawable = [layer nextDrawable];

      id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
      // colors
      renderPassDescriptor.colorAttachments[0].clearColor =
          MTLClearColorMake(clear_color[0] * clear_color[3], clear_color[1] * clear_color[3],
                            clear_color[2] * clear_color[3], clear_color[3]);

      renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
      renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
      renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
      id<MTLRenderCommandEncoder> renderEncoder =
          [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
      // [renderEncoder pushDebugGroup:@"ImGui demo"]; ????

      // Start the Dear ImGui frame
      ImGui_ImplMetal_NewFrame(renderPassDescriptor);
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      while (rl.is_any_ready_schedulable()) rl.dispatch();

      sendframe();

      while (rl.is_any_ready_schedulable()) rl.dispatch();

      // Rendering
      ImGui::Render();
      // ImGui::EndFrame();  // <-- Added, can be deleted?
      ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);

      // [renderEncoder popDebugGroup]; ????
      [renderEncoder endEncoding];

      [commandBuffer presentDrawable:drawable];
      [commandBuffer commit];
    }
  }

  subscription.unsubscribe();
  // Cleanup
  ImGui_ImplMetal_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
