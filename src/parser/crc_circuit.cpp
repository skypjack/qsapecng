/*
    SapecNG - Next Generation Symbolic Analysis Program for Electric Circuit
    Copyright (C) 2009, Michele Caini

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "exception/sapecngexception.h"
#include "parser/crc_circuit.h"
#include "parser/parser.h"

#ifndef Q_MOC_RUN
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_alternative.hpp>
#include <boost/spirit/include/lex_lexertl.hpp>
#include <boost/spirit/include/support_istream_iterator.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/fusion/include/vector.hpp>
#endif

#include <iostream>


namespace sapecng
{


namespace
{


template < typename Lexer >
struct crc_lexer: public boost::spirit::lex::lexer<Lexer>
{

public:
  crc_lexer()
  {
    this->self.add_pattern
        ("SID", "[RGCLVI]\\d+")
        ("CID", "[HEFY]\\d+")
        ("AID", "[A]\\d+")
        ("DGT", "\\d+")
        ("DBL", "[-+]?(\\d*\\.)?\\d+([eE][-+]?\\d+)?")
      ;

    sid_ = "{SID}";
    cid_ = "{CID}";
    aid_ = "{AID}";
    dgt_ = "{DGT}";
    dbl_ = "{DBL}";
    out_ = "\\.OUT";
    end_ = "\\.END";
    sps_ = "\\s+";

    this->self =
        sps_[boost::spirit::_pass
              = boost::spirit::lex::pass_flags::pass_ignore]
      | sid_
      | cid_
      | aid_
      | dgt_
      | dbl_
      | out_
      | end_
      ;
  }

public:
  boost::spirit::lex::token_def<std::string> sid_, cid_, aid_, out_, end_;
  boost::spirit::lex::token_def<double> dbl_;
  boost::spirit::lex::token_def<int> dgt_;
  boost::spirit::lex::token_def<> sps_;

};


template < typename Iterator >
struct crc_grammar: public boost::spirit::qi::grammar<Iterator>
{

public:
  template < typename Token >
  crc_grammar(Token const& tok, abstract_builder& builder)
    : crc_grammar::base_type(grammar_)
  {
    out_ =
      ( tok.out_ >> tok.dgt_ )
        [
          boost::phoenix::bind
          (
            &crc_grammar::out_rule,
            boost::phoenix::ref(*this),
            boost::phoenix::ref(builder),
            boost::spirit::_2
          )
        ];

    aelm_ =
      ( tok.aid_ >> tok.dgt_ >> tok.dgt_ >> tok.dgt_ >> tok.dgt_ )
        [
          boost::phoenix::bind
          (
            &crc_grammar::aelm_rule,
            boost::phoenix::ref(*this),
            boost::phoenix::ref(builder),
            boost::spirit::_1,
            boost::spirit::_2,
            boost::spirit::_3,
            boost::spirit::_4,
            boost::spirit::_5
          )
        ];

    celm_ =
      ( tok.cid_ >> tok.dgt_ >> tok.dgt_ >> tok.dgt_
          >> tok.dgt_ >> ( tok.dgt_ | tok.dbl_) >> tok.dgt_)
        [
          boost::phoenix::bind
          (
            &crc_grammar::celm_rule,
            boost::phoenix::ref(*this),
            boost::phoenix::ref(builder),
            boost::spirit::_1,
            boost::spirit::_6,
            boost::spirit::_7,
            boost::spirit::_2,
            boost::spirit::_3,
            boost::spirit::_4,
            boost::spirit::_5
          )
        ];

    selm_ =
      ( tok.sid_ >> tok.dgt_ >> tok.dgt_
          >> ( tok.dgt_ | tok.dbl_ ) >> tok.dgt_ )
        [
          boost::phoenix::bind
          (
            &crc_grammar::selm_rule,
            boost::phoenix::ref(*this),
            boost::phoenix::ref(builder),
            boost::spirit::_1,
            boost::spirit::_4,
            boost::spirit::_5,
            boost::spirit::_2,
            boost::spirit::_3
          )
        ];

    elms_ =
      ( selm_ | celm_ | aelm_ )
        [
          boost::phoenix::bind
          (
            &crc_grammar::elms_rule,
            boost::phoenix::ref(*this),
            boost::spirit::_1
          )
        ];

    circ_ =
      ( +elms_ >> out_ >> tok.end_ )
        [
          boost::phoenix::bind
          (
            &crc_grammar::circ_rule,
            boost::phoenix::ref(*this),
            boost::phoenix::ref(builder),
            boost::spirit::_1
          )
        ];

    grammar_ = circ_;

    circ_.name("circ");
    elms_.name("elms");
    selm_.name("selm");
    celm_.name("celm");
    aelm_.name("aelm");
    out_.name("out");
    grammar_.name("grammar");

    boost::spirit::qi::debug(circ_);
    boost::spirit::qi::debug(elms_);
    boost::spirit::qi::debug(selm_);
    boost::spirit::qi::debug(celm_);
    boost::spirit::qi::debug(aelm_);
    boost::spirit::qi::debug(out_);
    boost::spirit::qi::debug(grammar_);
  }

private:
  void out_rule(
      abstract_builder& builder,
      unsigned int v
    )
  {
    builder.add_out_component(v);
  }

  void aelm_rule(
      abstract_builder& builder,
      std::string name,
      unsigned int va,
      unsigned int vb,
      unsigned int vac,
      unsigned int vbc
    )
  {
    builder.add_quad_component(
        sapecng::abstract_builder::AO, name, 0, 1, va, vb, vac, vbc);
  }

  void celm_rule(
      abstract_builder& builder,
      std::string name,
      boost::variant<int, double> value,
      bool symbolic,
      unsigned int va,
      unsigned int vb,
      unsigned int vac,
      unsigned int vbc
    )
  {
    bool qct = false;
    abstract_builder::quad_component_type qctype;
    switch(name.at(0))
    {
    case 'H':
      qctype = abstract_builder::VCCS;
      qct = true;
      break;
    case 'E':
      qctype = abstract_builder::VCVS;
      qct = true;
      break;
    case 'F':
      qctype = abstract_builder::CCCS;
      qct = true;
      break;
    case 'Y':
      qctype = abstract_builder::CCVS;
      qct = true;
      break;
    case 'n':
      qctype = abstract_builder::n;
      qct = true;
      break;
    case 'K':
      qctype = abstract_builder::K;
      qct = true;
      break;
    }

    if(qct) {
      builder.add_quad_component(
        qctype, name,
        boost::get<double>(&value)
          ? boost::get<double>(value)
          : boost::get<int>(value),
        symbolic, va, vb, vac, vbc);
    }
  }

  void selm_rule(
      abstract_builder& builder,
      std::string name,
      boost::variant<int, double> value,
      bool symbolic,
      unsigned int va,
      unsigned int vb
    )
  {
    bool dct = false;
    abstract_builder::dual_component_type dctype;
    switch(name.at(0))
    {
    case 'R':
      dctype = abstract_builder::R;
      dct = true;
      break;
    case 'L':
      dctype = abstract_builder::L;
      dct = true;
      break;
    case 'G':
      dctype = abstract_builder::G;
      dct = true;
      break;
    case 'C':
      dctype = abstract_builder::C;
      dct = true;
      break;
    case 'V':
      dctype = abstract_builder::V;
      dct = true;
      break;
    case 'I':
      dctype = abstract_builder::I;
      dct = true;
      break;
    }

    if(dct) {
      builder.add_dual_component(
        dctype, name,
        boost::get<double>(&value)
          ? boost::get<double>(value)
          : boost::get<int>(value),
        symbolic, va, vb);
    }
  }

  void elms_rule(boost::spirit::unused_type v)
  {
    // nothing to do here.
  }

  void circ_rule(abstract_builder& builder, std::string v)
  {
    builder.flush();
  }

private:
  boost::spirit::qi::rule<Iterator> circ_, elms_, selm_, celm_, aelm_, out_;
  boost::spirit::qi::rule<Iterator> grammar_;

};


} // anonymous namespace


void crc_parser::parse(abstract_builder& builder)
{
  typedef
  boost::spirit::lex::lexertl::token<boost::spirit::istream_iterator>
  token_type;

  typedef
  boost::spirit::lex::lexertl::actor_lexer<token_type>
  lexer_type;

  typedef
  crc_lexer<lexer_type>::iterator_type
  iterator_type;

  std::ios_base::fmtflags flags = stream_.flags();
  stream_.unsetf(std::ios_base::skipws);

  crc_lexer<lexer_type> lexer;
  crc_grammar<iterator_type> parser(lexer, builder);
  boost::spirit::istream_iterator first(stream_), last;

  bool r = boost::spirit::lex::tokenize_and_parse(first, last, lexer, parser);

  stream_.flags(flags);

  if(!r)
    throw sapecng_exception();
}


void crc_builder::add_out_component(
    unsigned int v,
    std::map<std::string, std::string> props
  )
{
  out_ = v;
}


void crc_builder::add_dual_component(
    abstract_builder::dual_component_type c_type,
    std::string name,
    double value,
    bool symbolic,
    unsigned int va,
    unsigned int vb,
    std::map<std::string, std::string> props
  )
{
  std::vector<unsigned int> nodes;
  nodes.push_back(va);
  nodes.push_back(vb);

  switch(c_type)
  {
  case abstract_builder::R:
    {
      if(name.size() == 0 || name.at(0) != 'R')
        name = "R_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::G:
    {
      if(name.size() == 0 || name.at(0) != 'G')
        name = "G_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::L:
    {
      if(name.size() == 0 || name.at(0) != 'L')
        name = "L_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::C:
    {
      if(name.size() == 0 || name.at(0) != 'C')
        name = "C_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::V:
    {
      if(name.size() == 0 || name.at(0) != 'V')
        name = "V_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::I:
    {
      if(name.size() == 0 || name.at(0) != 'I')
        name = "I_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  default:
    break;
  }
}


void crc_builder::add_quad_component(
    abstract_builder::quad_component_type c_type,
    std::string name,
    double value,
    bool symbolic,
    unsigned int va,
    unsigned int vb,
    unsigned int vac,
    unsigned int vbc,
    std::map<std::string, std::string> props
  )
{
  std::vector<unsigned int> nodes;
  nodes.push_back(va);
  nodes.push_back(vb);
  nodes.push_back(vac);
  nodes.push_back(vbc);

  switch(c_type)
  {
  case abstract_builder::VCCS:
    {
      if(name.size() == 0 || name.at(0) != 'H')
        name = "H_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::VCVS:
    {
      if(name.size() == 0 || name.at(0) != 'E')
        name = "E_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::CCCS:
    {
      if(name.size() == 0 || name.at(0) != 'F')
        name = "F_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::CCVS:
    {
      if(name.size() == 0 || name.at(0) != 'Y')
        name = "Y_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::AO:
    {
      if(name.size() == 0 || name.at(0) != 'A')
        name = "A_" + name;

      stream_ << name << " ";
      for(std::vector<unsigned int>::size_type i = 0; i < nodes.size(); ++i)
        stream_ << nodes[i] << " ";
      stream_ << std::endl;

      break;
    }
  case abstract_builder::n:
    {
      if(name.size() == 0 || name.at(0) != 'n')
        name = "n_" + name;

      add_item(name, value, symbolic, nodes);

      break;
    }
  case abstract_builder::K:
    {
      if(name.size() == 0 || name.at(0) != 'K')
        name = "K_" + name;

      std::vector<unsigned int> lp_nodes, ls_nodes;
      lp_nodes.push_back(va);
      lp_nodes.push_back(vb);
      ls_nodes.push_back(vac);
      ls_nodes.push_back(vbc);

      add_item(
        props["lp:name"], strtol(props["lp:value"].c_str(), 0, 10),
        symbolic, lp_nodes);
      add_item(
        props["ls:name"], strtol(props["ls:value"].c_str(), 0, 10),
        symbolic, ls_nodes);

      stream_ << name << " " << props["lp:name"] << " " << props["ls:name"];
      stream_ << " " << value << " " << symbolic << std::endl;

      break;
    }
  default:
    break;
  }
}


void crc_builder::flush()
{
  stream_ << ".OUT " << out_ << std::endl;
  stream_ << ".END" << std::endl;
}


void crc_builder::add_item(
    std::string name,
    double value,
    bool symbolic,
    std::vector<unsigned int> nodes
  )
{
  stream_ << name << " ";
  for(std::vector<unsigned int>::size_type i = 0; i < nodes.size(); ++i)
    stream_ << nodes[i] << " ";
  stream_ << value << " ";
  stream_ << symbolic << std::endl;
}


}
