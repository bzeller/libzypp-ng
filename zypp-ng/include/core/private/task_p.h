#ifndef ZYPP_NG_CORE_PRIVATE_TASK_P_H_INCLUDED
#define ZYPP_NG_CORE_PRIVATE_TASK_P_H_INCLUDED

#include <core/zyppglobal.h>
#include <core/task.h>
#include <core/signals.h>
#include <core/private/base_p.h>
#include <unordered_set>

namespace zyppng {
	class TaskPrivate : public BasePrivate
	{
		ZYPP_DECLARE_PUBLIC( Task )

	public:
		std::weak_ptr<Context> _myContext;

	public:
		signal<void ( const Task &, Task::State ) > _stateChanged;
		signal<void ( const Task &, std::string, int )> _progressChanged;
		signal<void ( const Task &, const Error & )>      _error;

		signal<void ( Task::WeakPtr, Task::WeakPtr)> _subTaskStarted;
		signal<void ( Task::WeakPtr )>  _started;
		signal<void ( Task::WeakPtr )>  _finished;

		int _currentProgress = 0;
		std::string _progressText;
	};
}

#endif
