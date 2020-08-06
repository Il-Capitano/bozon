#include "expression.h"
#include "statement.h"

namespace ast
{

expr_compound::expr_compound(
	bz::vector<statement> _statements,
	expression            _final_expr
)
	: statements(std::move(_statements)),
	  final_expr(std::move(_final_expr))
{}

} // namespace ast
