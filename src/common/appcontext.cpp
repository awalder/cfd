#include "common/appcontext.h"

namespace app
{

auto AppContext::getParamsStruct() const -> ParamsCPtr
{
    return _parameters;
}

auto AppContext::getDispatcher() -> entt::dispatcher&
{
    return _dispatcher;
}

} // namespace app
