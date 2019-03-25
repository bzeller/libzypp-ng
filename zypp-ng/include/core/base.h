#ifndef ZYPP_NG_CORE_BASE_H_INCLUDED
#define ZYPP_NG_CORE_BASE_H_INCLUDED

#include <core/zyppglobal.h>
#include <core/signals.h>
#include <memory>
#include <unordered_set>
#include <vector>

namespace zyppng {

  class BasePrivate;

  class LIBZYPP_NG_EXPORT Base : public sigc::trackable, public std::enable_shared_from_this<Base>
  {
    NON_COPYABLE(Base);
    ZYPP_DECLARE_PRIVATE(Base)
  public:

    using Ptr = std::shared_ptr<Base>;
    using WeakPtr = std::weak_ptr<Base>;
    using TrackPtr = weak_trackable_ptr<Base>;

    Base ();
    virtual ~Base();

    TrackPtr parent () const;
    void addChild ( Base::Ptr child );
    void removeChild ( const Base::Ptr child );
    const std::unordered_set<Ptr> &children() const;

    template<typename T>
    std::vector< std::weak_ptr<T> > findChildren () const {
      std::vector< std::weak_ptr<T> > result;
      for ( Ptr p : children() ) {
        std::shared_ptr<T> casted = std::dynamic_pointer_cast<T>(p);
        if ( casted )
          result.push_back( std::weak_ptr<T>(casted) );
      }
      return result;
    }

    template<typename T>
    inline std::shared_ptr<T> shared_this () const {
      return std::static_pointer_cast<T>( shared_from_this() );
    }

    template<typename T>
    inline std::shared_ptr<T> shared_this () {
      return std::static_pointer_cast<T>( shared_from_this() );
    }

  protected:
    Base ( BasePrivate &dd );
    std::unique_ptr<BasePrivate> d_ptr;
  };

} // namespace zyppng

#endif // ZYPP_NG_CORE_BASE_H_INCLUDED
