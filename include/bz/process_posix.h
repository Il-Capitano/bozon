#ifndef _bz_process_posix_h__
#define _bz_process_posix_h__

#include <unistd.h>
#include <sys/wait.h>

#include "array.h"
#include "vector.h"
#include "fixed_vector.h"

bz_begin_namespace

namespace internal
{

struct pipe_closer
{
	pipe_closer(int pipe_id)
		: _pipe_id(pipe_id),
		  _closed(false)
	{}

	~pipe_closer(void)
	{
		if (!this->_closed)
		{
			close(this->_pipe_id);
		}
	}

	pipe_closer(pipe_closer const &other) = delete;
	pipe_closer(pipe_closer &&other) = delete;
	pipe_closer &operator = (pipe_closer const &rhs) = delete;
	pipe_closer &operator = (pipe_closer &&rhs) = delete;

	void reset(void)
	{
		close(this->_pipe_id);
		this->_closed = true;
	}

private:
	int _pipe_id;
	bool _closed;
};

inline process_result_t run_process(const char* command, char * const *argv)
{
	auto result = process_result_t();

	static constexpr size_t PIPE_READ = 0;
	static constexpr size_t PIPE_WRITE = 1;

	int stdout_pipe[2];
	int stderr_pipe[2];

	if (pipe(stdout_pipe) < 0)
	{
		result.error_kind = process_error_kind::stdout_pipe_create_failed;
		result.return_code = -1;
		return result;
	}
	auto stdout_read_closer = pipe_closer(stdout_pipe[PIPE_READ]);
	auto stdout_write_closer = pipe_closer(stdout_pipe[PIPE_WRITE]);

	if (pipe(stderr_pipe) < 0)
	{
		result.error_kind = process_error_kind::stderr_pipe_create_failed;
		result.return_code = -1;
		return result;
	}
	auto stderr_read_closer = pipe_closer(stderr_pipe[PIPE_READ]);
	auto stderr_write_closer = pipe_closer(stderr_pipe[PIPE_WRITE]);

	int id = fork();
	if (id == 0)
	{
		// child continues here

		// redirect stdout
		if (dup2(stdout_pipe[PIPE_WRITE], STDOUT_FILENO) == -1)
		{
			exit(errno);
		}
		stdout_write_closer.reset();
		stdout_read_closer.reset();

		// redirect stderr
		if (dup2(stderr_pipe[PIPE_WRITE], STDERR_FILENO) == -1)
		{
			exit(errno);
		}
		stderr_write_closer.reset();
		stderr_read_closer.reset();

		// run child process image
		// replace this with any exec* function find easier to use ("man exec")
		char *envp[] = { nullptr };
		auto const execve_result = execve(command, argv, envp);

		perror("execve");
		// if we get here at all, an error occurred, but we are in the child
		// process, so just exit
		exit(execve_result);
	}
	else if (id > 0)
	{
		// parent continues here

		// close unused file descriptors, these are for child only
		stdout_write_closer.reset();
		stderr_write_closer.reset();

		int status;
		if (waitpid(id, &status, 0) < 0)
		{
			result.error_kind = process_error_kind::process_create_failed;
			result.return_code = -1;
		}
		else
		{
			result.return_code = status;
		}

		array<char, 1024> buffer = {};
		while (true)
		{
			// read stdout
			auto const stdout_read_size = read(stdout_pipe[PIPE_READ], buffer.data(), buffer.size());
			if (stdout_read_size != 0)
			{
				result.stdout_string += u8string_view(buffer.data(), buffer.data() + stdout_read_size);
			}

			// read stderr
			auto const stderr_read_size = read(stderr_pipe[PIPE_READ], buffer.data(), buffer.size());
			if (stderr_read_size != 0)
			{
				result.stderr_string += u8string_view(buffer.data(), buffer.data() + stderr_read_size);
			}

			if (stdout_read_size == 0 && stderr_read_size == 0)
			{
				break;
			}
		}
	}
	else
	{
		// failed to create child
		result.error_kind = process_error_kind::process_create_failed;
		result.return_code = -1;
	}
	return result;
}

// https://stackoverflow.com/a/20053121/11488457
inline void posix_write_escaped_string(u8string &buffer, u8string_view str)
{
	auto const str_bytes_range = basic_range(str.data(), str.data() + str.size());
	if (str_bytes_range.is_all([](auto const c) {
		return (c >= 'a' && c <= 'z')
			|| (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9')
			|| c == ','
			|| c == '.'
			|| c == '_'
			|| c == '+'
			|| c == ':'
			|| c == '@'
			|| c == '%'
			|| c == '/'
			|| c == '-';
	}))
	{
		buffer += str;
		return;
	}

	buffer += '\'';

	auto it = str.begin();
	while (it != str.end())
	{
		auto const next_it = str.find(it, '\'');
		auto const s = u8string_view(it, next_it);
		buffer += s;
		if (next_it != str.end())
		{
			// this is "'\''"
			buffer += "\'\\\'\'";
			it = next_it + 1;
		}
		else
		{
			break;
		}
	}

	buffer += '\'';
}

} // namespace internal

inline u8string make_command_string(u8string_view command, auto &&args)
{
	u8string command_string = "";

	internal::posix_write_escaped_string(command_string, command);
	for (auto const &arg : args)
	{
		command_string += ' ';
		internal::posix_write_escaped_string(command_string, arg);
	}

	return command_string;
}

inline process_result_t run_process(u8string_view command, auto &&args)
{
	auto argv = vector<char *>();
	argv.reserve(4);

	char bin_sh[] = "/bin/sh";
	char c[] = "-c";
	argv.push_back(bin_sh);
	argv.push_back(c);

	u8string command_string = make_command_string(command, std::forward<decltype(args)>(args));
	command_string += '\0';
	argv.push_back(command_string.data_as_char_ptr());

	argv.push_back(nullptr);

	return internal::run_process("/bin/sh", argv.data());
}

bz_end_namespace

#endif // _bz_process_posix_h__
