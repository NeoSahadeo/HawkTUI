#include <iostream>
#ifndef HAWKTUI_H
#define HAWKTUI_H

class Node {
 private:
  int data;

 public:
  Node() { this->data = 0; }
  // ~Node(){
  // 	delete this->data;
  // }

  void print_data() { std::cout << this->data << "\n"; }
};

#endif
