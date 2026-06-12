#define IMGUI_DEFINE_MATH_OPERATORS

#include <stdio.h>

#include <chrono>
#include <iostream>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_metal.h"
#include "reactive.h"
#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "load_file.h"

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
static void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int argc, char **argv) {
  std::cout << "main " << std::this_thread::get_id() << std::endl;

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;

  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
  // io.Fonts->AddFontFromFileTTF(
  //     "/Users/thomashorstink/Projects/Symmetri/symmetri/gui/Manrope-VariableFont_wght.ttf",
  //     18.0f);  // Optionally load another font

  // Setup style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

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

  auto root_subscription = go();
  // Wake the (otherwise blocked) event loop whenever a reactive update is
  // queued, so we can idle at zero CPU instead of polling. Thread-safe in GLFW.
  rxdispatch::on_push = glfwPostEmptyEvent;
  if (argc > 1) loadPetriNet(argv[1]);

  // Power-saving event loop:
  //  - after any activity, render a few frames so interaction/animation stays
  //    smooth and any just-queued reactive work lands on the run loop;
  //  - once quiet, block in glfwWaitEventsTimeout until input or a posted wake
  //    arrives (the timeout is only a safety net for off-queue async updates,
  //    e.g. the reducer produced after an async file save completes);
  //  - while minimized, block fully; while occluded, keep the model current but
  //    skip the (expensive) GPU frame.
  constexpr double kIdleTimeout = 0.25;  // seconds
  int frames_to_render = 2;
  while (!glfwWindowShouldClose(window)) {
    if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
      glfwWaitEvents();  // minimized: fully idle until restored
      continue;
    }

    if (frames_to_render > 0) {
      glfwPollEvents();
    } else {
      glfwWaitEventsTimeout(kIdleTimeout);
    }

    // Covered by other windows: drain pending updates to keep the model current,
    // but don't spend GPU/CPU drawing what nobody can see.
    if (!(nswin.occlusionState & NSWindowOcclusionStateVisible)) {
      while (!rximgui::rl.is_empty()) rximgui::rl.dispatch();
      frames_to_render = 0;
      continue;
    }

    @autoreleasepool {
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);
      layer.drawableSize = CGSizeMake(width, height);
      id<CAMetalDrawable> drawable = [layer nextDrawable];

      id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
      renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
      renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
      renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
      id<MTLRenderCommandEncoder> renderEncoder =
          [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
      // [renderEncoder pushDebugGroup:@"ImGui demo"];

      // Start the Dear ImGui frame
      ImGui_ImplMetal_NewFrame(renderPassDescriptor);
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
      while (!rximgui::rl.is_empty()) {
        rximgui::rl.dispatch();
      };
      rximgui::sendframe();

      // Rendering
      ImGui::Render();
      ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);

      // [renderEncoder popDebugGroup]; ????
      [renderEncoder endEncoding];

      [commandBuffer presentDrawable:drawable];
      [commandBuffer commit];
    }

    // Keep rendering while the user is interacting, text is being edited, the
    // mouse is moving/scrolling, or reactive work is still pending; otherwise
    // wind down and block on the next iteration.
    ImGuiIO &io = ImGui::GetIO();
    const bool activity =
        !rximgui::rl.is_empty() || ImGui::IsAnyItemActive() || io.WantTextInput ||
        ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
        ImGui::IsMouseDown(ImGuiMouseButton_Middle) || io.MouseWheel != 0.0f ||
        io.MouseWheelH != 0.0f || io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f;
    frames_to_render = activity ? 3 : (frames_to_render > 0 ? frames_to_render - 1 : 0);
  }
  rxdispatch::on_push = nullptr;  // window is going away; stop posting wakes
  rxdispatch::unsubscribe();
  root_subscription.dispose();

  ImGui_ImplMetal_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
