	case '!':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(33, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '=':
		{
			++stream;
			return make_regular_token(280, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(33, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case '#':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(35, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '#':
		{
			++stream;
			return make_regular_token(292, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(35, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case '%':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(37, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '=':
		{
			++stream;
			return make_regular_token(271, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(37, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case '&':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(38, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '&':
		{
			++stream;
			return make_regular_token(283, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		case '=':
		{
			++stream;
			return make_regular_token(274, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(38, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case '(':
	{
		auto const begin_it = stream.it;
		++stream;
		return make_regular_token(40, begin_it, stream.it, stream.file_id, stream.line, context);
	}
	case ')':
	{
		auto const begin_it = stream.it;
		++stream;
		return make_regular_token(41, begin_it, stream.it, stream.file_id, stream.line, context);
	}
	case '*':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(42, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '=':
		{
			++stream;
			return make_regular_token(269, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(42, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case '+':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(43, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '+':
		{
			++stream;
			return make_regular_token(265, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		case '=':
		{
			++stream;
			return make_regular_token(267, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(43, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case ',':
	{
		auto const begin_it = stream.it;
		++stream;
		return make_regular_token(44, begin_it, stream.it, stream.file_id, stream.line, context);
	}
	case '-':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(45, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '-':
		{
			++stream;
			return make_regular_token(266, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		case '=':
		{
			++stream;
			return make_regular_token(268, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		case '>':
		{
			++stream;
			return make_regular_token(286, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(45, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case '.':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(46, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '.':
		{
			++stream;
			if (stream.it == end)
			{
				return make_regular_token(289, begin_it, stream.it, stream.file_id, stream.line, context);
			}
			switch (*stream.it)
			{
			case '.':
			{
				++stream;
				return make_regular_token(291, begin_it, stream.it, stream.file_id, stream.line, context);
			}
			case '=':
			{
				++stream;
				return make_regular_token(290, begin_it, stream.it, stream.file_id, stream.line, context);
			}
			default:
			{
				return make_regular_token(289, begin_it, stream.it, stream.file_id, stream.line, context);
			}
			}
		}
		default:
		{
			return make_regular_token(46, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case '/':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(47, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '=':
		{
			++stream;
			return make_regular_token(270, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(47, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case ':':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(58, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case ':':
		{
			++stream;
			return make_regular_token(288, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(58, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case ';':
	{
		auto const begin_it = stream.it;
		++stream;
		return make_regular_token(59, begin_it, stream.it, stream.file_id, stream.line, context);
	}
	case '<':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(60, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '<':
		{
			++stream;
			if (stream.it == end)
			{
				return make_regular_token(272, begin_it, stream.it, stream.file_id, stream.line, context);
			}
			switch (*stream.it)
			{
			case '=':
			{
				++stream;
				return make_regular_token(277, begin_it, stream.it, stream.file_id, stream.line, context);
			}
			default:
			{
				return make_regular_token(272, begin_it, stream.it, stream.file_id, stream.line, context);
			}
			}
		}
		case '=':
		{
			++stream;
			return make_regular_token(281, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(60, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case '=':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(61, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '=':
		{
			++stream;
			return make_regular_token(279, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		case '>':
		{
			++stream;
			return make_regular_token(287, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(61, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case '>':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(62, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '=':
		{
			++stream;
			return make_regular_token(282, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		case '>':
		{
			++stream;
			if (stream.it == end)
			{
				return make_regular_token(273, begin_it, stream.it, stream.file_id, stream.line, context);
			}
			switch (*stream.it)
			{
			case '=':
			{
				++stream;
				return make_regular_token(278, begin_it, stream.it, stream.file_id, stream.line, context);
			}
			default:
			{
				return make_regular_token(273, begin_it, stream.it, stream.file_id, stream.line, context);
			}
			}
		}
		default:
		{
			return make_regular_token(62, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case '?':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(63, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '?':
		{
			++stream;
			return make_regular_token(293, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(63, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case '@':
	{
		auto const begin_it = stream.it;
		++stream;
		return make_regular_token(64, begin_it, stream.it, stream.file_id, stream.line, context);
	}
	case '[':
	{
		auto const begin_it = stream.it;
		++stream;
		return make_regular_token(91, begin_it, stream.it, stream.file_id, stream.line, context);
	}
	case ']':
	{
		auto const begin_it = stream.it;
		++stream;
		return make_regular_token(93, begin_it, stream.it, stream.file_id, stream.line, context);
	}
	case '^':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(94, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '=':
		{
			++stream;
			return make_regular_token(275, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		case '^':
		{
			++stream;
			return make_regular_token(284, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(94, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case '{':
	{
		auto const begin_it = stream.it;
		++stream;
		return make_regular_token(123, begin_it, stream.it, stream.file_id, stream.line, context);
	}
	case '|':
	{
		auto const begin_it = stream.it;
		++stream;
		if (stream.it == end)
		{
			return make_regular_token(124, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		switch (*stream.it)
		{
		case '=':
		{
			++stream;
			return make_regular_token(276, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		case '|':
		{
			++stream;
			return make_regular_token(285, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		default:
		{
			return make_regular_token(124, begin_it, stream.it, stream.file_id, stream.line, context);
		}
		}
	}
	case '}':
	{
		auto const begin_it = stream.it;
		++stream;
		return make_regular_token(125, begin_it, stream.it, stream.file_id, stream.line, context);
	}
	case '~':
	{
		auto const begin_it = stream.it;
		++stream;
		return make_regular_token(126, begin_it, stream.it, stream.file_id, stream.line, context);
	}
