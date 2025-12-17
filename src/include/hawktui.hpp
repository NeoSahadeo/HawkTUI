#include <ncurses.h>
#include <unistd.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "log.hpp"
#ifndef HAWKTUI_H
#define HAWKTUI_H

#define M_PI_2x 6.283185307
#define M_PI_3rd 4.71238898

typedef struct Coords {
  int x, y;
} Coords;

enum class TypeId { None, Box, Text, Button, Line, Curve, Node };

enum class TypeFlags : uint8_t {
  None,
  Draggable,
  Editable,
};

/** @brief Manages event dispatch, subcription, and unsubscribe methods for any
 * instance of the class.
 * */
class EventManager {
 public:
  /** @brief Event base for any user-defined events.
   * @warning Custom events MUST inherit from Event to receive dispatch `data`.
   * */
  struct Event {};

 private:
  /** @brief Stores callback function and unique identifier for event tracking.
   */
  struct CallbackMeta {
    std::function<void(Event&)> func;
    std::uintptr_t id;
  };

  std::unordered_map<std::string, std::vector<CallbackMeta>> _handlers;

 public:
  /** @brief Subscribes a callback to an event name.
   * @tparam EventT Expected event type (must match dispatched events).
   * @param event Event name string to subscribe to.
   * @param callback Reference to a function to invoke on dispatch.
   * @note Callback recieves EventManager::Event struct during dispatch.
   * */
  template <typename EventT>
  void subscribe(const std::string& event, void (*cb)(const EventT&)) {
    _handlers[event].emplace_back(CallbackMeta{
        .func =
            [cb](const auto& e) mutable { cb(static_cast<const EventT&>(e)); },
        .id = reinterpret_cast<std::uintptr_t>(cb)});
  }

  /** @brief Dispatches an event to all subscribed callbacks.
   * @param event Event string to dispatch to.
   * @param data Event payload passed to matching callbacks.
   * @note Only callbacks subscribed to `event` receive the `data` parameter.
   * */
  void dispatch(const std::string& event, Event& data) {
    if (auto callbacks = _handlers.find(event); callbacks != _handlers.end()) {
      for (auto& vec : callbacks->second) {
        vec.func(data);
      }
    }
  }

  /** @brief Dispatches an event to all subscribed callbacks.
   * @param event Event string to dispatch to.
   * @param data Optional Event payload passed to matching callbacks (default:
   * empty).
   * @note Only callbacks subscribed to `event` receive the `data` parameter.
   * */
  void dispatch(const std::string& event, Event&& data = Event{}) {
    if (auto callbacks = _handlers.find(event); callbacks != _handlers.end()) {
      for (auto& vec : callbacks->second) {
        vec.func(data);
      }
    }
  }

  /** @brief Unsubscribes a callback from an event name.
   * @tparam EventT Expected event type (must match dispatched events).
   * @param event Event string name
   * @param callback Callback to remove (must match original subcription).
   * @note Safe if event/callback is not found.
   * */
  template <typename EventT>
  void unsubscribe(const std::string& event, void (*cb)(const EventT&)) {
    auto callbacks = _handlers.find(event);
    if (callbacks == _handlers.end() || callbacks->second.empty())
      return;

    auto& vecs = callbacks->second;
    vecs.erase(std::remove_if(vecs.begin(), vecs.end(),
                              [&cb](const CallbackMeta& v) {
                                return v.id ==
                                       reinterpret_cast<std::uintptr_t>(cb);
                              }),
               vecs.end());
  }
};

/** @brief Abstract base for all UI elements with nested composition support.
 *
 * Subclasses must implement render() and type().
 * Supports hierarchical UI trees
 * via composition member.
 * Defaults:
 *  empty composition
 *  TypeFlags::None
 *  null window
 * */
class AbstractUIElement {
 public:
  AbstractUIElement() = default;
  virtual ~AbstractUIElement() = default;

  std::vector<std::shared_ptr<AbstractUIElement>> composition{};
  TypeFlags flags{TypeFlags::None};
  WINDOW* window{nullptr};

  /** @brief Calls ncurses functions to draw UI element to its parent
   * ScreenContext window.
   * @note Automatically invoked by UIContext::render() during batch rendering.
   */
  virtual void render() = 0;

  /**@brief Returns this UI element's TypeId.
   * @warning TypeId must be registered with TypeId::register() before use.
   * @note Automatically invoked by UIContext::handle_click() during mouse
   * events.
   * */
  virtual TypeId type() = 0;
};

/** @brief Templated UI element base with shared window support and default
 * type().
 * */
template <TypeId T>
class IUIElement : public AbstractUIElement {
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
    mvwin(window, y, x);
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

/** @brief RAII wrapper for ncurses screen context with UI element hierarchy and
 * event management
 *
 * Initializes ncurses with mouse support, manages screen dimensions, and owns a
 * tree of UI elements.
 *
 * Handles lifecycle (initscr/endwin), resize events, and event dispatching via
 * EventManager.
 *
 * @note Non-copyable/moveable to ensure exclusive ownership of ncurses state.
 * */
class ScreenContext {
 private:
  WINDOW* _window;
  mmask_t _oldmask;
  int _screen_width;
  int _screen_height;
  bool _running;
  EventManager _events;
  std::vector<std::unique_ptr<AbstractUIElement>> _children;

  void configure_ncurses();
  void cleanup_ncurses();

 public:
  ScreenContext();
  ~ScreenContext();

  /** Disallows move/copy semantics */
  ScreenContext(const ScreenContext&) = delete;
  ScreenContext& operator=(const ScreenContext&) = delete;
  ScreenContext(ScreenContext&&) = delete;
  ScreenContext& operator=(ScreenContext&&) = delete;

  /** @brief Returns the current screen width in characters.
   * @return Cached width from last ncurses query.
   */
  int get_width() const { return _screen_width; }

  /** @brief Returns the current screen height in characters.
   * @return Cached height from last ncurses query.
   */
  int get_height() const { return _screen_height; }

  /**
   * @brief Returns the raw ncurses window handle.
   * @return Pointer to underlying WINDOW (do not delete).
   * @warning Direct ncurses usage bypasses RAII safety.
   */
  WINDOW* get_window() const { return _window; }

  /** @brief Returns const reference to child UI elements.
   * @return Read-only view of the element hierarchy.
   * @note Do not modify or store the returned reference.
   */
  const std::vector<std::unique_ptr<AbstractUIElement>>& get_children() const {
    return _children;
  }

  /** @brief Updates the cached screen dimensions from the ncurses
   * window.
   * @note Called automatically by the resize event */
  void update_dimensions();

  /** @brief Sets the running state of the screen context to false. */
  void stop() { _running = false; }

  /** @brief Queries the running state of the screen context. */
  bool is_running() const { return _running; }

  /** @brief Adds child UI element to this context's hierarchy.
   * @param child Unique ownership of element to add (moved into storage).
   * @warning Child ownership transfers to ScreenContext. Do not reference after
   * calling.
   * */
  void add_child(std::unique_ptr<AbstractUIElement> child);

  /** @brief Deletes child UI element from this context's hierarchy.
   * @param child Raw UI Element pointer (from any_child.get()).
   * @note Safe if not found. Compares object addresses only.
   * */
  void del_child(AbstractUIElement* child);

  /** @brief Clears the children UI hierarchy from this context.
   * @note Safe to call at any point. Destroys all owned children.
   * */
  void clear_children();

  /** @brief Returns reference to the event manager for this context.
   * @return Mutable reference to EventManager for event subscription/dispatch
   * */
  EventManager& event_manager() { return _events; }
};

void ScreenContext::add_child(std::unique_ptr<AbstractUIElement> child) {
  if (!child)
    return;
  _children.emplace_back(std::move(child));
}

void ScreenContext::del_child(AbstractUIElement* child) {
  if (!child)
    return;
  auto it =
      std::find_if(_children.begin(), _children.end(),
                   [&child](const auto& ptr) { return ptr.get() == child; });
  if (it != _children.end()) {
    _children.erase(it);
  }
}

ScreenContext::ScreenContext()
    : _window(nullptr),
      _oldmask(0),
      _screen_width(0),
      _screen_height(0),
      _running(false) {
  configure_ncurses();
}

ScreenContext::~ScreenContext() {
  cleanup_ncurses();
}

void ScreenContext::clear_children() {
  _children.clear();
}

void ScreenContext::configure_ncurses() {
  _window = initscr();
  if (!_window) {
    throw std::runtime_error("Failed to initialize ncurses window");
  }

  getmaxyx(_window, _screen_height, _screen_width);

  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  mmask_t mask;
  mask = ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION;
  curs_set(0);
  mouseinterval(0);
  mousemask(mask, &_oldmask);
  printf("\033[?1003h\n");
  fflush(stdout);
  _running = true;
}

void ScreenContext::cleanup_ncurses() {
  printf("\033[?1003l\n");
  fflush(stdout);
  curs_set(1);
  mousemask(_oldmask, nullptr);
  if (_window) {
    delwin(_window);
    _window = nullptr;
  }
  endwin();
}

void ScreenContext::update_dimensions() {
  if (!_window) {
    return;
  }
  getmaxyx(_window, _screen_height, _screen_width);
}

class UIContext;

class RegisteredEvents {
 public:
  struct MouseEvent : EventManager::Event {
    int x{};
    int y{};
    std::shared_ptr<AbstractUIElement> element{};
    Coords click_offset{};
    UIContext* ctx;
  } mouse_event;
};

/** @brief Internal renderer supporting shared_ptr and unique ptr hierarchies.
 * @note Private implementation. Use UIContext::start() instead.
 * */
class Renderer {
 public:
  /** @brief Recursively renders shared_ptr UI element hierarchy.
   * @param children Shared ownership hierarchy to render.
   * @note Internal. Use UIContext::start() instead to avoid flickering and perf
   * overhead.
   * */
  void render(std::vector<std::shared_ptr<AbstractUIElement>> children) {
    render_impl(children);
  };

  /** @brief Recursively renders unique_ptr UI element hierarchy.
   * @param children Unique hierarchy to render.
   * @note Internal. Use UIContext::start() instead to avoid flickering and perf
   * overhead.
   * */
  void render(const std::vector<std::unique_ptr<AbstractUIElement>>& children) {
    render_impl(children);
  };

 private:
  template <class C>
  void render_impl(const C& children) {
    for (auto& child : children) {
      if (child->composition.size() > 0) {
        render(child->composition);
      }
      child->render();
    }
  }
};

/** @brief RAII UI context that renders child hierarchy and dispatches ncurses
 * events.
 *
 * Extends ScreenContext with automatic child rendering and MEVENT dispatching
 * to EventManager. Inherits RegisteredEvents for event subscription.
 * */
class UIContext : public ScreenContext,
                  public Renderer,
                  public RegisteredEvents {
 public:
  UIContext() = default;
  ~UIContext() = default;

  /** @brief Starts the main ncurses event loop with child rendering and event
   * dispatch.
   * @note Blocks until stop() is called or window is destroyed.
   * */
  void start();

  /** @brief Detects mouse interaction on elements and dispatches mousedown,
   * mouseup, and click events.
   * @param children Shared ownership hierarchy to test for click hits.
   * @note Internal. Automatically called from unique_ptr-based handle_click().
   * */
  void handle_click(std::vector<std::shared_ptr<AbstractUIElement>> children);

  /** @brief Checks if child has a shared_ptr UI hierarchy and forwards handling
   * to the shared_ptr handle_click().
   * @param children Unique ownership hierarchy to test for shared_ptr
   * hierarchy.
   * @note Internal. Automatically called from start().
   * */
  void handle_click(
      const std::vector<std::unique_ptr<AbstractUIElement>>& children);

  /** @brief Wraps render() with a single wnoutrefresh() + doupdate() for
   * efficiency and to avoid flickering.
   * @note Internal method. Use start() instead to insure children exist.
   * */
  void batch_render();
};

void UIContext::start() {
  batch_render();

  WINDOW* win = get_window();
  MEVENT event;
  int c{};
  mouse_event.ctx = this;
  while (is_running()) {
    c = wgetch(win);
    if (c == 'q') {
      stop();
      break;
    }
    touchwin(stdscr);
    wnoutrefresh(win);
    if (c == KEY_RESIZE) {
      update_dimensions();
      event_manager().dispatch("resize");
    }

    if (c == KEY_MOUSE) {
      while (getmouse(&event) == OK) {
        mouse_event.x = event.x;
        mouse_event.y = event.y;
        event_manager().dispatch("mousemove", mouse_event);
        if (event.bstate & BUTTON1_PRESSED) {
          handle_click(get_children());
          event_manager().dispatch("mousedown", mouse_event);
        } else if (event.bstate & BUTTON1_RELEASED) {
          event_manager().dispatch("mouseup", mouse_event);
          event_manager().dispatch("click", mouse_event);
          mouse_event.element.reset();
        }
      }
    }
    batch_render();
  }
}

void UIContext::batch_render() {
  wnoutrefresh(get_window());
  render(get_children());
  doupdate();
}

void UIContext::handle_click(
    const std::vector<std::unique_ptr<AbstractUIElement>>& _children) {
  for (auto& child : _children) {
    if (child->composition.size() > 0) {
      UIContext::handle_click(child->composition);
    };
  }
}

void UIContext::handle_click(
    std::vector<std::shared_ptr<AbstractUIElement>> _children) {
  for (auto& child : _children) {
    if (child->composition.size() > 0) {
      handle_click(child->composition);
    };
    switch (child->type()) {
      case TypeId::Box: {
        auto box = std::static_pointer_cast<UIBox>(child);
        int start_y, start_x;
        getbegyx(child->window, start_y, start_x);
        if (mouse_event.x >= start_x && mouse_event.x <= start_x + box->width &&
            mouse_event.y >= start_y &&
            mouse_event.y <= start_y + box->height) {
          mouse_event.click_offset.x = mouse_event.x - start_x;
          mouse_event.click_offset.y = mouse_event.y - start_y;
          mouse_event.element = child;
        }
        break;
      }
      default:
        break;
    };
  }
}

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

    // events->subscribe<UIContext::MouseEvent>(
    //     "click", [&](UIContext::MouseEvent e) {
    //       if (e.element && e.element->window == this->window) {
    //         callback(e);
    //       }
    //     });
  }

  static std::shared_ptr<UIButton> create(
      EventManager* events,
      std::string label,
      int x,
      int y,
      std::function<void(UIContext::MouseEvent)> callback = {}) {
    return std::make_shared<UIButton>(events, label, x, y, callback);
  }

  void render() {};
};

#endif
