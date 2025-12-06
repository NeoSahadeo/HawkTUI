#include <unistd.h>
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
  std::string stats = oss.str();
  auto textibox =
      std::make_shared<UITextiBox>(30, 5, 0, 0, false, update_stats(), 1, 1);
  ctx->events.subscribe("resize", [&]() { textibox->text = update_stats(); });

  // auto box = std::make_shared<UIBox>(10, 5, 0, 0, true);
  // textibox->flags |= TypeFlags::Draggable;
  ctx->add_child(textibox);
  // ctx->add_child(box);
  ctx->start();
  delete ctx;
  return 0;
}
