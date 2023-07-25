	switch (id_value.size())
	{
	case 2:
		if (id_value == "as")
		{
			return token(
				315,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "if")
		{
			return token(
				294,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "in")
		{
			return token(
				314,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		break;
	case 3:
		if (id_value == "for")
		{
			return token(
				298,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "mut")
		{
			return token(
				319,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "let")
		{
			return token(
				317,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		break;
	case 4:
		if (id_value == "else")
		{
			return token(
				295,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "true")
		{
			return token(
				326,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "move")
		{
			return token(
				321,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "null")
		{
			return token(
				328,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "type")
		{
			return token(
				307,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "auto")
		{
			return token(
				316,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "enum")
		{
			return token(
				305,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		break;
	case 5:
		if (id_value == "false")
		{
			return token(
				327,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "class")
		{
			return token(
				303,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "break")
		{
			return token(
				330,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "while")
		{
			return token(
				297,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "defer")
		{
			return token(
				300,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "using")
		{
			return token(
				311,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		break;
	case 6:
		if (id_value == "sizeof")
		{
			return token(
				309,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "typeof")
		{
			return token(
				310,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "export")
		{
			return token(
				312,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "import")
		{
			return token(
				313,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "extern")
		{
			return token(
				318,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "struct")
		{
			return token(
				304,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "switch")
		{
			return token(
				296,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "return")
		{
			return token(
				299,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		break;
	case 8:
		if (id_value == "__move__")
		{
			return token(
				322,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "continue")
		{
			return token(
				331,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "typename")
		{
			return token(
				306,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "function")
		{
			return token(
				301,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "operator")
		{
			return token(
				302,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		break;
	case 9:
		if (id_value == "namespace")
		{
			return token(
				308,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "consteval")
		{
			return token(
				320,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "__forward")
		{
			return token(
				323,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		break;
	case 10:
		if (id_value == "__delete__")
		{
			return token(
				325,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		break;
	case 11:
		if (id_value == "unreachable")
		{
			return token(
				329,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		if (id_value == "__default__")
		{
			return token(
				324,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		break;
	case 13:
		if (id_value == "static_assert")
		{
			return token(
				332,
				id_value,
				stream.file_id, line, begin_it, end_it
			);
		}
		break;
	}
	return token(
		token::identifier,
		id_value,
		stream.file_id, line, begin_it, end_it
	);
