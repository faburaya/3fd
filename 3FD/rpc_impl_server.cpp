#include "stdafx.h"
#include "rpc_impl_server.h"
#include "rpc_impl_util.h"
#include "callstacktracer.h"
#include "logger.h"

#include <NtDsAPI.h>
#include <codecvt>
#include <sstream>

namespace _3fd
{
    namespace rpc
    {
        /////////////////////////
        // RpcServer Class
        /////////////////////////

        std::unique_ptr<RpcServerImpl> RpcServer::uniqueObject;

        std::mutex RpcServer::singletonAccessMutex;

        /// <summary>
        /// Initializes the RPC server before running it.
        /// </summary>
        /// <param name="protSeq">The protocol sequence to be used.</param>
        /// <param name="serviceName">A friendly name to identify this service. Such name will be
        /// employed for both informational (will be displayed in admin tools that expose listening
        /// servers, as a description) and security purposes (the "service class" in the SPN). Thus
        /// it must be unique (in a way the generated SPN will not collide with another in the Windows
        /// Active directory), and cannot contain characters such as '/', '&lt;' or '&gt;'.</param>
        /// <param name="useActDirSec">Whether Microsoft Active Directory security services
        /// should be used instead of local authentication.</param>
        void RpcServer::Initialize(
            ProtocolSequence protSeq,
            const string &serviceName,
            bool useActDirSec)
        {
            CALL_STACK_TRACE;

            try
            {
                std::lock_guard<std::mutex> lock(singletonAccessMutex);

                _ASSERTE(uniqueObject.get() != nullptr); // cannot initialize RPC server twice
                uniqueObject.reset(new RpcServerImpl(protSeq, serviceName, useActDirSec));
            }
            catch (core::IAppException &)
            {
                throw; // just forward an exception regarding an error known to have been already handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "System error prevented RPC server initialization: "
                    << core::StdLibExt::GetDetailsFromSystemError(ex);

                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure prevented RPC server initialization: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="RpcServerImpl" /> class.
        /// </summary>
        /// <param name="protSeq">The protocol sequence to be used.</param>
        /// <param name="serviceClass">A friendly name to identify this service. Such name will be
        /// employed for both informational (will be displayed in admin tools that expose listening
        /// servers, as a description) and security purposes (the "service class" in the SPN). Thus
        /// it must be unique (in a way the generated SPN will not collide with another in the Windows
        /// Active directory), and cannot contain characters such as '/', '&lt;' or '&gt;'.</param>
        /// <param name="useActDirSec">Whether Microsoft Active Directory security services
        /// should be used instead of local authentication.</param>
        RpcServerImpl::RpcServerImpl(ProtocolSequence protSeq,
                                     const string &serviceClass,
                                     bool useActDirSec) :
            m_bindings(nullptr),
            m_protSeqName(ToString(protSeq)),
            m_state(State::NotInitialized)
        {
            CALL_STACK_TRACE;

            DWORD spnArraySize(0);
            LPWSTR *spnArray;

            try
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
                m_serviceClass = transcoder.from_bytes(serviceClass);

                // Generate a list of SPN`s using the fully qualified DNS name of the local computer:
                auto rc = DsGetSpnW(
                    useActDirSec ? DS_SPN_DN_HOST : DS_SPN_DNS_HOST,
                    m_serviceClass.c_str(),
                    nullptr, 0,
                    0, nullptr,
                    nullptr,
                    &spnArraySize,
                    &spnArray
                    );

                if (rc != ERROR_SUCCESS)
                {
                    std::ostringstream oss;
                    oss << "Could not generate SPN for RPC server - ";
                    core::WWAPI::AppendDWordErrorMessage(rc, "DsGetSpn", oss);
                    throw core::AppException<std::runtime_error>(oss.str());
                }

                _ASSERTE(spnArraySize > 0);

                // Register the SPN in the authentication service:
                auto status = RpcServerRegisterAuthInfoW(
                    (RPC_WSTR)spnArray[0],
                    RPC_C_AUTHN_GSS_KERBEROS,
                    nullptr,
                    nullptr
                );

                string spn = transcoder.to_bytes(spnArray[0]);
                ThrowIfError(status, "Could not register SPN with authentication service", spn);

                std::ostringstream oss;
                oss << "RPC server \'" << serviceClass
                    << "\' was registered with the authentication service using SPN = " << spn;

                core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);
                oss.str("");

                // Set the protocol sequence:
                status = RpcServerUseProtseqW(
                    (unsigned short *)m_protSeqName,
                    RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
                    nullptr
                );

                ThrowIfError(status, "Could not set protocol sequence for RPC server");

                // Inquire bindings:
                status = RpcServerInqBindings(&m_bindings);
                ThrowIfError(status, "Could not inquire bindings for RPC server");
                m_state = State::BindingsAcquired;
            }
            catch (core::IAppException &ex)
            {
                if(spnArraySize > 0)
                    DsFreeSpnArrayW(spnArraySize, spnArray);

                throw core::AppException<std::runtime_error>("Failed to initialize RPC server", ex);
            }
            catch (std::exception &)
            {
                if (spnArraySize > 0)
                    DsFreeSpnArrayW(spnArraySize, spnArray);

                throw;
            }

            DsFreeSpnArrayW(spnArraySize, spnArray);
        }

        /// <summary>
        /// Private implementation for <see cref="RpcServer::Start"/>.
        /// </summary>
        bool RpcServerImpl::Start(const std::vector<RPC_IF_HANDLE> &interfaces)
        {
            CALL_STACK_TRACE;

            try
            {
                if (m_state == State::BindingsAcquired)
                {
                    const int annStrMaxSize(64);
                    auto annotation = m_serviceClass.substr(0, annStrMaxSize - 1);

                    // For each interface:
                    for (auto &intfHandle : interfaces)
                    {
                        // Register the interface:
                        auto status = RpcServerRegisterIf(intfHandle, nullptr, nullptr);
                        ThrowIfError(status, "Failed to register RPC interface");

                        // if any interface has been registered, flag it...
                        m_state = State::InterfacesRegistered;

                        // Update server address information in the local endpoint-map database:
                        status = RpcEpRegisterW(
                            intfHandle,
                            m_bindings,
                            nullptr,
                            (RPC_WSTR)annotation.c_str()
                        );

                        ThrowIfError(status, "Failed to register endpoints for RPC server");
                    }

                    // Start listening requests (asynchronous call, returns immediately):
                    auto status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, 1);
                    ThrowIfError(status, "Failed to start RPC server listeners");
                    m_state = State::Listening;
                    return STATUS_OKAY;
                }
                else if (m_state == State::Listening)
                    return STATUS_FAIL; // nothing to do
                else
                {
                    _ASSERTE(false); // unsupported or unexpected state
                    return STATUS_FAIL;
                }
            }
            catch (std::exception &)
            {
                if (m_state == State::InterfacesRegistered)
                {
                    auto status = RpcServerUnregisterIf(nullptr, nullptr, 1);

                    LogIfError(status,
                        "RPC Server start request suffered a secondary failure "
                        "on attempt to unregister interfaces",
                        core::Logger::PRIO_CRITICAL);
                }

                throw;
            }
        }

        /// <summary>
        /// If the server is not already running, registers the given interfaces
        /// and starts the RPC server listeners asynchronously (does not block).
        /// </summary>
        /// <param name="interfaces">The handles for the interfaces implemented by this RPC server,
        /// as provided in the source compiled from the IDL.</param>
        /// <return>
        /// <see cref="STATUS_OKAY"/> whether successful, otherwise, <see cref="STATUS_FAIL"/>
        /// (if the server was already listening or in an unexpected state).
        /// </return>
        bool RpcServer::Start(const std::vector<RPC_IF_HANDLE> &interfaces)
        {
            CALL_STACK_TRACE;

            try
            {
                std::lock_guard<std::mutex> lock(singletonAccessMutex);

                _ASSERTE(uniqueObject.get() != nullptr); // singleton not instantiated
                return uniqueObject->Start(interfaces);
            }
            catch (core::IAppException &)
            {
                throw; // just forward an exception regarding an error known to have been already handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "System error on attempt to start running RPC server: "
                    << core::StdLibExt::GetDetailsFromSystemError(ex);

                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure on attempt to start running RPC server: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="RpcServerImpl"/> class.
        /// </summary>
        RpcServerImpl::~RpcServerImpl()
        {
            CALL_STACK_TRACE;

            RPC_STATUS status;

            switch (m_state)
            {
            case State::Listening:

                status = RpcMgmtStopServerListening(m_bindings);
                LogIfError(status,
                    "Failed to stop RPC server listeners",
                    core::Logger::PRIO_CRITICAL);

                status = RpcMgmtWaitServerListen();
                LogIfError(status,
                    "Failed to await for RPC server stop",
                    core::Logger::PRIO_CRITICAL);

            // FALLTHROUGH:
            case State::InterfacesRegistered:

                status = RpcServerUnregisterIf(nullptr, nullptr, 1);
                LogIfError(status,
                    "Failed to unregister interfaces from RPC server",
                    core::Logger::PRIO_CRITICAL);

            // FALLTHROUGH:
            case State::BindingsAcquired:

                status = RpcBindingVectorFree(&m_bindings);
                LogIfError(status,
                    "Failed to release resources for RPC server bindings",
                    core::Logger::PRIO_CRITICAL
                );
                break;

            default:
                _ASSERTE(false); // unsupported state
                break;
            }
        }

        /// <summary>
        /// Private implementation for <see cref="RpcServer::Stop"/>.
        /// </summary>
        bool RpcServerImpl::Stop()
        {
            if (m_state == State::Listening)
            {
                CALL_STACK_TRACE;

                auto status = RpcMgmtStopServerListening(m_bindings);
                ThrowIfError(status, "Failed to request RPC server stop");

                status = RpcMgmtWaitServerListen();
                ThrowIfError(status, "Failed to await for RPC server stop");

                m_state = State::InterfacesRegistered;
                return STATUS_OKAY;
            }
            else
                return STATUS_FAIL;
        }

        /// <summary>
        /// Stops the RPC server listeners, but keeps the registered interfaces and bindings.
        /// </summary>
        /// <return>
        /// <see cref="STATUS_OKAY"/> whether successful, otherwise,
        /// <see cref="STATUS_FAIL"/> (if the server was not listening).
        /// </return>
        bool RpcServer::Stop()
        {
            CALL_STACK_TRACE;

            try
            {
                std::lock_guard<std::mutex> lock(singletonAccessMutex);

                _ASSERTE(uniqueObject.get() != nullptr); // singleton not instantiated
                return uniqueObject->Stop();
            }
            catch (core::IAppException &)
            {
                throw; // just forward an exception regarding an error known to have been already handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "System error on attempt to stop RPC server: "
                    << core::StdLibExt::GetDetailsFromSystemError(ex);

                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure on attempt to stop RPC server: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Private implementation for <see cref="RpcServer::Resume"/>.
        /// </summary>
        bool RpcServerImpl::Resume()
        {
            CALL_STACK_TRACE;

            RPC_STATUS status;

            switch (m_state)
            {
            case State::BindingsAcquired:
                return STATUS_FAIL;
            
            case State::InterfacesRegistered:

                // Start listening requests (blocking call):
                status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, 1);
                ThrowIfError(status, "Failed to start RPC server listeners");
                m_state = State::Listening;

            // FALLTHROUGH:
            case State::Listening:
                return STATUS_OKAY;

            default:
                _ASSERTE(false); // unsupported state
                return STATUS_FAIL;
            }
        }

        /// <summary>
        /// Resumes the RPC server listening if previously stopped.
        /// </summary>
        /// <return>
        /// <see cref="STATUS_OKAY"/> whether successful, otherwise,
        /// <see cref="STATUS_FAIL"/> (if the server was not stopped after a previous run).
        /// </return>
        bool RpcServer::Resume()
        {
            CALL_STACK_TRACE;

            try
            {
                std::lock_guard<std::mutex> lock(singletonAccessMutex);

                _ASSERTE(uniqueObject.get() != nullptr); // singleton not instantiated
                return uniqueObject->Resume();
            }
            catch (core::IAppException &)
            {
                throw; // just forward an exception regarding an error known to have been already handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "System error on attempt to resume RPC server: "
                    << core::StdLibExt::GetDetailsFromSystemError(ex);

                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure on attempt to resume RPC server: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Private implementation for <see cref="RpcServer::Wait"/>.
        /// </summary>
        bool RpcServerImpl::Wait()
        {
            if (m_state == State::Listening)
            {
                CALL_STACK_TRACE;
                auto status = RpcMgmtWaitServerListen();
                ThrowIfError(status, "Failed to await for RPC server stop");
                return STATUS_OKAY;
            }
            else
                return STATUS_FAIL;
        }

        /// <summary>
        /// Wait until the RPC server stops listening and all requests are completed
        /// </summary>
        /// <return>
        /// <see cref="STATUS_OKAY"/> whether successful, otherwise,
        /// <see cref="STATUS_FAIL"/> (if the server was not listening).
        /// </return>
        bool RpcServer::Wait()
        {
            CALL_STACK_TRACE;

            try
            {
                std::lock_guard<std::mutex> lock(singletonAccessMutex);

                _ASSERTE(uniqueObject.get() != nullptr); // singleton not instantiated
                return uniqueObject->Wait();
            }
            catch (core::IAppException &)
            {
                throw; // just forward an exception regarding an error known to have been already handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "System error when awaiting for RPC server stop: "
                    << core::StdLibExt::GetDetailsFromSystemError(ex);

                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when awaiting for RPC server stop: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Finalizes the RPC server: stop, unregister
        /// interfaces, release bindings and other resources.
        /// </summary>
        void RpcServer::Finalize() noexcept
        {
            CALL_STACK_TRACE;

            try
            {
                std::lock_guard<std::mutex> lock(singletonAccessMutex);

                _ASSERTE(uniqueObject.get() != nullptr); // cannot finalize uninitialized RPC server
                uniqueObject.reset(nullptr);
            }
            catch (core::IAppException &)
            {
                throw; // just forward an exception regarding an error known to have been already handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "System error prevented RPC server finalization: "
                    << core::StdLibExt::GetDetailsFromSystemError(ex);

                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure prevented RPC server finalization: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

    }// end of namespace rpc
}// end of namespace _3fd
