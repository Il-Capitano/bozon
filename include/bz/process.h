#ifndef _bz_process_h__
#define _bz_process_h__

#include "core.h"

#include "u8string.h"
#include "u8string_view.h"

bz_begin_namespace

enum class process_error_kind : int
{
	none = 0,
	stdout_pipe_create_failed,
	stderr_pipe_create_failed,
	process_create_failed,
};

struct process_result_t
{
	process_error_kind error_kind;
	int return_code;
	u8string stdout_string;
	u8string stderr_string;
};

inline bz::u8string make_command_string(u8string_view command, auto &&args);
inline process_result_t run_process(u8string_view command, auto &&args);

bz_end_namespace

#ifdef _WIN32
#include "process_windows.h"
#else
#include "process_posix.h"
#endif

#endif // _bz_process_h__
