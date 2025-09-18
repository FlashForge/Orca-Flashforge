#ifndef _FNET_FREEINDESTRUCTOR_H_
#define _FNET_FREEINDESTRUCTOR_H_

namespace fnet {

template<typename Ty, typename TyFunc>
class FreeInDestructor
{
public:
    FreeInDestructor(const Ty &val, const TyFunc &func)
        : m_val(val)
        , m_func(func)
    {
    }
    ~FreeInDestructor()
    {
        m_func(m_val);
    }

private:
    Ty m_val;
    TyFunc m_func;
};

template<typename Ty, typename TyFunc, typename TyArg>
class FreeInDestructorArg
{
public:
    FreeInDestructorArg(const Ty &val, const TyFunc &func, const TyArg &arg)
        : m_val(val)
        , m_func(func)
        , m_arg(arg)
    {
    }
    ~FreeInDestructorArg()
    {
        m_func(m_val, m_arg);
    }

private:
    Ty m_val;
    TyFunc m_func;
    TyArg m_arg;
};

} // namespace fnet

#endif
