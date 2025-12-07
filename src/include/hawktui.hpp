#include <ncurses.h>
#include <any>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>
#ifndef HAWKTUI_H
#define HAWKTUI_H

enum class TypeId { Ctx, Box, Button, Label };

enum class TypeFlags : uint8_t {
  None = 0,
  Draggable = 1,
};

// template<typename Enum, typename = std::enable_if_t<std::is_enum_v<Enum>>>
// constexpr Enum bitwise_op(Enum x, Enum y, auto op) noexcept {
//   using underlying = std::underlying_type_t<Enum>;
//   return static_cast<Enum>(op(static_cast<underlying>(x),
//   static_cast<underlying>(y)));
// }

constexpr TypeFlags operator|=(TypeFlags& x, TypeFlags y) noexcept {
  x = static_cast<TypeFlags>(static_cast<uint8_t>(x) | static_cast<uint8_t>(y));
  return x;
}

constexpr bool operator&(TypeFlags x, TypeFlags y) noexcept {
  return static_cast<bool>(static_cast<uint8_t>(x) & static_cast<uint8_t>(y));
}

template <typename Derived, typename Base>
std::unique_ptr<Derived> static_unique_ptr_cast(
    std::unique_ptr<Base>&& ptr) noexcept {
  return std::unique_ptr<Derived>(static_cast<Derived*>(ptr.release()));
}

// template <typename... Args>
// static std::unordered_map<std::string,
//                           std::vector<std::function<void(Args&&...)>>>
//     EventHandlersMap;

class EventManager {
 public:
  struct Event {};

  std::unordered_map<std::string, std::vector<std::function<void(Event&)>>>
      handlers;

  /**
   * Subscribe a callback to an event
   * */
  template <typename EventT, typename F>
  void subscribe(const std::string& event, F&& callback) {
    handlers[event].emplace_back(
        [cb = std::forward<F>(callback)](const auto& e) mutable {
          cb(static_cast<const EventT&>(e));
        });
  }

  /**
   * Dispatch an event with a lvalue
   * */
  void dispatch(const std::string& event, Event& data) {
    if (auto callbacks = handlers.find(event); callbacks != handlers.end()) {
      for (auto& f : callbacks->second) {
        f(data);
      }
    }
  }

  /**
   * Dispatch an event with an rvalue
   * */
  void dispatch(const std::string& event, Event&& data = Event{}) {
    if (auto callbacks = handlers.find(event); callbacks != handlers.end()) {
      for (auto& f : callbacks->second) {
        f(data);
      }
    }
  }
};

class UIElement {
 public:
  std::vector<std::shared_ptr<UIElement>> children;
  TypeFlags flags = TypeFlags::None;
  WINDOW* window = nullptr;

  virtual TypeId type() const = 0;
  virtual ~UIElement() = default;
  virtual void render() = 0;

  /**
   * Add a UIElement to the children of the current element
   * */
  void add_child(std::shared_ptr<UIElement> child) {
    children.emplace_back(child);
  }
};

/**
 * Creates a UIBox element - empty box with a border
 * */
class UIBox : public UIElement {
 public:
  size_t width;
  size_t height;
  int x;
  int y;

  UIBox(size_t w = 10,
        size_t h = 5,
        int x = 0,
        int y = 0,
        bool draggable = false)
      : width(w), height(h), x(x), y(y) {
    window = newwin(height, width, this->y, this->x);
    flags = TypeFlags::None;
    if (draggable) {
      flags |= TypeFlags::Draggable;
    }
  }

  ~UIBox() { delwin(window); }

  void render() override {
    box(window, 0, 0);
    touchwin(window);
    wnoutrefresh(window);
  }

  TypeId type() const override { return TypeId::Box; };
};

/*
 * Creates a UITextiBox, it is a box with text rendered on top of it
 * */
class UITextiBox : public UIBox {
 public:
  std::string text;
  int t_x;
  int t_y;

  UITextiBox() = default;
  UITextiBox(std::string text, int x = 0, int y = 0)
      : UIBox(), text(text), t_x(x), t_y(y) {}

  UITextiBox(size_t b_w,
             size_t b_h,
             int b_x,
             int b_y,
             bool d,
             std::string text,
             int x = 0,
             int y = 0)
      : UIBox(b_w, b_h, b_x, b_y, d), text(text), t_x(x), t_y(y) {}

  void render() override {
    mvwprintw(window, t_y, t_x, "%s", text.c_str());
    touchwin(window);
    wnoutrefresh(window);
    UIBox::render();
  }
};

/**
 * UI Context class that sets up the main window and pointer events
 * */
class UIContext : public UIElement {
 public:
  WINDOW* window;
  mmask_t oldmask;
  int screen_width;
  int screen_height;
  EventManager events;

  struct MouseEvent : EventManager::Event {
    int x;
    int y;
  } m_e;

  UIContext() {
    window = initscr();
    getmaxyx(window, screen_height, screen_width);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mmask_t mask, oldmask;
    mask = ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION;
    curs_set(0);
    mouseinterval(0);
    mousemask(mask, &oldmask);
    oldmask = oldmask;
    printf("\033[?1003h\n");
  }

  ~UIContext() {
    printf("\033[?1003l\n");
    curs_set(1);
    mousemask(oldmask, NULL);
    delwin(window);
    endwin();

    children.clear();
  }

  void start() {
    render();
    int c;
    MEVENT event;
    struct Coords {
      int x, y;
    };
    Coords click_offset{0};
    std::shared_ptr<UIElement> current_element;

    while ((c = wgetch(window)) != 'q') {
      touchwin(stdscr);
      wnoutrefresh(window);
      if (c == KEY_RESIZE) {
        getmaxyx(window, screen_height, screen_width);
        events.dispatch("resize");
      }

      if (c == KEY_MOUSE) {
        while (getmouse(&event) == OK) {
          if (current_element && current_element->type() == TypeId::Box) {
            auto box = std::dynamic_pointer_cast<UIBox>(current_element);
            mvwin(box->window, event.y - click_offset.y,
                  event.x - click_offset.x);
          }

          if (event.bstate & BUTTON1_PRESSED) {
            events.dispatch("mousedown");
            for (auto& child : children) {
              if (child->flags & TypeFlags::Draggable &&
                  child->type() == TypeId::Box) {
                auto box = std::dynamic_pointer_cast<UIBox>(child);
                int start_y, start_x;
                getbegyx(child->window, start_y, start_x);
                if (event.x >= start_x && event.x <= start_x + box->width &&
                    event.y >= start_y && event.y <= start_y + box->height) {
                  current_element = child;
                  click_offset.x = event.x - start_x;
                  click_offset.y = event.y - start_y;
                }
              }
            }
            m_e.x = event.x;
            m_e.y = event.y;
            events.dispatch("mousemove", m_e);
          } else if (event.bstate & BUTTON1_RELEASED) {
            events.dispatch("mouseup");
            current_element.reset();
          }
        }
      }
      render();
    }
  }

  void render() override {
    wnoutrefresh(window);
    for (auto& child : children)
      child->render();
    doupdate();
  }

  TypeId type() const override { return TypeId::Ctx; };
};

#endif
