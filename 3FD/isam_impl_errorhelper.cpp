#include "stdafx.h"
#include "isam_impl.h"
#include "configuration.h"
#include <codecvt>
#include <cassert>

namespace _3fd
{
namespace isam
{
    ///////////////////////////
    // ErrorHelper Class
    ///////////////////////////

    /// <summary>
    /// Handles a return code if it refers to an error condition.
    /// An error will raise an exception, whereas a warning will be logged.
    /// </summary>
    /// <param name="jetInstance">The instance handle in the context.</param>
    /// <param name="jetSession">The session handle in the context.</param>
    /// <param name="errorCode">The error code.</param>
    /// <param name="what">The main part of the error message.
    /// The details will be retrieved from the ISAM engine implementation using the error code.</param>
    void ErrorHelper::HandleError(JET_INSTANCE jetInstance,
                                  JET_SESID jetSession,
                                  JET_ERR errorCode,
                                  const char *what)
    {
        if (errorCode == JET_errSuccess)
            return;
        else
        {
            if (errorCode < 0) // An error has a negative numeric code (http://msdn.microsoft.com/EN-US/library/gg269297) and will trigger an exception:
                throw MakeException(jetInstance, jetSession, errorCode, what);
            else
            {
                // But a warning will add an entry to the log output:
                core::Logger::Write(
                    MakeException(jetInstance, jetSession, errorCode, what),
                    core::Logger::PRIO_WARNING
                );
            }
        }
    }

    /// <summary>
    /// Handles a return code if it refers to an error condition.
    /// An error will raise an exception, whereas a warning will be logged.
    /// </summary>
    /// <param name="jetInstance">The instance handle in the context.</param>
    /// <param name="jetSession">The session handle in the context.</param>
    /// <param name="errorCode">The error code.</param>
    /// <param name="what">A functor which wich generates the main part of the error message.
    /// The details will be retrieved from the ISAM engine implementation using the error code.</param>
    void ErrorHelper::HandleError(JET_INSTANCE jetInstance,
                                  JET_SESID jetSession,
                                  JET_ERR errorCode,
                                  const std::function<string(void)> &what)
    {
        if (errorCode == JET_errSuccess)
            return;
        else
        {
            if (errorCode < 0) // An error has a negative numeric code (http://msdn.microsoft.com/EN-US/library/gg269297) and will trigger an exception:
                throw MakeException(jetInstance, jetSession, errorCode, what);
            else
            {
                // But a warning will add an entry to the log output:
                core::Logger::Write(
                    MakeException(jetInstance, jetSession, errorCode, what),
                    core::Logger::PRIO_WARNING
                );
            }
        }
    }

    /// <summary>
    /// Handles a return code if it refers to an error condition.
    /// </summary>
    /// <param name="jetInstance">The instance handle in the context.</param>
    /// <param name="jetSession">The session handle in the context.</param>
    /// <param name="errorCode">The error code.</param>
    /// <param name="what">The main part of the error message.
    /// The details will be retrieved from the ISAM engine implementation using the error code.</param>
    /// <param name="prio">The priority of the error. (optional, in case the error must be logged)</param>
    void ErrorHelper::LogError(JET_INSTANCE jetInstance,
                               JET_SESID jetSession,
                               JET_ERR errorCode,
                               const char *what,
                               core::Logger::Priority prio)
    {
        if (errorCode == JET_errSuccess)
            return;
        else
            core::Logger::Write(MakeException(jetInstance, jetSession, errorCode, what), prio);
    }

    /// <summary>
    /// Handles a return code if it refers to an error condition.
    /// </summary>
    /// <param name="jetInstance">The instance handle in the context.</param>
    /// <param name="jetSession">The session handle in the context.</param>
    /// <param name="errorCode">The error code.</param>
    /// <param name="what">A functor which wich generates the main part of the error message.
    /// The details will be retrieved from the ISAM engine implementation using the error code.</param>
    /// <param name="prio">The priority of the error. (optional, in case the error must be logged)</param>
    void ErrorHelper::LogError(JET_INSTANCE jetInstance,
                               JET_SESID jetSession,
                               JET_ERR errorCode,
                               const std::function<string(void)> &what,
                               core::Logger::Priority prio)
    {
        if (errorCode == JET_errSuccess)
            return;
        else
            core::Logger::Write(MakeException(jetInstance, jetSession, errorCode, what), prio);
    }

    /// <summary>
    /// Makes an exception from an error code returned by the ISAM engine implementation.
    /// This function must only be used when it is certain that an error took place.
    /// </summary>
    /// <param name="jetInstance">The instance handle in the context.</param>
    /// <param name="jetSession">The session handle in the context.</param>
    /// <param name="errorCode">The error code.</param>
    /// <param name="what">The main part of the error message.
    /// The details will be retrieved from the ISAM engine implementation using the error code.</param>
    /// <returns>An application exception describing the error.</returns>
    core::AppException<std::exception> ErrorHelper::MakeException(
        JET_INSTANCE jetInstance,
        JET_SESID jetSession,
        JET_ERR errorCode,
        const char *what)
    {
        return MakeException(jetInstance, jetSession, errorCode, [what]() { return what; });
    }

    /// <summary>
    /// Makes an exception from an error code returned by the ISAM engine implementation.
    /// This function must only be used when it is certain that an error took place.
    /// </summary>
    /// <param name="jetInstance">The instance handle in the context.</param>
    /// <param name="jetSession">The session handle in the context.</param>
    /// <param name="errorCode">The error code.</param>
    /// <param name="what">A functor which wich generates the main part of the error message.
    /// The details will be retrieved from the ISAM engine implementation using the error code.</param>
    /// <returns>An application exception describing the error.</returns>
    core::AppException<std::exception> ErrorHelper::MakeException(
        JET_INSTANCE jetInstance,
        JET_SESID jetSession,
        JET_ERR errorCode,
        const std::function<string(void)> &what)
    {
        _ASSERTE(errorCode != JET_errSuccess); // This function must only be called when it is known an error took place

        try
        {
            std::array<wchar_t, 256> ucs2text;
            auto param = static_cast<JET_API_PTR> (errorCode);
            auto rcode = JetGetSystemParameterW(jetInstance, jetSession, JET_paramErrorToString, &param, ucs2text.data(), ucs2text.size());

            if (rcode == JET_errSuccess)
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
                auto utf8text = transcoder.to_bytes(ucs2text.data());
                return core::AppException<std::exception>(what().c_str(), utf8text);
            }
            else
            {
                std::ostringstream oss;
                oss << "Microsoft ESE API returned error code " << errorCode
                    << " - Another failure prevented proper error details to be loaded (error "
                    << rcode << ")";

                return core::AppException<std::exception>(what().c_str(), oss.str());
            }
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Microsoft ESE API returned error code " << errorCode
                << " - Another failure prevented proper error details to be loaded: "
                << ex.what();

            return core::AppException<std::exception>(what().c_str(), oss.str());
        }
    }

}// end namespace isam
}// end namespace _3fd
