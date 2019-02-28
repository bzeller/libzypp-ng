#ifndef ZYPP_NG_REPO_REFRESHREPO_H_INCLUDED
#define ZYPP_NG_REPO_REFRESHREPO_H_INCLUDED

#include <core/zyppglobal.h>
#include <core/task.h>
#include <string>
#include <vector>


namespace zyppng {

	class RefreshRepoPrivate;
	class LIBZYPP_NG_EXPORT RefreshRepo : public Task
	{
	public:
		RefreshRepo( std::string repo );

		// Task interface
		virtual IdString taskType () const override;
		void cancel() override;

	protected:
		void run( std::weak_ptr<Context> ctx ) override;
		bool advanceSimulation();

	private:
		ZYPP_DECLARE_PRIVATE( RefreshRepo )
	};

	class RefreshRepositoriesPrivate;
	class LIBZYPP_NG_EXPORT RefreshRepositories : public Task
	{
		ZYPP_DECLARE_PRIVATE(RefreshRepositories)
	public:
		RefreshRepositories( const std::vector<std::string> &repos );
		~RefreshRepositories();

		// Task interface
	public:
		IdString taskType() const override;
		void cancel() override;

	protected:
		void run(std::weak_ptr<Context> executionCtx) override;
	};

} // namespace zyppng

#endif // REFRESHREPO_H
