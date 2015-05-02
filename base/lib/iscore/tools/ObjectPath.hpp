#pragma once
#include <iscore/tools/ObjectIdentifier.hpp>
#include <QVector>
#include <QPointer>

/**
 * @brief The ObjectPath class
 *
 * A path in the QObject hierarchy, using names and identifiers.
 * See @c{NamedObject} and @c{IdentifiedObject}.
 *
 * If the object id is not known at compile time a path may be created using ObjectPath::pathFromObject.
 *
 * Example:
 * @code
 *	ObjectPath p{ {"MyObject", {}}, {"SomeSubObjectInACollection", 27} };
 * @endcode
*/
class ObjectPath
{
        friend Serializer<DataStream>;
        friend Serializer<JSONObject>;
        friend Deserializer<DataStream>;
        friend Deserializer<JSONObject>;

        friend ObjectIdentifierVector::iterator begin(ObjectPath& path)
        {
            return path.m_objectIdentifiers.begin();
        }

        friend ObjectIdentifierVector::iterator end(ObjectPath& path)
        {
            return path.m_objectIdentifiers.end();
        }

        friend bool operator== (const ObjectPath& lhs, const ObjectPath& rhs)
        {
            return lhs.m_objectIdentifiers == rhs.m_objectIdentifiers;
        }

    public:
        ObjectPath() = default;
        QString toString() const;

        ObjectPath(QVector<ObjectIdentifier>&& vec) :
            m_objectIdentifiers {std::move(vec)}
        {
        }

        ObjectPath(std::initializer_list<ObjectIdentifier> lst) :
            m_objectIdentifiers {lst}
        {
        }

        ObjectPath(const ObjectPath& obj) = default;
        ObjectPath(ObjectPath&&) = default;
        ObjectPath& operator= (ObjectPath &&) = default;
        ObjectPath& operator= (const ObjectPath&) = default;

        /**
         * @brief pathFromObject Factory method for ObjectPath
         * @param origin Name of the object from which the search is to be started.
         * @param obj Pointer of the object to find.
         *
         * @return An object path allowing to find again "obj" in the software object hierarchy
         * (that can be serialized to text, and does not use pointers).
         */
        static ObjectPath pathFromObject(QString origin, QObject* obj);

        static ObjectPath pathBetweenObjects(const QObject* const parent_obj,
                                             const QObject* target_object);
        static ObjectPath pathFromObject(QObject* origin_object);

        /**
         * @brief find the object described by the ObjectPath
         * @return A pointer to the object, or nullptr if it is not found
         *
         * This search starts from qApp.
         * @todo (maybe) a way to specify custom ways of finding an object
         * (for instance if obj->blurb() == Ding::someDing)
         * @todo search starting from another object, for more performance.
         */
        template<typename T>
        T* find() const
        {
            // First see if the pointer is still loaded in the cache.
            if(!m_cache.isNull())
            {
                auto ptr = dynamic_cast<T*>(m_cache.data());
                Q_ASSERT(ptr);
                return ptr;
            }
            else // Load it by hand
            {
                auto ptr = dynamic_cast<T*>(find_impl());
                Q_ASSERT(ptr);
                m_cache = const_cast<typename std::remove_const<T>::type*>(ptr);
                return ptr;
            }
        }

        const ObjectIdentifierVector& vec() const
        {
            return m_objectIdentifiers;
        }

    private:
        QObject* find_impl() const;
        ObjectIdentifierVector m_objectIdentifiers;
        mutable QPointer<QObject> m_cache;
};

Q_DECLARE_METATYPE(ObjectPath)
