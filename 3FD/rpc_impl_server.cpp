#include "stdafx.h"
#include "rpc_impl_server.h"
#include "rpc_impl_util.h"
#include "callstacktracer.h"
#include "logger.h"

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
        void RpcServer::Initialize(ProtocolSequence protSeq)
        {
            CALL_STACK_TRACE;

            try
            {
                std::lock_guard<std::mutex> lock(singletonAccessMutex);

                _ASSERTE(uniqueObject.get() != nullptr); // cannot initialize RPC server twice
                uniqueObject.reset(new RpcServerImpl(protSeq));
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
        RpcServerImpl::RpcServerImpl(ProtocolSequence protSeq) :
            m_bindings(nullptr),
            m_protSeqName(ToString(protSeq)),
            m_state(State::Instantiated)
        {
        }

        /// <summary>
        /// Private implementation for <see cref="RpcServer::Start"/>.
        /// </summary>
        bool RpcServerImpl::Start(const std::vector<RPC_IF_HANDLE> &interfaces,
                                  const string &description)
        {
            CALL_STACK_TRACE;

            try
            {
                RPC_STATUS status;

                switch (m_state)
                {
                case State::Instantiated:

                    // Set the protocol sequence:
                    status = RpcServerUseProtseqW(
                        (unsigned short *)m_protSeqName,
                        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
                        nullptr
                    );

                    ThrowIfError(status, "Failed to set protocol sequence for RPC server");

                    // Inquire bindings:
                    status = RpcServerInqBindings(&m_bindings);
                    ThrowIfError(status, "Failed to inquire bindings for RPC server");
                    m_state = State::BindingsAcquired;

                    // FALLTHROUGH:
                    case State::BindingsAcquired:
                    {
                        // Generate the annotation according the API doc:
                        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
                        const int annStrMaxSize(64);
                        std::wstring annotation =
                            transcoder.from_bytes(description).substr(0, annStrMaxSize - 1);

                        // For each interface:
                        for (auto &intfHandle : interfaces)
                        {
                            // Register the interface:
                            status = RpcServerRegisterIf(intfHandle, nullptr, nullptr);
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
                    }

                    // Start listening requests (asynchronous call, returns immediately):
                    status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, 1);
                    ThrowIfError(status, "Failed to start RPC server listeners");
                    m_state = State::Listening;
                    return STATUS_OKAY;

                case State::Listening:
                    return STATUS_FAIL; // nothing to do

                default:
                    _ASSERTE(false); // unsupported state
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
                        core::Logger::PRIO_CRITICAL
                    );
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
        /// <param name="description">A description for this RPC server instance,
        /// just for informational purposes.</param>
        bool RpcServer::Start(const std::vector<RPC_IF_HANDLE> &interfaces,
                              const string &description)
        {
            CALL_STACK_TRACE;

            try
            {
                std::lock_guard<std::mutex> lock(singletonAccessMutex);

                _ASSERTE(uniqueObject.get() != nullptr); // singleton not instantiated
                return uniqueObject->Start(interfaces, description);
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
            if (m_state == State::Instantiated)
                return;

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
                    core::Logger::PRIO_CRITICAL);

            // FALLTHROUGH:
            case State::Instantiated:
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
            case State::Instantiated:
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
