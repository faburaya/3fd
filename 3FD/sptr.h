#ifndef SPTR_H // header guard
#define SPTR_H

#include "gc.h"
#include "gc_common.h"
#include <functional>

// A macro through which the client code constructs garbage collected objects and assigns them to a safe pointer
#define has(CTOR_CALL)    createAndAcquireGCObject<decltype(CTOR_CALL)>([&] (void *gcRegMem) { new (gcRegMem) CTOR_CALL; })

namespace _3fd
{
namespace memory
{
    /////////////////////////////////
    //  sptr_base Class Template
    /////////////////////////////////
    
    /// <summary>
    /// Base class for both <see cref="sptr" /> and <see cref="const_sptr" />.
    /// It was made a template so as to enforce compilation errors when the client code tries use it with not derived/base types.
    /// </summary>
    template <typename Type>
    class sptr_base
    {
    private:

        // Make all the sptr_base classes mutually trustable:
        template <typename OtherType> friend class sptr_base;

        /// <summary>
        /// The memory address referenced by this instance.
        /// </summary>
        Type *m_pointedAddress;

    protected:

        /// <summary>
        /// Gives the pointed address to the derived classes.
        /// </summary>
        /// <returns>The memory address held by the instance.</returns>
        Type *GetPointedAddress() const
        {
            return m_pointedAddress;
        }

        /// <summary>
        /// Default parameterless constructor.
        /// Just registers the safe pointer with the GC.
        /// </summary>
        sptr_base() : 
            m_pointedAddress(nullptr)
        {
            GarbageCollector::GetInstance()
                .RegisterSptr(this, nullptr);
        }

        /// <summary>
        /// Copy constructor.
        /// Tells the GC there is a new safe pointer referencing the same memory address.
        /// </summary>
        /// <param name="ob">The object to be copied.</param>
        sptr_base(const sptr_base &ob) :
            m_pointedAddress(ob.m_pointedAddress)
        {
            GarbageCollector::GetInstance()
                .RegisterSptrCopy(this, const_cast<sptr_base *> (&ob));
        }

        /// <summary>
        /// Copy constructor.
        /// Tells the GC there is a new safe pointer referencing the same memory address.
        /// </summary>
        /// <param name="ob">The object to be copied.</param>
        template <typename ObjectType> 
        sptr_base(const sptr_base<ObjectType> &ob) :
            m_pointedAddress(static_cast<Type *> (ob.m_pointedAddress)) // Fires a compile-time error when 'ObjectType' is not a derived/same/convertible type
        {
            GarbageCollector::GetInstance()
                .RegisterSptrCopy(this, const_cast<sptr_base<ObjectType> *> (&ob));
        }

        /// <summary>
        /// Destructor.
        /// Tells the GC that the current reference to the pointed memory address no longer exists.
        /// </summary>
        ~sptr_base()
        {
            GarbageCollector::GetInstance()
                .UnregisterSptr(this);
        }

        /// <summary>
        /// Assigns an object to the current instance.
        /// </summary>
        /// <param name="ob">The object to assign.</param>
        template <typename ObjectType> 
        void Assign(const sptr_base<ObjectType> &ob)
        {
            if (static_cast<const void *> (&ob) != static_cast<const void *> (this)
                && static_cast<const void *> (m_pointedAddress) != static_cast<const void *> (ob.m_pointedAddress))
            {
                GarbageCollector::GetInstance()
                    .UpdateReference(this, const_cast<sptr_base<ObjectType> *> (&ob));

                // Fires a compile-time error when 'ObjectType' is not a derived/same/convertible type
                m_pointedAddress = static_cast<Type *> (ob.m_pointedAddress);
            }
        }

    public:

        /// <summary>
        /// Invoked by the <see cref="has" /> macro to create and acquire a garbage collected object.
        /// </summary>
        /// <param name="invokeObjectCtor">A lambda which contructs the object when invoked.</param>
        template <typename ObjectType> 
        void createAndAcquireGCObject(const std::function<void (void *)> &invokeObjectCtor)
        {
            /* The object memory must first be registered with the GC. That is because the referred object
            might contain a member which is a safe pointer. If that is the case, the registration of this
            'child' safe pointer must be able to know it belongs to the memory region of the current instance,
            which is possible only if its memory was allocated before hand. */
            void *gcRegMem = AllocMemoryAndRegisterWithGC(sizeof (ObjectType), this, &FreeMemAddr<ObjectType>);

            try
            {
                invokeObjectCtor(gcRegMem);
            }
            catch(...) // Object construction threw an exception:
            {
                m_pointedAddress = nullptr;
                GarbageCollector::GetInstance()
                    .UnregisterAbortedObject(this);
                throw;
            }

            m_pointedAddress = static_cast<Type *> (
                static_cast<ObjectType *> (gcRegMem)
            ); // Fires a compile time error when 'ObjectType' is not a derived/same/convertible type
        }

        /// <summary>
        /// Equality operator overload.
        /// </summary>
        /// <param name="ob">The object to compare with.</param>
        /// <returns>'true' if refers to the same memory address, otherwise, 'false'.</returns>
        bool operator ==(const sptr_base &ob) const
        {
            return m_pointedAddress == ob.m_pointedAddress; // Fires a compile time error when 'ObjectType' is not a derived/same/convertible type
        }

        /// <summary>
        /// Equality operator overload.
        /// </summary>
        /// <param name="ob">The object to compare with.</param>
        /// <returns>'true' if refers to the same memory address, otherwise, 'false'.</returns>
        template <typename ObjectType> 
        bool operator ==(const sptr_base<ObjectType> &ob) const
        {
            return m_pointedAddress == static_cast<Type *> (ob.m_pointedAddress); // Fires a compile time error when 'ObjectType' is not a derived/same/convertible type
        }

        /// <summary>
        /// Inequality operator overload.
        /// </summary>
        /// <param name="ob">The object to compare with.</param>
        /// <returns>'true' if refers to a different memory address, otherwise, 'false'.</returns>
        bool operator !=(const sptr_base &ob) const
        {
            return m_pointedAddress != ob.m_pointedAddress;
        }

        /// <summary>
        /// Inequality operator overload.
        /// </summary>
        /// <param name="ob">The object to compare with.</param>
        /// <returns>'true' if refers to a different memory address, otherwise, 'false'.</returns>
        template <typename ObjectType> 
        bool operator !=(const sptr_base<ObjectType> &ob) const
        {
            return m_pointedAddress != static_cast<Type *> (ob.m_pointedAddress); // Fires a compile time error when 'ObjectType' is not a derived/same/convertible type
        }

        /// <summary>
        /// Whether the instance helds a null pointer or not.
        /// </summary>
        /// <returns>'true' if a null pointer, otherwise, 'false'</returns>
        bool Off() const
        {
            return (m_pointedAddress == nullptr);
        }

        /// <summary>
        /// Resets the held memory address to a null pointer.
        /// </summary>
        void Reset()
        {
            GarbageCollector::GetInstance().ReleaseReference(this);
            m_pointedAddress = nullptr;
        }
    };


    ///////////////////////////////////
    //  const_sptr Class Template
    ///////////////////////////////////

    /// <summary>
    /// A class for safe pointers (make use of the GC). Referred objects are constant.
    /// </summary>
    template <typename Type> 
    class const_sptr : public sptr_base<Type>
    {
    public:

        const_sptr() : sptr_base<Type>() {}

        const_sptr(const sptr_base<Type> &ob) : sptr_base<Type>(ob) {}

        template <typename ObjectType> 
        const_sptr(const sptr_base<ObjectType> &ob) : sptr_base<Type>(ob) {}

        const_sptr &operator =(const sptr_base<Type> &ob)
        {
            Assign(ob);
            return *this;
        }

        template <typename ObjectType> 
        const_sptr &operator =(const sptr_base<ObjectType> &ob)
        {
            Assign(ob);
            return *this;
        }

        template <typename ObjectType> 
        operator const_sptr<ObjectType>() const
        {
            return const_sptr<ObjectType>(*this);
        }

        const Type &operator *() const
        {
            return *this->GetPointedAddress();
        }

        const Type *operator ->() const
        {
            return this->GetPointedAddress();
        }
    };


    ///////////////////////////////////
    //  sptr Class Template
    ///////////////////////////////////

    /// <summary>
    /// A class for safe pointers (make use of the GC).
    /// </summary>
    template <typename Type> 
    class sptr : public sptr_base<Type>
    {
    public:

        sptr() : sptr_base<Type>() {}

        sptr(const sptr &ob) : sptr_base<Type>(ob) {}

        template <typename ObjectType> 
        sptr(const sptr<ObjectType> &ob) : sptr_base<Type>(ob) {}

        sptr &operator =(const sptr &ob)
        {
            this->Assign(ob);
            return *this;
        }

        template <typename ObjectType> 
        sptr &operator =(const sptr<ObjectType> &ob)
        {
            this->Assign(ob);
            return *this;
        }

        template <typename ObjectType> 
        operator sptr<ObjectType>() const
        {
            return sptr<ObjectType>(*this);
        }

        template <typename ObjectType> 
        operator const_sptr<ObjectType>() const
        {
            return const_sptr<ObjectType>(*this);
        }

        Type &operator *() const
        {
            return *this->GetPointedAddress();
        }

        Type *operator ->() const
        {
            return this->GetPointedAddress();
        }
    };

}// end of namespace memory
}// end of namespace _3fd

#endif // end of header guard
