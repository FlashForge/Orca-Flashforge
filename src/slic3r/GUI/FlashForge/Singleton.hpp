#ifndef slic3r_GUI_Singleton_hpp_
#define slic3r_GUI_Singleton_hpp_

template<typename T>
class Singleton
{
public:
    static T *inst()
    {
        static T *s_inst = new T; // With C++11 static local or global data is initialized thread-safe.
        return s_inst;
    }
};

#endif
