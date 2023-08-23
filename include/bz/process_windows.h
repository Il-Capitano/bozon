#ifndef _bz_process_windows_h__
#define _bz_process_windows_h__

#ifdef _WIN32

#include <windows.h>
#undef min
#undef max
#include <thread>

#include "array_view.h"
#include "fixed_vector.h"
#include "array.h"

bz_begin_namespace

namespace internal
{

struct handle_closer
{
	handle_closer(HANDLE h)
		: _h(h)
	{}

	~handle_closer(void)
	{
		if (this->_h != INVALID_HANDLE_VALUE)
		{
			CloseHandle(this->_h);
		}
	}

	handle_closer(handle_closer const &other) = delete;
	handle_closer(handle_closer &&other) = delete;
	handle_closer &operator = (handle_closer const &rhs) = delete;
	handle_closer &operator = (handle_closer &&rhs) = delete;

	void reset(void)
	{
		if (this->_h != INVALID_HANDLE_VALUE)
		{
			CloseHandle(this->_h);
			this->_h = INVALID_HANDLE_VALUE;
		}
	}

private:
	HANDLE _h;
};

struct attribute_list_freer
{
	attribute_list_freer(LPPROC_THREAD_ATTRIBUTE_LIST attribute_list)
		: _attribute_list(attribute_list)
	{}

	~attribute_list_freer(void)
	{
		if (this->_attribute_list != nullptr)
		{
			HeapFree(GetProcessHeap(), 0, this->_attribute_list);
		}
	}

	attribute_list_freer(attribute_list_freer const &other) = delete;
	attribute_list_freer(attribute_list_freer &&other) = delete;
	attribute_list_freer &operator = (attribute_list_freer const &rhs) = delete;
	attribute_list_freer &operator = (attribute_list_freer &&rhs) = delete;

	void reset(void)
	{
		if (this->_attribute_list != nullptr)
		{
			HeapFree(GetProcessHeap(), 0, this->_attribute_list);
			this->_attribute_list = nullptr;
		}
	}

private:
	LPPROC_THREAD_ATTRIBUTE_LIST _attribute_list;
};

struct attribute_list_deleter
{
	attribute_list_deleter(LPPROC_THREAD_ATTRIBUTE_LIST attribute_list)
		: _attribute_list(attribute_list)
	{}

	~attribute_list_deleter(void)
	{
		if (this->_attribute_list != nullptr)
		{
			DeleteProcThreadAttributeList(this->_attribute_list);
		}
	}

	attribute_list_deleter(attribute_list_deleter const &other) = delete;
	attribute_list_deleter(attribute_list_deleter &&other) = delete;
	attribute_list_deleter &operator = (attribute_list_deleter const &rhs) = delete;
	attribute_list_deleter &operator = (attribute_list_deleter &&rhs) = delete;

	void reset(void)
	{
		if (this->_attribute_list != nullptr)
		{
			DeleteProcThreadAttributeList(this->_attribute_list);
			this->_attribute_list = nullptr;
		}
	}

private:
	LPPROC_THREAD_ATTRIBUTE_LIST _attribute_list;
};

// https://stackoverflow.com/a/46348112/11488457
inline process_result_t run_process(u8string_view command_line, u8string_view command_run_dir)
{
	auto result = process_result_t();

	SECURITY_ATTRIBUTES  security_attributes = {
		.nLength = sizeof (SECURITY_ATTRIBUTES),
		.lpSecurityDescriptor = nullptr,
		.bInheritHandle = TRUE,
	};

	HANDLE stdout_reader = INVALID_HANDLE_VALUE;
	HANDLE stdout_writer = INVALID_HANDLE_VALUE;
	if (!CreatePipe(&stdout_reader, &stdout_writer, &security_attributes, 0))
	{
		result.error_kind = process_error_kind::stdout_pipe_create_failed;
		result.return_code = -1;
		return result;
	}

	auto stdout_reader_closer = handle_closer(stdout_reader);
	auto stdout_writer_closer = handle_closer(stdout_writer);

	if (!SetHandleInformation(stdout_reader, HANDLE_FLAG_INHERIT, 0))
	{
		result.error_kind = process_error_kind::stdout_pipe_create_failed;
		result.return_code = -1;
		return result;
	}

	HANDLE stderr_reader = INVALID_HANDLE_VALUE;
	HANDLE stderr_writer = INVALID_HANDLE_VALUE;
	if (!CreatePipe(&stderr_reader, &stderr_writer, &security_attributes, 0))
	{
		result.error_kind = process_error_kind::stderr_pipe_create_failed;
		result.return_code = -1;
		return result;
	}

	auto stderr_reader_closer = handle_closer(stderr_reader);
	auto stderr_writer_closer = handle_closer(stderr_writer);

	if (!SetHandleInformation(stderr_reader, HANDLE_FLAG_INHERIT, 0))
	{
		result.error_kind = process_error_kind::stderr_pipe_create_failed;
		result.return_code = -1;
		return result;
	}

	// Make a copy because CreateProcess needs to modify string buffer
	auto command_line_str = fixed_vector<char>(command_line.size() + 1, 0);
	std::memcpy(command_line_str.data(), command_line.data(), command_line.size());

	auto command_run_dir_str = fixed_vector<char>(command_run_dir.size() + 1, 0);
	std::memcpy(command_run_dir_str.data(), command_run_dir.data(), command_run_dir.size());

	// https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873
	LPPROC_THREAD_ATTRIBUTE_LIST attribute_list = nullptr;
	auto const list_freer = attribute_list_freer(attribute_list);
	SIZE_T size = 0;
	auto success = InitializeProcThreadAttributeList(nullptr, 1, 0, &size) || GetLastError() == ERROR_INSUFFICIENT_BUFFER;

	if (success)
	{
		attribute_list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, size));
		success = attribute_list != nullptr;
	}

	if (success)
	{
		success = InitializeProcThreadAttributeList(attribute_list, 1, 0, &size);
	}

	auto const list_deleter = attribute_list_deleter(success ? attribute_list : nullptr);
	HANDLE handles_to_inherit[] = {
		stdout_writer,
		stderr_writer,
	};
	if (success)
	{
		success = UpdateProcThreadAttribute(
			attribute_list,
			0,
			PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
			handles_to_inherit,
			2 * sizeof (HANDLE),
			nullptr,
			nullptr
		);
	}

	PROCESS_INFORMATION process_info;
	ZeroMemory(&process_info, sizeof (PROCESS_INFORMATION));
	if (success)
	{
		STARTUPINFOEX startup_info = {
			.StartupInfo = {
				.cb = sizeof (STARTUPINFO),
				.hStdInput = 0,
				.hStdOutput = stdout_writer,
				.hStdError = stderr_writer,
			},
			.lpAttributeList = attribute_list,
		};

		startup_info.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
		success = CreateProcess(
			nullptr,
			command_line_str.data(),
			nullptr,
			nullptr,
			TRUE,
			EXTENDED_STARTUPINFO_PRESENT,
			nullptr,
			nullptr,
			&startup_info.StartupInfo,
			&process_info
		);
	}

	auto process_closer = handle_closer(process_info.hProcess);
	auto thread_closer = handle_closer(process_info.hThread);

	stdout_writer_closer.reset();
	stderr_writer_closer.reset();

	if(!success)
	{
		result.error_kind = process_error_kind::process_create_failed;
		result.return_code = -1;
		return result;
	}

	thread_closer.reset();

	if (stdout_reader || stderr_reader)
	{
		array<char, 1024> buffer = {};
		WINBOOL stdout_success = TRUE;
		WINBOOL stderr_success = TRUE;
		while (stdout_success || stderr_success)
		{
			if (stdout_reader)
			{
				DWORD read_size = 0;
				stdout_success = ReadFile(
					stdout_reader,
					buffer.data(),
					(DWORD)buffer.size(),
					&read_size,
					nullptr
				);
				stdout_success = stdout_success && read_size != 0;
				if (stdout_success)
				{
					result.stdout_string += u8string_view(buffer.data(), buffer.data() + read_size);
				}
			}

			if (stderr_reader)
			{
				DWORD read_size = 0;
				stderr_success = ReadFile(
					stderr_reader,
					buffer.data(),
					(DWORD)buffer.size(),
					&read_size,
					nullptr
				);
				stderr_success = stderr_success && read_size != 0;
				if (stderr_success)
				{
					result.stderr_string += u8string_view(buffer.data(), buffer.data() + read_size);
				}
			}
		}
	}

	WaitForSingleObject(process_info.hProcess, INFINITE);
	DWORD return_code = 0;
	if(GetExitCodeProcess(process_info.hProcess, &return_code))
	{
		result.return_code = return_code;
	}
	else
	{
		result.return_code = -1;
	}

	return result;
}

inline void windows_write_escaped_string(u8string &buffer, u8string_view str)
{
	if (str.contains_any(" \t\""))
	{
		buffer += '\"';

		auto it = str.begin();
		while (it != str.end())
		{
			auto const next_it = str.find(it, '\"');
			auto const s = u8string_view(it, next_it);
			buffer += s;
			// '\"' needs to escaped as '\\\"'
			if (next_it != str.end())
			{
				if (s.ends_with('\\'))
				{
					// this is '\\"'
					buffer += "\\\\\"";
				}
				else
				{
					buffer += "\\\"";
				}
				it = next_it + 1;
			}
			else
			{
				break;
			}
		}

		if (buffer.ends_with('\\'))
		{
			buffer += "\\\"";
		}
		else
		{
			buffer += '\"';
		}
	}
	else
	{
		buffer += str;
	}
}

} // namespace internal

inline u8string make_command_string(u8string_view command, auto &&args)
{
	u8string command_string = "";

	internal::windows_write_escaped_string(command_string, command);
	for (auto const &arg : std::forward<decltype(args)>(args))
	{
		command_string += ' ';
		internal::windows_write_escaped_string(command_string, arg);
	}

	return command_string;
}

inline process_result_t run_process(u8string_view command, auto &&args)
{
	return internal::run_process(make_command_string(command, std::forward<decltype(args)>(args)), ".");
}

bz_end_namespace

#endif // _WIN32

#endif // _bz_process_windows_h__
