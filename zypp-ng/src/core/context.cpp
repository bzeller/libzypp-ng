#include "core/context.h"
#include "core/private/context_p.h"
#include "core/private/base_p.h"

namespace zyppng {

	void ContextPrivate::onTaskFinished( Task::WeakPtr task )
	{
		Task::Ptr lockedTask = task.lock();
		if ( lockedTask == currentTask ) {

			taskFinishedConnection.disconnect();

			// clear reference counter on Task, this might delete the object
			currentTask.reset();
		}
	}

	Context::Context() : Base ( *new ContextPrivate )
	{

	}

	bool Context::runTask( std::shared_ptr<Task> task )
	{
		Z_D();
		if ( d->currentTask )
			return d->currentTask == task;

		d->currentTask = task;

		task->sigFinished().connect( sigc::mem_fun( *d_func(), &ContextPrivate::onTaskFinished ) );
		task->start( shared_this<Context>() );

		return true;
	}

}
