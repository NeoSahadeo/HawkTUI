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

  // ctx->events.subscribe<EventManager::Event>(
  //     "resize", [&](EventManager::Event e) {
  //       text_label->label = update_stats().c_str();
  //     });

  // ctx->events.subscribe<UIContext::MouseEvent>(
  //     "mousemove", [&](UIContext::MouseEvent e) {
  //       oss.str("");
  //       oss.clear();
  //       oss << "screen_width: " << ctx->screen_width << "\n ";
  //       oss << "screen_height: " << ctx->screen_height << "\n ";
  //       oss << e.x << " " << e.y << '\n';
  //       textibox->text = oss.str();
  //     });

  // ctx->events.subscribe<UIContext::MouseEvent>(
  //     "click", [&](UIContext::MouseEvent e) {
  //       oss.str("");
  //       oss.clear();
  //       oss << "screen_width: " << ctx->screen_width << "\n";
  //       oss << "screen_height: " << ctx->screen_height << "\n";
  //       oss << e.x << " " << e.y << '\n';
  //       text_label->label = oss.str();
  //     });

  auto box = UIButton::create(&ctx->events, [&](HawkTuahed::MouseEvent e) {});

  ctx->add_child(box);
  ctx->add_child(text_label);
  ctx->tua();
  delete ctx;
  return 0;
}
