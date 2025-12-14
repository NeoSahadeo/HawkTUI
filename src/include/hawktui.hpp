#include <ncurses.h>
#include <unistd.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "log.hpp"
#ifndef HAWKTUI_H
#define HAWKTUI_H

#define M_PI_2x 6.283185307
#define M_PI_3rd 4.71238898

enum class TypeId { None, Box, Text, Button, Line, Curve, Node };

enum class TypeFlags : uint8_t {
  None,
  Draggable,
  Editable,
};

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

class _AbstractUIElement {
 public:
  ~_AbstractUIElement() = default;
  _AbstractUIElement() { delwin(window); };

  std::vector<std::shared_ptr<_AbstractUIElement>> composition{};
  TypeFlags flags{TypeFlags::None};
  WINDOW* window{};

  virtual void render() = 0;
  virtual TypeId type() = 0;
};

template <TypeId T>
class IUIElement : public _AbstractUIElement {
 public:
  IUIElement() = default;
  IUIElement(WINDOW* window) { this->window = window; }
  TypeId type() { return T; }
};

/**
 * Draws a line from point a to point b
 * */
class UILine : public IUIElement<TypeId::Line> {
  typedef struct Coords {
    int x{};
    int y{};
  } Coords;

  int eq_line(int x) { return gradient * (x - p1.x) + p1.y; }

 public:
  int x_delta;
  int y_delta;
  int width;
  int height;
  double gradient;
  Coords p1, p2;
  UILine(int x1, int y1, int x2, int y2) {
    p1 = {.x = x1, .y = y1};
    p2 = {.x = x2, .y = y2};
    x_delta = x2 - x1;
    y_delta = y2 - y1;
    if (x2 - x1 != 0) {
      gradient = (double)(y2 - y1) / (double)(x2 - x1);
    } else {
      gradient = 0;
    }
    width = std::abs(x_delta) + 1;
    height = std::abs(y_delta) + 1;
    window = newwin(width, height, 0, 0);
    // wbkgd(window, COLOR_PAIR(1));
  }

  void render() override {
    if (y_delta == 0) {
      mvwhline(window, p1.y, p1.x, '-', width);
      wnoutrefresh(window);
      return;
    }
    if (x_delta == 0) {
      mvwvline(window, p1.y, p1.x, '|', height);
      wnoutrefresh(window);
      return;
    }

    /*  Quadrant layout
     *  0,2 = '/'
     *  1,3 = '\'
     *
     *       |
     *    1  |  0
     *       |
     *  -----|------
     *       |
     *    2  |  3
     *       |
     * */

    Coords norm_p2 = {.x = p2.x, .y = p2.y};
    if (p1.x != 0) {
      norm_p2.x = p2.x - p1.x;
    }
    if (p1.y != 0) {
      norm_p2.y = p2.y - p1.y;
    }

    char x_dir = norm_p2.x > 0 ? 1 : -1;
    char y_dir = norm_p2.y > 0 ? 1 : -1;
    // logToFile("---> " + std::to_string(norm_p2.x) + ", " +
    //           std::to_string(norm_p2.y));

    char quadrant;
    if (x_dir == 1 && y_dir == 1) {
      quadrant = '\\';
    } else if (x_dir == -1 && y_dir == 1) {
      quadrant = '/';
    } else if (x_dir == -1 && y_dir == -1) {
      quadrant = '\\';
    } else if (x_dir == 1 && y_dir == -1) {
      quadrant = '/';
    }

    // logToFile("Quad: " + std::to_string(quadrant));
    for (int i{p1.x}; i != p2.x; i += x_dir) {
      // logToFile(std::to_string(i) + ", " + std::to_string(eq_line(i)));
      mvwprintw(window, eq_line(i), i, "%c", quadrant);
    }
    wnoutrefresh(window);
  }
};

/**
 * Creates a UIBox element - empty box with a border
 * */
class UIBox : public IUIElement<TypeId::Box> {
 public:
  size_t width;
  size_t height;
  int x;
  int y;

  /* Overrides the default window */
  UIBox(WINDOW* window) : IUIElement(window) {}
  UIBox(WINDOW* window, int w, int h, int xpos, int ypos)
      : IUIElement(window), width(w), height(h), x(xpos), y(ypos) {}

  UIBox() : UIBox(10, 5, 0, 0) {}
  UIBox(int w, int h, int xpos, int ypos)
      : width(w), height(h), x(xpos), y(ypos) {
    window = newwin(height, width, y, x);
  }

  void adjust() {
    wresize(window, height, width);
    mvwin(window, x, y);
  };

  void render() override {
    box(window, 0, 0);
    wnoutrefresh(window);
  }
};

/**
 * Creates a text element
 * */
class UIText : public IUIElement<TypeId::Text> {
 public:
  std::string label;
  size_t width;
  size_t height;
  int text_x;
  int text_y;
  int win_x;
  int win_y;

  UIText() = default;

  UIText(std::string label, int win_x = 0, int win_y = 0)
      : label(label), win_x(win_x), win_y(win_y) {
    width = label.length() + 2;
    height = 3;
    window = newwin(height, width, this->win_y, this->win_x);
  }

  UIText(std::string label, int w, int h, int win_x, int win_y)
      : label(label), width(w), height(h), win_x(win_x), win_y(win_y) {
    window = newwin(height, width, this->win_y, this->win_x);
  }

  UIText(WINDOW* window, std::string label)
      : IUIElement(window), label(label) {}

  /**
   * Generate a perfectly padded text window
   * */
  void auto_size() {
    width = label.length() + 2;
    height = 3;
    text_x = 1;
    text_y = 1;
    adjust();
  }

  /**
   * Updates the current window's x, y, width, and height
   * */
  void adjust() {
    wresize(window, height, width);
    mvwin(window, win_y, win_x);
  };

  void render() override {
    mvwprintw(window, text_y, text_x, "%s", label.c_str());
    wnoutrefresh(window);
  }
};

/**
 * UI Context class that sets up the main window and pointer events
 * */
class HawkTuahed {
 public:
  WINDOW* window;
  mmask_t oldmask;
  int screen_width;
  int screen_height;
  EventManager events;
  bool running;
  struct Coords {
    int x, y;
  };
  std::vector<std::shared_ptr<_AbstractUIElement>> children;

  struct MouseEvent : EventManager::Event {
    int x;
    int y;
    std::shared_ptr<_AbstractUIElement> element;
  } m_e;

  HawkTuahed() {
    window = initscr();
    // start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
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
    running = true;
  }

  ~HawkTuahed() {
    printf("\033[?1003l\n");
    curs_set(1);
    mousemask(oldmask, NULL);
    delwin(window);
    endwin();
    children.clear();
  }

  void tua() {
    dirty_render(children);
    int c;
    MEVENT event;
    Coords click_offset{};

    std::shared_ptr<_AbstractUIElement> current_element;
    c = wgetch(window);
    while (running) {
      c = wgetch(window);
      if (c == 'q') {
        running = false;
        break;
      }
      touchwin(stdscr);
      wnoutrefresh(window);
      if (c == KEY_RESIZE) {
        getmaxyx(window, screen_height, screen_width);
        events.dispatch("resize");
      }

      if (c == KEY_MOUSE) {
        while (getmouse(&event) == OK) {
          m_e.x = event.x;
          m_e.y = event.y;
          events.dispatch("mousemove", m_e);

          if (event.bstate & BUTTON1_PRESSED) {
            handle_click(event, click_offset, current_element, children);
            events.dispatch("mousedown", m_e);
          } else if (event.bstate & BUTTON1_RELEASED) {
            events.dispatch("mouseup", m_e);
            events.dispatch("click", m_e);
            current_element.reset();
          }
        }
      }
      dirty_render(children);
    }
  }

  void add_child(std::shared_ptr<_AbstractUIElement> child) {
    children.emplace_back(child);
  }

  void handle_click(
      MEVENT event,
      Coords click_offset,
      std::shared_ptr<_AbstractUIElement> current_element,
      std::vector<std::shared_ptr<_AbstractUIElement>> __children) {
    for (auto& child : __children) {
      if (child->composition.size() > 0) {
        handle_click(event, click_offset, current_element, child->composition);
      };

      switch (child->type()) {
        case TypeId::Box: {
          auto box = std::static_pointer_cast<UIBox>(child);
          int start_y, start_x;
          getbegyx(child->window, start_y, start_x);
          if (event.x >= start_x && event.x <= start_x + box->width &&
              event.y >= start_y && event.y <= start_y + box->height) {
            // logToFile(box->window);
            click_offset.x = event.x - start_x;
            click_offset.y = event.y - start_y;
            current_element = child;
            m_e.element = child;
          }
          break;
        }
        default:
          break;
      };
    }
  }

  /**
   * Recursive render callback to mark all windows as dirty.
   * */
  void render(std::vector<std::shared_ptr<_AbstractUIElement>> __children) {
    for (auto& child : __children) {
      if (child->composition.size() > 0) {
        render(child->composition);
      }
      child->render();
    }
  }

  /**
   * Batches render calls to reduce flickers
   * */
  void dirty_render(
      std::vector<std::shared_ptr<_AbstractUIElement>> __children) {
    wnoutrefresh(window);
    render(children);
    doupdate();
  }
};

/**
 * Create a button
 * */
class UIButton : public IUIElement<TypeId::Button> {
 public:
  template <typename F>
  UIButton(EventManager* events,
           std::string label,
           int x,
           int y,
           F&& callback) {
    auto box = std::make_shared<UIBox>();
    auto text = std::make_shared<UIText>(box->window, label);
    text->win_x = x;
    text->win_y = y;
    text->auto_size();
    composition.emplace_back(box);
    composition.emplace_back(text);
    this->window = box->window;

    events->subscribe<HawkTuahed::MouseEvent>(
        "click", [&](HawkTuahed::MouseEvent e) {
          if (e.element && e.element->window == this->window) {
            callback(e);
          }
        });
  }

  static std::shared_ptr<UIButton> create(
      EventManager* events,
      std::string label,
      int x,
      int y,
      std::function<void(HawkTuahed::MouseEvent)> callback = {}) {
    return std::make_shared<UIButton>(events, label, x, y, callback);
  }

  void render() {};
};

#endif
