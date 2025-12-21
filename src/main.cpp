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

  auto g_mouse_callback = [](Event::MouseData d) { d.ctx->stop(); };
  auto button = UIButton::create(&ctx->mouse_event, "Quit",
                                 ctx->get_width() - 6, 0, g_mouse_callback);

  auto origin = Coords{0, 0};
  auto p2 = Coords{ctx->get_width(), ctx->get_height()};

  auto b = UILine::create(origin, p2);

  ctx->screen_event.add(Event::Type::Resize, [&](Event::ScreenData d) {
    auto p = button->composition[1];
    auto text = std::static_pointer_cast<UIText>(button->composition[1]);
    text->set_pos(ctx->get_width() - 6, 0);
    // p2 = Coords{ctx->get_width(), ctx->get_height()};
    // b->set_pos(origin, p2);
  });

  ctx->mouse_event.add(Event::Type::Mousemove, [&](Event::MouseData d) {
    b->set_pos(origin, Coords{d.x, d.y});
  });

  ctx->mouse_event.add(Event::Type::Click, [&](Event::MouseData d) {
    auto p = Coords{d.x, d.y};
    origin = p;
    b->set_pos(origin, p);
  });

  ctx->observer().sub(Event::Type::Mousemove, ctx->mouse_event);
  ctx->observer().sub(Event::Type::Click, ctx->mouse_event);
  ctx->observer().sub(Event::Type::Resize, ctx->screen_event);

  ctx->add_child(button);
  ctx->add_child(b);
  ctx->start();
  delete ctx;
  return 0;
}
