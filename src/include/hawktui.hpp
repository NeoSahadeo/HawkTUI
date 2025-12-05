#include <ncurses.h>
#include <iostream>
#include <memory>
#include <vector>
#ifndef HAWKTUI_H
#define HAWKTUI_H

class UIElement {
 public:
  virtual ~UIElement() = default;
  virtual void render() = 0;
};

/**
 * UI Context class that sets up the main window and pointer events
 * */
class UIContext : public UIElement {
 public:
  WINDOW* window;
  mmask_t oldmask;
  std::vector<std::unique_ptr<UIElement>> children;

  UIContext() {
    window = initscr();
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
  }

  void add_child(std::unique_ptr<UIElement> child) {
    children.push_back(std::move(child));
  }

  void render() {
    for (auto& child : children)
      child->render();
  }
};

class Box {};

#endif
