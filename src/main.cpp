#include <unistd.h>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include "include/hawktui.hpp"

int main() {
  UIContext* ctx = new UIContext();
  std::ostringstream oss;
  auto update_stats = [&]() -> std::string {
    oss.str("");
    oss.clear();
    oss << "screen_width: " << ctx->screen_width << "\n ";
    oss << "screen_height: " << ctx->screen_height << '\n';
    return oss.str();
  };

  auto textibox =
      std::make_shared<UITextiBox>(30, 5, 10, 0, true, update_stats(), 1, 1);
  textibox->flags |= TypeFlags::Draggable;

  // ctx->events.subscribe<EventManager::Event>(
  //     "resize",
  //     [&](EventManager::Event e) { textibox->text = update_stats(); });
  //
  // ctx->events.subscribe<UIContext::MouseEvent>(
  //     "mousemove", [&](UIContext::MouseEvent e) {
  //       oss.str("");
  //       oss.clear();
  //       oss << "screen_width: " << ctx->screen_width << "\n ";
  //       oss << "screen_height: " << ctx->screen_height << "\n ";
  //       oss << e.x << " " << e.y << '\n';
  //       textibox->text = oss.str();
  //     });

  auto box = std::make_shared<UITextiBox>(std::string("Click me!"));
  // box->text = std::move(str);
  // auto box = UIButton::create(&ctx->events, "Click me!",
  //                             [&](UIContext::MouseEvent e) {});
  // auto box = std::make_shared<UIButton>();
  ctx->add_child(textibox);
  ctx->add_child(box);
  ctx->start();
  delete ctx;
  return 0;
}
