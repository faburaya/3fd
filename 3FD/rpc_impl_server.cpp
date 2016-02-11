#include "stdafx.h"
#include "rpc_impl_server.h"
#include "rpc_impl_util.h"
#include "callstacktracer.h"
#include "logger.h"

#include <codecvt>
#include <sstream>
#include <NtDsAPI.h>

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
        /// <param name="serviceClass">A friendly name to identify this service. Such name will be
        /// employed for both informational (will be displayed in admin tools that expose listening
        /// servers, as a description) and security purposes (the "service class" in the SPN). Thus
        /// it must be unique (in a way the generated SPN will not collide with another in the Windows
        /// Active directory), and cannot contain characters such as '/', '&lt;' or '&gt;'.</param>
        /// <param name="authLevel">The authentication level required for the client.</param>
        /// <param name="useActDirSec">Whether Microsoft Active Directory security services
        /// should be used instead of local authentication.</param>
        void RpcServer::Initialize(
            ProtocolSequence protSeq,
            const string &serviceClass,
            AuthenticationLevel authLevel,
            bool useActDirSec)
        {
            CALL_STACK_TRACE;

            try
            {
                std::lock_guard<std::mutex> lock(singletonAccessMutex);

                // cannot initialize RPC server twice, unless you stop it first
                _ASSERTE(uniqueObject.get() == nullptr);

                uniqueObject.reset(
                    new RpcServerImpl(protSeq, serviceClass, authLevel, useActDirSec)
                );
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
        /// RAII for Directory Service binding handle.
        /// </summary>
        struct DirSvcBinding
        {
            HANDLE handle;

            DirSvcBinding()
                : handle(nullptr) {}

            ~DirSvcBinding()
            {
                if (handle != nullptr)
                    DsUnBindW(&handle);
            }
        };

        /// <summary>
        /// RAII for array of SPN's.
        /// </summary>
        struct ArrayOfSpn
        {
            DWORD size;
            LPWSTR *data;

            ArrayOfSpn()
                : size(0), data(nullptr) {}

            ~ArrayOfSpn()
            {
                if (data != nullptr)
                    DsFreeSpnArrayW(size, data);
            }
        };

        /// <summary>
        /// Initializes a new instance of the <see cref="RpcServerImpl" /> class.
        /// </summary>
        /// <param name="protSeq">The protocol sequence to be used.</param>
        /// <param name="serviceClass">A friendly name to identify this service. Such name will be
        /// employed for both informational (will be displayed in admin tools that expose listening
        /// servers, as a description) and security purposes (the "service class" in the SPN). Thus
        /// it must be unique (in a way the generated SPN will not collide with another in the Windows
        /// Active directory), and cannot contain characters such as '/', '&lt;' or '&gt;'.</param>
        /// <param name="authLevel">The authentication level required for the client.</param>
        /// <param name="useActDirSec">Whether Microsoft Active Directory security services
        /// should be used instead of local authentication.</param>
        RpcServerImpl::RpcServerImpl(
            ProtocolSequence protSeq,
            const string &serviceClass,
            AuthenticationLevel authLevel,
            bool useActDirSec)
        try :
            m_bindings(nullptr),
            m_authLevel(authLevel),
            m_state(State::NotInitialized)
        {
            CALL_STACK_TRACE;

            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            m_serviceClass = transcoder.from_bytes(serviceClass);

            std::wstring protSeqName = transcoder.from_bytes(ToString(protSeq));

            // Set the protocol sequence:
            auto status = RpcServerUseProtseqW(
                (unsigned short *)protSeqName.c_str(),
                RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
                nullptr
            );

            ThrowIfError(status,
                "Could not set protocol sequence for RPC server",
                ToString(protSeq));

            // Notify setting of protocol sequence:
            std::ostringstream oss;
            oss << "RPC server \'" << serviceClass
                << "\' using protocol sequence \'" << ToString(protSeq) << '\'';

            core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);
            oss.str("");

            // Inquire bindings (dynamic endpoints):
            status = RpcServerInqBindings(&m_bindings);
            ThrowIfError(status, "Could not inquire bindings for RPC server");
            m_state = State::BindingsAcquired;

            if (authLevel == AuthenticationLevel::None)
                return;

            // Generate a list of SPN`s using the fully qualified DNS name of the local computer:
            ArrayOfSpn spnArray;
            auto rc = DsGetSpnW(
                useActDirSec ? DS_SPN_DN_HOST : DS_SPN_DNS_HOST,
                m_serviceClass.c_str(),
                nullptr, 0,
                0, nullptr,
                nullptr,
                &spnArray.size,
                &spnArray.data
            );

            if (rc != ERROR_SUCCESS)
            {
                oss << "Could not generate SPN for RPC server - ";
                core::WWAPI::AppendDWordErrorMessage(rc, "DsGetSpn", oss);
                throw core::AppException<std::runtime_error>(oss.str());
            }

            _ASSERTE(spnArray.size > 0);

            if (useActDirSec)
            {
                // Bind to domain:
                DirSvcBinding dirSvcBinding;
                rc = DsBindW(nullptr, nullptr, &dirSvcBinding.handle);

                if (rc != ERROR_SUCCESS)
                {
                    oss << "Could not bind to global catalog server - ";
                    core::WWAPI::AppendDWordErrorMessage(rc, "DsBind", oss);
                    throw core::AppException<std::runtime_error>(oss.str());
                }

                // Register SPN with domain on computer account:
                rc = DsWriteAccountSpnW(
                    dirSvcBinding.handle,
                    DS_SPN_ADD_SPN_OP,
                    spnArray.data[0],
                    spnArray.size,
                    (LPCWSTR *)spnArray.data
                );
                
                if (rc != ERROR_SUCCESS)
                {
                    oss << "Could not register SPN in local computer account - ";
                    core::WWAPI::AppendDWordErrorMessage(rc, "DsWriteAccountSpn", oss);
                    string message = oss.str();
                    oss.str("");

                    oss << "List of SPN's: " << transcoder.to_bytes(spnArray.data[0]);

                    for (int idx = 1; idx < spnArray.size; ++idx)
                        oss << "; " << transcoder.to_bytes(spnArray.data[idx]);

                    throw core::AppException<std::runtime_error>(message, oss.str());
                }
            }

            // Register the SPN in the authentication service:
            status = RpcServerRegisterAuthInfoW(
                (RPC_WSTR)spnArray.data[0],
                RPC_C_AUTHN_GSS_KERBEROS,
                nullptr,
                nullptr
            );

            string spn = transcoder.to_bytes(spnArray.data[0]);
            ThrowIfError(status,
                "Could not register SPN with Kerberos authentication service", spn);

            // Notify registration in authentication service:
            oss << "RPC server \'" << serviceClass
                << "\' has been registered with Kerberos "
                   "authentication service using SPN = " << spn;

            core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);
            oss.str("");
        }
        catch (core::IAppException &ex)
        {
            throw core::AppException<std::runtime_error>("Failed to initialize RPC server", ex);
        }
        catch (std::exception &)
        {
            throw;
        }

        /// <summary>
        /// Gets the required authentication level.
        /// </summary>
        /// <returns>The required authentication level for clients,
        /// as defined upon initialization.</returns>
        AuthenticationLevel RpcServer::GetRequiredAuthLevel()
        {
            _ASSERTE(uniqueObject.get() != nullptr); // singleton not instantiated
            return uniqueObject->GetRequiredAuthLevel();
        }

        /* This callback is invoked by RPC runtime every time an RPC takes
        place. At this point, the client is already authenticated.*/
        RPC_STATUS CALLBACK callbackIntfAuthz(
            _In_ RPC_IF_HANDLE interface,
            _In_ void *context)
        {
            CALL_STACK_TRACE;

            RPC_CALL_ATTRIBUTES_V2_W callAttributes = { 0 };
            callAttributes.Version = 2;

            auto status = RpcServerInqCallAttributesW(
                static_cast<RPC_BINDING_HANDLE> (context),
                &callAttributes
            );

            if (status != RPC_S_OK)
            {
                LogIfError(status,
                    "Failed to inquire RPC attributes during authorization",
                    core::Logger::PRIO_ERROR);

                return RPC_S_ACCESS_DENIED;
            }

            // Reject call if client authentication level does not meet server requirements:
            auto requiredAuthLevel = static_cast<unsigned long> (RpcServer::GetRequiredAuthLevel());
            if (callAttributes.AuthenticationLevel < requiredAuthLevel)
                return RPC_S_ACCESS_DENIED;

            return RPC_S_OK;
        }

        /// <summary>
        /// Private implementation for <see cref="RpcServer::Start"/>.
        /// </summary>
        bool RpcServerImpl::Start(const std::vector<RpcSrvObject> &objects)
        {
            CALL_STACK_TRACE;

            RPC_STATUS status;

            try
            {
                if (m_state == State::BindingsAcquired)
                {
                    std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
                    std::map<RPC_IF_HANDLE, VectorOfUuids> objsByIntfHnd;

                    // For each object implementing a RPC interface:
                    for (auto &obj : objects)
                    {
                        // Create an UUID on the fly for the EPV:
                        UUID paramMgrTypeUuid;
                        status = UuidCreateSequential(&paramMgrTypeUuid);
                        ThrowIfError(status, "Could not generate UUID for EPV in RPC server");

                        // Register the interface with the RPC runtime lib:
                        status = RpcServerRegisterIfEx(
                            obj.interfaceHandle,
                            &paramMgrTypeUuid,
                            obj.epv,
                            0, // use default security flags
                            RPC_C_LISTEN_MAX_CALLS_DEFAULT, // ignored because the interface is not auto-listen
                            (m_authLevel != AuthenticationLevel::None) ? callbackIntfAuthz : nullptr
                        );

                        ThrowIfError(status, "Could not register interface with RPC runtime library");

                        // if any interface has been registered, flag it...
                        m_state = State::IntfRegRuntimeLib;

                        UUID paramObjUuid;
                        std::wstring ucs2ObjUuid = transcoder.from_bytes(obj.uuid);
                        status = UuidFromStringW((RPC_WSTR)ucs2ObjUuid.c_str(), &paramObjUuid);
                        ThrowIfError(status, "Could not parse UUID provided for object in RPC server", obj.uuid);

                        /* Assign the object UUID (as known by the customer)
                        to the EPV (particular interface implementation): */
                        status = RpcObjectSetType(&paramObjUuid, &paramMgrTypeUuid);
                        ThrowIfError(status, "Could not associate RPC object with EPV", obj.uuid);

                        // keep the UUID assigned to the EPV (object type UUID) for later...
                        objsByIntfHnd[obj.interfaceHandle].Add(paramObjUuid);
                    }

                    const int annStrMaxSize(64);
                    auto annotation = m_serviceClass.substr(0, annStrMaxSize - 1);

                    // For each RPC interface:
                    for (auto &pair : objsByIntfHnd)
                    {
                        auto interfaceHandle = pair.first;
                        auto &objects = pair.second;

                        UuidVectorFix objsUuidsVec;

                        /* Complete binding of dynamic endpoints by registering
                        these objects in the local endpoint-map database: */
                        status = RpcEpRegisterW(
                            interfaceHandle,
                            m_bindings,
                            objects.CopyTo(objsUuidsVec), // use the fix for UUID_VECTOR...
                            (RPC_WSTR)annotation.c_str()
                        );

                        ThrowIfError(status,
                            "Could not complete binding of dynamic endpoints for RPC server interface");

                        // if any interface has been registered, flag it...
                        m_state = State::IntfRegLocalEndptMap;

                        /* Keep track of every successful registration in the local
                        endpoint-map database. Later, for proper resource release
                        this will be needed. */
                        m_regObjsByIntfHnd[interfaceHandle] = std::move(objects);
                    }

                    objsByIntfHnd.clear(); // release some memory

                    // Start listening requests (asynchronous call, returns immediately):
                    status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE);
                    ThrowIfError(status, "Could not start RPC server listeners");
                    m_state = State::Listening;
                    return STATUS_OKAY;
                }
                else if(m_state == State::Listening)
                    return STATUS_FAIL; // nothing to do
                else
                {
                    _ASSERTE(false); // unsupported or unexpected state
                    return STATUS_FAIL;
                }
            }
            catch (core::IAppException &ex)
            {
                OnStartFailureRollbackIntfReg();
                throw core::AppException<std::runtime_error>("Failed to start RPC server", ex);
            }
            catch (std::exception &ex)
            {
                OnStartFailureRollbackIntfReg();
                std::ostringstream oss;
                oss << "Generic failure prevented starting the RPC server: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Executes the rollback of interfaces registration (in runtime
        /// library and local enpoint-map database) upon failure.
        /// </summary>
        void RpcServerImpl::OnStartFailureRollbackIntfReg() noexcept
        {
            CALL_STACK_TRACE;

            RPC_STATUS status;

            switch (m_state)
            {
            case State::IntfRegLocalEndptMap:

                for (auto &pair : m_regObjsByIntfHnd)
                {
                    auto interfaceHandle = pair.first;
                    auto &objects = pair.second;

                    UuidVectorFix objsUuidsVec;
                    status = RpcEpUnregister(
                        interfaceHandle,
                        m_bindings,
                        objects.CopyTo(objsUuidsVec) // use the fix for UUID_VECTOR...
                    );

                    LogIfError(status,
                        "RPC server start request suffered a secondary failure upon rollback of state - "
                        "Could not unregister interface from local endpoint-map database",
                        core::Logger::PRIO_CRITICAL);
                }

                m_regObjsByIntfHnd.clear();

                // FALLTROUGH
            case State::IntfRegRuntimeLib:

                status = RpcServerUnregisterIf(nullptr, nullptr, TRUE);

                LogIfError(status,
                    "RPC server start request suffered a secondary failure upon rollback of state - "
                    "Could not unregister interfaces from runtime library",
                    core::Logger::PRIO_CRITICAL);

            // FALLTROUGH
            default:
                return;
            }
        }

        /// <summary>
        /// If the server is not already running, registers the given interfaces
        /// and starts the RPC server listeners asynchronously (does not block).
        /// </summary>
        /// <param name="objects">The objects are every particular implementation
        /// of interface for this RPC server.</param>
        /// <return>
        /// <see cref="STATUS_OKAY"/> whether successful, otherwise, <see cref="STATUS_FAIL"/>
        /// (if the server was already listening or in an unexpected state).
        /// </return>
        bool RpcServer::Start(const std::vector<RpcSrvObject> &objects)
        {
            CALL_STACK_TRACE;

            try
            {
                std::lock_guard<std::mutex> lock(singletonAccessMutex);

                _ASSERTE(uniqueObject.get() != nullptr); // singleton not instantiated
                return uniqueObject->Start(objects);
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

                status = RpcMgmtStopServerListening(nullptr);
                LogIfError(status,
                    "Failed to stop RPC server listeners",
                    core::Logger::PRIO_CRITICAL);

                status = RpcMgmtWaitServerListen();
                LogIfError(status,
                    "Failed to await for RPC server stop",
                    core::Logger::PRIO_CRITICAL);

            // FALLTHROUGH
            case State::IntfRegLocalEndptMap:

                for (auto &pair : m_regObjsByIntfHnd)
                {
                    auto interfaceHandle = pair.first;
                    auto &objects = pair.second;

                    UuidVectorFix objsUuidsVec;
                    status = RpcEpUnregister(
                        interfaceHandle,
                        m_bindings,
                        objects.CopyTo(objsUuidsVec) // use the fix for UUID_VECTOR...
                    );

                    LogIfError(status,
                        "Failed to unregister interface from local endpoint-map database",
                        core::Logger::PRIO_CRITICAL);
                }

            // FALLTROUGH
            case State::IntfRegRuntimeLib:

                status = RpcServerUnregisterIf(nullptr, nullptr, TRUE);
                LogIfError(status,
                    "Failed to unregister interface from RPC server",
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

                auto status = RpcMgmtStopServerListening(nullptr);
                ThrowIfError(status, "Failed to request RPC server stop");

                status = RpcMgmtWaitServerListen();
                ThrowIfError(status, "Failed to await for RPC server stop");

                m_state = State::IntfRegLocalEndptMap;
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
            case State::IntfRegRuntimeLib:
                return STATUS_FAIL;
            
            case State::IntfRegLocalEndptMap:

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
        /// <see cref="STATUS_OKAY"/> whether successful or already running,
        /// otherwise, <see cref="STATUS_FAIL"/> (if the binding was not completed
        /// with interface registration in the local endpoint-map database).
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
        /// <return>
        /// <see cref="STATUS_OKAY"/> whether successful, otherwise,
        /// <see cref="STATUS_FAIL"/> (if the server was not initialized).
        /// </return>
        bool RpcServer::Finalize() noexcept
        {
            CALL_STACK_TRACE;

            try
            {
                std::lock_guard<std::mutex> lock(singletonAccessMutex);

                if (uniqueObject.get() != nullptr)
                {
                    uniqueObject.reset(nullptr);
                    return STATUS_OKAY;
                }
                else
                    return STATUS_FAIL;
            }
            catch (core::IAppException &ex)
            {
                core::Logger::Write(ex, core::Logger::PRIO_CRITICAL);
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "System error prevented RPC server finalization: "
                    << core::StdLibExt::GetDetailsFromSystemError(ex);

                core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL);
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure prevented RPC server finalization: " << ex.what();
                core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL);
            }
        }

    }// end of namespace rpc
}// end of namespace _3fd
