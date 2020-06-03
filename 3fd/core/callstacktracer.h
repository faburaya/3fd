#ifndef CALLSTACKTRACER_H
#define CALLSTACKTRACER_H

#include <3fd/core/preprocessing.h>
#include <string>
#include <vector>

namespace _3fd
{
namespace core
{
    using std::string;

    // forward declarations:
    class StackTracer;
    class StackDeactivationTrigger;


    /// <summary>
    /// Stores an history of procedure call events
    /// </summary>
    class CallStack
    {
    private:

        /// <summary>
        /// Represents a stack frame traced event
        /// </summary>
        struct Frame
        {
            const char *file;
            const char *function;
            unsigned long line;

            Frame(const char *p_file, 
                    const char *p_function, 
                    unsigned long p_line)
                : file(p_file), function(p_function), line(p_line) 
            {}
        };

        std::vector<Frame> m_stackFrames;

    public:

        CallStack(size_t logInitialCap);

        CallStack(const CallStack &) = delete;

        void RegisterFrame(const char *file, 
                            unsigned long line, 
                            const char *function) noexcept;

        bool PopStackFrameEntry() noexcept;

        string GetReport();
    };

        
    /// <summary>
    /// A facade for the call stack tracer, which is operated through macros defined on 'preprocessing.h'
    /// </summary>
    class CallStackTracer
    {
    private:

        thread_local static CallStack *callStack;

        /// <summary>
        /// Prevents a default instance of the <see cref="CallStackTracer"/> class from being created.
        /// </summary>
        CallStackTracer() {}

        static bool RegisterThread();
        static void UnregisterThread();

    public:

        /// <summary>
        /// Determines whether call stack tracing is ready for the calling thread.
        /// </summary>
        /// <returns>'true' if available, otherwise, 'false'.</returns>
        static bool IsReady()
        {
            /* This method  is necessary to prevent things such as an exception attempting to access CST when it is not started yet, 
            or any other piece of code trying to access the framework configurations before they were loaded. These situations might 
            take place if the initialization of the frameowrk core features runs into an error, when neither CST is ready or/nor the 
            configurations are available. In these cases, an exception still has to be used in order to signalize the failure, but it 
            cannot make use of such 'services'.

            Obs.: If the exception tries to invoke the RTM to get trace information, the framework initialization is requested again recursively, 
            leading to a stack overflow. */
            return callStack != nullptr;
        }

        static void TrackCall(const char *file, 
                                unsigned long line, 
                                const char *function);

        static void PopStackFrameEntry() noexcept;

        static string GetStackReport();
    };

    /// <summary>
    /// This class is a trick to automatize the call stack tracing using scope finalizations.
    /// </summary>
    class StackDeactivationTrigger
    {
    public:

        /// <summary>
        /// Finalizes an instance of the <see cref="StackDeactivationTrigger"/> class.
        /// </summary>
        ~StackDeactivationTrigger()
        {
            CallStackTracer::PopStackFrameEntry();
        }
    };

}// end of namespace core
}// end of namespace _3fd

#endif // header guard
