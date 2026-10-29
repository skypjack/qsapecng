// Regression test for issue #6 (segmentation fault on Resolve).
//
// metacircuit::compress() removes zero coefficients and then the powers of s
// left without terms. It used to erase a map entry and then advance the
// erased iterator, which crashed depending on the standard library.
// Built with _GLIBCXX_DEBUG (see tests/CMakeLists.txt) the old code aborts
// deterministically. (No "#define private public": MSVC encodes access in
// symbol names, so that would not link.)

#include <iostream>
#include <utility>

#include "model/metacircuit.h"


namespace sapecng
{

// declared a friend in metacircuit (simplify() is private)
struct metacircuit_test_access
{
  static void simplify(metacircuit& m,
      std::pair<metacircuit::expression, metacircuit::expression>& p)
    { m.simplify(p); }
};

}


int main()
{
  using M = sapecng::metacircuit;

  M::expression num, den;
  num[1].push_back(M::basic_expression(0., {"x"}));   // cancelled: must vanish
  num[2].push_back(M::basic_expression(2., {"y"}));
  den[0].push_back(M::basic_expression(1., {}));

  std::pair<M::expression, M::expression> p(num, den);
  M meta;
  sapecng::metacircuit_test_access::simplify(meta, p);

  std::cout << M::as_string(p.first) << " / " << M::as_string(p.second) << "\n";

  if(p.first.size() != 1 || p.first.count(2) != 1) {
    std::cerr << "FAIL: expected only the s^2 term in the numerator\n";
    return 1;
  }
  return 0;
}
