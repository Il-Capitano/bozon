#include "ast_type.h"

void ast_typespec::resolve(void)
{
	switch (this->kind())
	{
	case unresolved:
	{
		auto &type = this->get<unresolved>();

		auto stream = type.typespec.begin;
		auto end    = type.typespec.end;

		if (stream == end)
		{
			this->emplace<none>();
			return;
		}

		switch (stream->kind)
		{
		case token::identifier:
		{
			auto id = stream->value;
			++stream;
			if (stream != end)
			{
				bad_token(stream);
			}

			this->emplace<name>(id);
			return;
		}

		case token::kw_const:
		{
			++stream; // 'const'
			this->emplace<constant>(
				make_ast_typespec(ast_ts_unresolved({ stream, end }))
			);

			this->get<constant>().base->resolve();
			return;
		}

		case token::star:
		{
			++stream; // '*'
			this->emplace<pointer>(
				make_ast_typespec(ast_ts_unresolved({ stream, end }))
			);

			this->get<pointer>().base->resolve();
			return;
		}

		case token::ampersand:
		{
			++stream; // '&'
			this->emplace<reference>(
				make_ast_typespec(ast_ts_unresolved({ stream, end }))
			);

			this->get<reference>().base->resolve();
			return;
		}

		case token::kw_function:
		{
			++stream; // 'function'
			assert_token(stream, token::paren_open, false);

			bz::vector<ast_typespec_ptr> param_types = {};
			while (stream != end && stream->kind != token::paren_close)
			{
				++stream;
				auto param_begin = stream;
				while (
					stream != end
					&& stream->kind != token::comma
					&& stream->kind != token::paren_close
				)
				{
					++stream;
				}
				auto param_end = stream;

				param_types.emplace_back(
					make_ast_typespec(
						ast_ts_unresolved({ param_begin, param_end })
					)
				);
				param_types.back()->resolve();
			}
			assert_token(stream, token::paren_close);
			assert_token(stream, token::arrow);

			auto ret_type = make_ast_typespec(
				ast_ts_unresolved({ stream, end })
			);
			ret_type->resolve();

			this->emplace<function>(ret_type, std::move(param_types));
			return;
		}

		default:
			bad_token(stream, "Expected a type");
			return;
		}
	}

	case name:
		return;

	case constant:
		this->get<constant>().base->resolve();
		return;

	case pointer:
		this->get<pointer>().base->resolve();
		return;

	case reference:
		this->get<reference>().base->resolve();
		return;

	case function:
	{
		auto &fn = this->get<function>();
		fn.return_type->resolve();
		for (auto &t : fn.argument_types)
		{
			t->resolve();
		}
		return;
	}

	case none:
		return;

	default:
		assert(false);
		return;
	}
}
