#include <unistd.h>
#include <memory>
#include <sstream>
#include <string>
#include "include/hawktui.hpp"
#define NDEBUG

int main() {
  HawkTuahed* ctx = new HawkTuahed();
  std::ostringstream oss;
  auto update_stats = [&]() -> std::string {
    oss.str("");
    oss.clear();
    oss << "screen_width: " << ctx->screen_width << "\n";
    oss << "screen_height: " << ctx->screen_height << '\n';
    return oss.str();
  };

  auto text_label = std::make_shared<UIText>(update_stats(), 20, 20, 20, 0);

  ctx->events.subscribe<EventManager::Event>(
      "resize", [&](EventManager::Event e) {
        text_label->label = update_stats().c_str();
      });

  ctx->events.subscribe<HawkTuahed::MouseEvent>(
      "mousemove", [&](HawkTuahed::MouseEvent e) {
        oss.str("");
        oss.clear();
        oss << "screen_width: " << ctx->screen_width << "\n";
        oss << "screen_height: " << ctx->screen_height << "\n";
        oss << e.x << " " << e.y << '\n';
        text_label->label = oss.str();
      });

  auto box =
      UIButton::create(&ctx->events, "Quit NEOW!", 30, 0,
                       [&](HawkTuahed::MouseEvent e) { ctx->running = false; });

  auto line = std::make_shared<UILine>(0, 40, 20, 0);
  // auto line = std::make_shared<UILine>(0, 0, 40, 20);
  // auto line = std::make_shared<UILine>(0, 3, 6, 0);
  ctx->add_child(box);
  ctx->add_child(line);
  // ctx->add_child(text_label);
  ctx->tua();
  delete ctx;
  return 0;
}
