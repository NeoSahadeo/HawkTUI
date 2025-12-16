#include <unistd.h>
#include <memory>
#include <sstream>
#include <string>
#include "include/hawktui.hpp"
#define NDEBUG

int main() {
  UIContext* ctx = new UIContext();
  std::ostringstream oss;
  auto update_stats = [&]() -> std::string {
    oss.str("");
    oss.clear();
    oss << "screen_width: " << ctx->get_width() << "\n";
    oss << "screen_height: " << ctx->get_height() << '\n';
    return oss.str();
  };

  // auto line = std::make_shared<UILine>(0, 0, 20, 20);
  // auto text_label = std::make_shared<UIText>(update_stats(), 20, 20, 20, 0);

  // ctx->events.subscribe<HawkTuahed::MouseEvent>(
  //     "mousemove", [&](HawkTuahed::MouseEvent e) {
  //       oss.str("");
  //       oss.clear();
  //       oss << "screen_width: " << ctx->screen_width << "\n";
  //       oss << "screen_height: " << ctx->screen_height << "\n";
  //       oss << e.x << " " << e.y << '\n';
  //       text_label->label = oss.str();
  //       line->p2.x = e.x;
  //       line->p2.y = e.y;
  //       line->x_delta = line->p1.x - e.x;
  //       line->y_delta = line->p1.y - e.y;
  //       if (line->p2.x - line->p1.x != 0) {
  //         line->gradient = line->p2.y - line->p1.y / line->p2.x - line->p1.x;
  //       }
  //     });

  // auto box = UIButton::create(
  //     &CtxData::events, "Quit NEOW!", CtxData::screen_width - 12, 0,
  //     [&](UIContext::MouseEvent e) { CtxData::running = false; });

  // ctx->events.subscribe<EventManager::Event>(
  //     "resize", [&](EventManager::Event e) {
  //       text_label->label = update_stats().c_str();
  //       auto text = std::static_pointer_cast<UIText>(box->composition[1]);
  //       text->win_x = ctx->screen_width - 12;
  //       text->adjust();
  //     });

  // auto line = std::make_shared<UILine>(-3, 4, -5, 0);
  // auto line = std::make_shared<UILine>(0, 0, 10, -10);
  // ctx->add_child(box);
  // ctx->add_child(line);
  // ctx->add_child(text_label);
  ctx->start();
  delete ctx;
  return 0;
}
