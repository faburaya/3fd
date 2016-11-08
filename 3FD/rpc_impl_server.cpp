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

    // Template for basic initialization of RPC server singleton
    void RpcServer::InitializeBaseTemplate(const std::function<void ()> &callback)
    {
        try
        {
            std::lock_guard<std::mutex> lock(singletonAccessMutex);

            // cannot initialize RPC server twice, unless you stop it first
            _ASSERTE(uniqueObject.get() == nullptr);

            callback();
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
    /// Initializes the RPC server (without authentication security)
    /// before running it.
    /// </summary>
    /// <param name="protSeq">The protocol sequence to be used.</param>
    /// <param name="serviceName">A friendly name to identify this service. Such name will
    /// be employed for informational purposes (will be displayed in management tools that 
    /// expose listening servers, as a description).</param>
    /// <param name="authnLevel">The authentication level required for the client.</param>
    void RpcServer::Initialize(ProtocolSequence protSeq, const string &serviceName)
    {
        CALL_STACK_TRACE;
        InitializeBaseTemplate([protSeq, &serviceName]()
        {
            uniqueObject.reset(
                new RpcServerImpl(protSeq, serviceName, false)
            );
        });
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="RpcServerImpl" /> class
    /// without setting any security options. This server as a basic constructor
    /// to be reused by other implementations.
    /// </summary>
    /// <param name="protSeq">The protocol sequence to be used.</param>
    /// <param name="serviceName">A friendly name to identify this service. Such name
    /// will be employed for informational purposes (will be displayed in management
    /// tools that expose listening servers, as a description).</param>
    /// <param name="hasAuthnSec">Other implementations must pass <c>false</c> when
    /// no authentication will be used, otherwise, <c>true</c>.</param>
    RpcServerImpl::RpcServerImpl(ProtocolSequence protSeq,
                                 const string &serviceName,
                                 bool hasAuthnSec)
    try :
        m_bindings(nullptr),
        m_state(State::NotInitialized),
        m_hasAuthnSec(hasAuthnSec)
    {
        CALL_STACK_TRACE;

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        m_serviceName = transcoder.from_bytes(serviceName);

        std::wstring protSeqName = transcoder.from_bytes(ToString(protSeq));

        // Set the protocol sequence:
        auto status = RpcServerUseProtseqW(
            const_cast<wchar_t *> (protSeqName.c_str()),
            RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
            nullptr
        );

        ThrowIfError(status,
            "Could not set protocol sequence for RPC server",
            ToString(protSeq));

        // Notify setting of protocol sequence:
        std::ostringstream oss;
        oss << "RPC server '" << serviceName
            << "' will use protocol sequence '" << ToString(protSeq) << '\'';

        core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);
        oss.str("");

        // Inquire bindings (dynamic endpoints):
        status = RpcServerInqBindings(&m_bindings);
        ThrowIfError(status, "Could not inquire bindings for RPC server");
        m_state = State::BindingsAcquired;
    }
    catch (core::IAppException &ex)
    {
        CALL_STACK_TRACE;
        throw core::AppException<std::runtime_error>("Failed to instantiate RPC server", ex);
    }
    catch (std::exception &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Generic failure prevented RPC server instantiation: " << ex.what();
        throw core::AppException<std::runtime_error>(oss.str());
    }

    /// <summary>
    /// Initializes the RPC server (with security options set to use
    /// Microsoft NTLM/Negotiate/Kerberos SSP) before running it.
    /// </summary>
    /// <param name="protSeq">The protocol sequence to be used.</param>
    /// <param name="serviceName">A friendly name to identify this service. Such name will
    /// be employed for informational purposes (will be displayed in management tools that 
    /// expose listening servers, as a description).</param>
    /// <param name="authnLevel">The authentication level required for the client.</param>
    void RpcServer::Initialize(ProtocolSequence protSeq,
                               const string &serviceName,
                               AuthenticationLevel authnLevel)
    {
        CALL_STACK_TRACE;
        InitializeBaseTemplate([protSeq, &serviceName, authnLevel]()
        {
            uniqueObject.reset(
                new RpcServerImpl(protSeq, serviceName, authnLevel)
            );
        });
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="RpcServerImpl" /> class
    /// with security options set to use Microsoft NTLM/Negotiate/Kerberos SSP.
    /// </summary>
    /// <param name="protSeq">The protocol sequence to be used.</param>
    /// <param name="serviceName">A friendly name to identify this service. Such name will
    /// be employed for informational purposes (will be displayed in management tools that 
    /// expose listening servers, as a description).</param>
    /// <param name="authnLevel">The authentication level required for the client.</param>
    RpcServerImpl::RpcServerImpl(ProtocolSequence protSeq,
                                 const string &serviceName,
                                 AuthenticationLevel authnLevel)
    : RpcServerImpl(protSeq, serviceName, true)
    {
        CALL_STACK_TRACE;

        try
        {
            /* Kerberos security package is preferable over NTLM because it offers
            mutual authentication, however, that requires SPN registration, which is
            only available with Microsoft Active Directory services. */

            RPC_STATUS status;
            DirSvcBinding dirSvcBinding;
            bool useActDirSec = DetectActiveDirectoryServices(dirSvcBinding, false);

            if (!useActDirSec)
            {
                // Use Microsoft NTLM SSP
                status = RpcServerRegisterAuthInfoW(nullptr, RPC_C_AUTHN_WINNT, nullptr, nullptr);
                ThrowIfError(status, "Could not set RPC server authentication to use NTLM security package");
            }
            else
            {
                // Get the default principal name for this server:
                RpcString spn;
                status = RpcServerInqDefaultPrincNameW(RPC_C_AUTHN_GSS_KERBEROS, &spn.data);
                ThrowIfError(status, "Could not get default SPN for RPC server");

                // Use Microsoft SSP Negotiate, so provide the SPN in case Kerberos is used:
                auto authnService = static_cast<unsigned long> (RPC_C_AUTHN_GSS_NEGOTIATE);
                status = RpcServerRegisterAuthInfoW(spn.data, authnService, nullptr, nullptr);

                std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
                string utf8spn = transcoder.to_bytes(spn.data);

                ThrowIfError(status,
                    "Could not register authentication information with Microsoft Negotiate SSP",
                    utf8spn);

                // Notify registration in authentication service:
                std::ostringstream oss;
                oss << "RPC server '" << serviceName
                    << "' has been registered with  "
                    << ConvertAuthnSvcOptToString(authnService)
                    << " [SPN = " << utf8spn << "]";

                core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);
            }// end if using AD

            m_cliReqAuthnLevel = authnLevel;
        }
        catch (core::IAppException &ex)
        {
            throw core::AppException<std::runtime_error>("Failed to instantiate RPC server", ex);
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure prevented RPC server instantiation: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

    /// <summary>
    /// Initializes the RPC server (with security options
    /// set to use Schannel SSP) before running it.
    /// </summary>
    /// <param name="serviceName">A friendly name to identify this service. Such name will
    /// be employed for informational purposes (will be displayed in management tools that
    /// expose listening servers, as a description).</param>
    /// <param name="authnLevel">The authentication level required for the client.</param>
    /// <param name="certInfoX509">Description of the server side X.509 certificate
    /// to use, or <c>nullptr</c> for a default certificate.</param>
    /// <remarks>Because Schannel SSP is only compatible with transport
    /// over TCP/IP, that is the implicitly chosen protocol sequence.</remarks>
    void RpcServer::Initialize(const string &serviceName,
                               const CertInfo *certInfoX509,
                               AuthenticationLevel authnLevel)
    {
        CALL_STACK_TRACE;
        InitializeBaseTemplate([&serviceName, &certInfoX509, authnLevel]()
        {
            uniqueObject.reset(
                new RpcServerImpl(serviceName, certInfoX509, authnLevel)
            );
        });
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="RpcServerImpl" /> class
    /// with security options set to use Schannel SSP.
    /// </summary>
    /// <param name="serviceName">A friendly name to identify this service. Such name will
    /// be employed for informational purposes (will be displayed in management tools that 
    /// expose listening servers, as a description).</param>
    /// <param name="authnLevel">The authentication level required for the client.</param>
    /// <param name="certInfoX509">Description of the server side X.509 certificate
    /// to use, or <c>nullptr</c> for a default certificate.</param>
    /// <remarks>Because Schannel SSP is only compatible with transport
    /// over TCP/IP, that is the implicitly chosen protocol sequence.</remarks>
    RpcServerImpl::RpcServerImpl(const string &serviceName,
                                 const CertInfo *certInfoX509,
                                 AuthenticationLevel authnLevel)
    : RpcServerImpl(ProtocolSequence::TCP, serviceName, true)
    {
        CALL_STACK_TRACE;

        try
        {
            if (certInfoX509 != nullptr)
            {
                m_sysCertStore.reset(
                    new SystemCertificateStore(certInfoX509->storeLocation, certInfoX509->storeName)
                );

                auto certX509 = m_sysCertStore->FindCertBySubject(certInfoX509->subject);

                if (certX509 == nullptr)
                {
                    std::ostringstream oss;
                    oss << "Could not get from system store code "
                        << certInfoX509->storeLocation
                        << " named '" << certInfoX509->storeName
                        << "' the specified X.509 certificate (subject = '"
                        << certInfoX509->subject << "')";

                    throw core::AppException<std::runtime_error>(
                        "Certificate for RPC server was not found in store", oss.str()
                        );
                }

                m_schannelCred.reset(
                    new SChannelCredWrapper(certX509, certInfoX509->strongerSecurity)
                );
            }

            auto authnService = static_cast<unsigned long> (AuthenticationSecurity::SecureChannel);

            auto status = RpcServerRegisterAuthInfoW(
                nullptr,
                authnService,
                nullptr,
                m_schannelCred ? m_schannelCred->GetCredential() : nullptr
            );

            ThrowIfError(status,
                "Could not register authentication information with Schannel SSP");

            std::ostringstream oss;
            oss << "RPC server " << serviceName
                << "has been registered with "
                << ConvertAuthnSvcOptToString(authnService);

            if (certInfoX509 != nullptr)
            {
                oss << " and X.509 certificate (subject = '" << certInfoX509->subject
                    << "' in store '" << certInfoX509->storeName << "')";
            }
            else
                oss << " and a default X.509 certificate";

            core::Logger::Write(oss.str(), core::Logger::PRIO_NOTICE);

            m_cliReqAuthnLevel = authnLevel;
        }
        catch (core::IAppException &ex)
        {
            throw core::AppException<std::runtime_error>("Failed to instantiate RPC server", ex);
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure prevented RPC server instantiation: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

    /// <summary>
    /// Gets the required authentication level.
    /// </summary>
    /// <returns>The required authentication level for clients,
    /// as defined upon initialization.</returns>
    AuthenticationLevel RpcServer::GetRequiredAuthnLevel()
    {
        _ASSERTE(uniqueObject.get() != nullptr); // singleton not instantiated
        return uniqueObject->GetRequiredAuthnLevel();
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
        auto requiredAuthLevel = static_cast<unsigned long> (RpcServer::GetRequiredAuthnLevel());
        if (callAttributes.AuthenticationLevel < requiredAuthLevel)
        {
            std::ostringstream oss;
            oss << "Authenticated RPC client PID = " << callAttributes.ClientPID;

            switch (callAttributes.IsClientLocal)
            {
            case rcclInvalid:
                break;

            case rcclLocal:
                oss << " (local)";
                break;

            case rcclRemote:
                oss << " (remote)";
                break;

            case rcclClientUnknownLocality:
                oss << " (unknown locality)";
                break;

            default:
                oss << " (unrecognized locality)";
                break;
            }

            oss << " was denied access due to insufficient authentication level";
            core::Logger::Write(oss.str(), core::Logger::PRIO_INFORMATION);
            return RPC_S_ACCESS_DENIED;
        }

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
            core::Logger::Write("Starting RPC server...", core::Logger::PRIO_NOTICE);

            if (m_state == State::BindingsAcquired)
            {
                std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
                std::ostringstream oss;

                std::map<RPC_IF_HANDLE, VectorOfUuids> objsByIntfHnd;

                // For each object implementing a RPC interface:
                for (auto &obj : objects)
                {
                    oss << "Registering RPC server object " << obj.uuid << "... ";

                    core::ScopedLogWrite scope(
                        oss.str(),
                        core::Logger::PRIO_INFORMATION, "done",
                        core::Logger::PRIO_ERROR, "failed"
                    );

                    oss.str("");

                    // Create an UUID on the fly for the EPV:
                    UUID paramMgrTypeUuid;
                    status = UuidCreateSequential(&paramMgrTypeUuid);
                    ThrowIfError(status, "Could not generate UUID for EPV of RPC server");

                    // Register the interface with the RPC runtime lib:
                    status = RpcServerRegisterIfEx(
                        obj.interfaceHandle,
                        &paramMgrTypeUuid,
                        obj.epv,
                        m_schannelCred ? RPC_IF_ALLOW_CALLBACKS_WITH_NO_AUTH : 0,
                        RPC_C_LISTEN_MAX_CALLS_DEFAULT, // ignored because the interface is not auto-listen
                        m_hasAuthnSec ? callbackIntfAuthz : nullptr
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

                    scope.LogSuccess();
                }

                const int annStrMaxSize(64);
                auto annotation = m_serviceName.substr(0, annStrMaxSize - 1);

                int intfCount(0);

                // For each RPC interface:
                for (auto &pair : objsByIntfHnd)
                {
                    auto interfaceHandle = pair.first;
                    auto &objects = pair.second;

                    oss << "Registering RPC interface " << ++intfCount
                        << " with " << objects.Size() << " objects... ";

                    core::ScopedLogWrite scope(
                        oss.str(),
                        core::Logger::PRIO_INFORMATION, "done",
                        core::Logger::PRIO_ERROR, "failed"
                    );

                    oss.str("");

                    UuidVectorFix objsUuidsVec;

                    /* Complete binding of dynamic endpoints by registering
                    these objects in the local endpoint-map database: */
                    status = RpcEpRegisterW(
                        interfaceHandle,
                        m_bindings,
                        objects.CopyTo(objsUuidsVec), // use the fix for UUID_VECTOR...
                        const_cast<RPC_WSTR> (annotation.c_str())
                    );

                    ThrowIfError(status,
                        "Could not complete binding of dynamic endpoints for RPC server interface");

                    // if any interface has been registered, flag it...
                    m_state = State::IntfRegLocalEndptMap;

                    /* Keep track of every successful registration in the local
                    endpoint-map database. Later, for proper resource release
                    this will be needed. */
                    m_regObjsByIntfHnd[interfaceHandle] = std::move(objects);

                    scope.LogSuccess();
                }

                objsByIntfHnd.clear(); // release some memory

                // Start listening requests (asynchronous call, returns immediately):
                status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE);
                ThrowIfError(status, "Could not start RPC server listeners");

                core::Logger::Write("RPC server is listening", core::Logger::PRIO_NOTICE);

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

        core::Logger::Write(
            "RPC server will rollback its state to after initialization",
            core::Logger::PRIO_INFORMATION);

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

        core::Logger::Write("Shutting down RPC server...", core::Logger::PRIO_NOTICE);

        RPC_STATUS status;

        switch (m_state)
        {
        case State::Listening:

            status = RpcMgmtStopServerListening(nullptr);

            // Still listening? (there was no stop request from a client)
            if (status != RPC_S_NOT_LISTENING)
            {
                LogIfError(status,
                    "Failed to stop RPC server listeners",
                    core::Logger::PRIO_CRITICAL);

                status = RpcMgmtWaitServerListen();
                LogIfError(status,
                    "Failed to await for RPC server stop",
                    core::Logger::PRIO_CRITICAL);
            }

        // FALLTHROUGH:
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

        // FALLTROUGH:
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
                core::Logger::PRIO_CRITICAL);

            break;

        default:
            _ASSERTE(false); // unsupported state
            break;
        }

        // First release resources for the certificate, than the store:
        m_schannelCred.release();
        m_sysCertStore.release();

        core::Logger::Write("RPC server was successfully shut down", core::Logger::PRIO_NOTICE);
    }

    /// <summary>
    /// Private implementation for <see cref="RpcServer::Stop"/>.
    /// </summary>
    bool RpcServerImpl::Stop()
    {
        if (m_state != State::Listening)
            return STATUS_FAIL;

        CALL_STACK_TRACE;

        core::ScopedLogWrite scope(
            "Stopping RPC server... ",
            core::Logger::PRIO_NOTICE, "done",
            core::Logger::PRIO_ERROR, "failed"
        );

        auto status = RpcMgmtStopServerListening(nullptr);

        // Still listening? (there was no stop request from a client)
        if (status != RPC_S_NOT_LISTENING)
        {
            ThrowIfError(status, "Failed to request RPC server stop");

            status = RpcMgmtWaitServerListen();
            ThrowIfError(status, "Failed to await for RPC server stop");
        }

        scope.LogSuccess();

        m_state = State::IntfRegLocalEndptMap;
        return STATUS_OKAY;
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

            core::Logger::Write("RPC server is listening", core::Logger::PRIO_NOTICE);

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

            core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL, true);
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure prevented RPC server finalization: " << ex.what();
            core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL, true);
        }
    }

}// end of namespace rpc
}// end of namespace _3fd
