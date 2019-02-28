#ifndef ZYPP_NG_CORE_PRIVATE_CONTEXT_P_H_INCLUDED
#define ZYPP_NG_CORE_PRIVATE_CONTEXT_P_H_INCLUDED

#include <core/zyppglobal.h>
#include <core/private/base_p.h>
#include <core/context.h>
#include <core/task.h>

#include <queue>

namespace zyppng {

	class Task;

	class ContextPrivate : public BasePrivate
	{
		ZYPP_DECLARE_PUBLIC(Context)
	public:
		Task::Ptr currentTask;
		connection taskFinishedConnection;

	private:
		void onTaskFinished ( Task::WeakPtr task );
	};
}


#endif
