// Solves a circuit file with the SapecNG engine and compares the symbolic
// and numeric transfer functions with the expected strings.
//
// usage: solve_test <circuit.info|circuit.crc> <expected raw> <expected digit>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "model/circuit.h"
#include "model/metacircuit.h"
#include "parser/crc_circuit.h"
#include "parser/ir_circuit.h"


int main(int argc, char** argv)
{
  if(argc != 4) {
    std::cerr << "usage: " << argv[0] << " <file> <raw> <digit>\n";
    return 2;
  }

  const std::string file(argv[1]);
  std::ifstream in(file.c_str());
  if(!in) {
    std::cerr << "cannot open " << file << "\n";
    return 2;
  }

  std::unique_ptr<sapecng::abstract_parser> parser;
  if(file.size() > 4 && file.substr(file.size() - 4) == ".crc")
    parser.reset(new sapecng::crc_parser(in));
  else
    parser.reset(new sapecng::info_parser(in));

  sapecng::circuit circ;
  sapecng::circuit_builder builder(circ);
  parser->parse(builder);

  sapecng::metacircuit meta;
  meta(circ);

  using M = sapecng::metacircuit;
  const std::string raw =
    M::as_string(meta.raw().first) + " / " + M::as_string(meta.raw().second);
  const std::string digit =
    M::as_string(meta.digit().first) + " / " + M::as_string(meta.digit().second);

  std::cout << "raw:   " << raw << "\ndigit: " << digit << "\n";

  bool ok = true;
  if(raw != argv[2]) {
    std::cerr << "FAIL raw, expected:   " << argv[2] << "\n";
    ok = false;
  }
  if(digit != argv[3]) {
    std::cerr << "FAIL digit, expected: " << argv[3] << "\n";
    ok = false;
  }
  return ok ? 0 : 1;
}
