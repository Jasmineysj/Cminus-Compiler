#include "syntax_tree.hpp"
int main()
{
  auto s = syntax("../testcase/array.cminus");
  auto tree = syntax_tree(s);
  auto printer = syntax_tree_printer();
  tree.run_visitor(printer);
  return 0;
}
