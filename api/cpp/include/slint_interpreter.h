// Copyright © SixtyFPS GmbH <info@slint-ui.com>
// SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Slint-commercial

#pragma once

#include "slint.h"

#include "slint_interpreter_internal.h"

#include <optional>

#if !defined(DOXYGEN)
#    define SLINT_QT_INTEGRATION // In the future, should be defined by cmake only if this is
                                 // enabled
#endif
#ifdef SLINT_QT_INTEGRATION
class QWidget;
#endif

namespace slint::cbindgen_private {
//  This has to stay opaque, but VRc don't compile if it is just forward declared
struct ErasedComponentBox : vtable::Dyn
{
    ~ErasedComponentBox() = delete;
    ErasedComponentBox() = delete;
    ErasedComponentBox(ErasedComponentBox &) = delete;
};
}

/// The types in this namespace allow you to load a .slint file at runtime and show its UI.
///
/// You only need to use them if you do not want to use pre-compiled .slint code, which is
/// the normal way to use Slint.
///
/// The entry point for this namespace is the \ref ComponentCompiler, which you can
/// use to create \ref ComponentDefinition instances with the
/// \ref ComponentCompiler::build_from_source() or \ref ComponentCompiler::build_from_path()
/// functions.
namespace slint::interpreter {

class Value;

/// This type represents a runtime instance of structure in `.slint`.
///
/// This can either be an instance of a name structure introduced
/// with the `struct` keyword in the .slint file, or an anonymous struct
/// written with the `{ key: value, }`  notation.
///
/// It can be constructed with the range constructor or initializer lst,
/// and converted into or from a Value with the Value constructor and
/// Value::to_struct().
struct Struct
{
public:
    /// Constructs a new empty struct. You can add fields with set_field() and
    /// read them with get_field().
    Struct() { cbindgen_private::slint_interpreter_struct_new(&inner); }

    /// Creates a new Struct as a copy from \a other. All fields are copied as well.
    Struct(const Struct &other)
    {
        cbindgen_private::slint_interpreter_struct_clone(&other.inner, &inner);
    }
    /// Creates a new Struct by moving all fields from \a other into this struct.
    Struct(Struct &&other)
    {
        inner = other.inner;
        cbindgen_private::slint_interpreter_struct_new(&other.inner);
    }
    /// Assigns all the fields of \a other to this struct.
    Struct &operator=(const Struct &other)
    {
        if (this == &other)
            return *this;
        cbindgen_private::slint_interpreter_struct_destructor(&inner);
        slint_interpreter_struct_clone(&other.inner, &inner);
        return *this;
    }
    /// Moves all the fields of \a other to this struct.
    Struct &operator=(Struct &&other)
    {
        if (this == &other)
            return *this;
        cbindgen_private::slint_interpreter_struct_destructor(&inner);
        inner = other.inner;
        cbindgen_private::slint_interpreter_struct_new(&other.inner);
        return *this;
    }
    /// Destroys this struct.
    ~Struct() { cbindgen_private::slint_interpreter_struct_destructor(&inner); }

    /// Creates a new struct with the fields of the std::initializer_list given by args.
    inline Struct(std::initializer_list<std::pair<std::string_view, Value>> args);

    /// Creates a new struct with the fields produced by the iterator \a it. \a it is
    /// advanced until it equals \a end.
    template<typename InputIterator
// Doxygen doesn't understand this template wizardry
#if !defined(DOXYGEN)
             ,
             typename std::enable_if_t<
                     std::is_convertible<decltype(std::get<0>(*std::declval<InputIterator>())),
                                         std::string_view>::value
                     && std::is_convertible<decltype(std::get<1>(*std::declval<InputIterator>())),
                                            Value>::value

                     > * = nullptr
#endif
             >
    Struct(InputIterator it, InputIterator end) : Struct()
    {
        for (; it != end; ++it) {
            auto [key, value] = *it;
            set_field(key, value);
        }
    }

    // FIXME: this probably miss a lot of iterator api
    /// The Struct::iterator class implements the typical C++ iterator protocol and conveniently
    /// provides access to the field names and values of a Struct. It is created by calling either
    /// Struct::begin() or Struct::end().
    ///
    /// Make sure to compare the iterator to the iterator returned by Struct::end() before
    /// de-referencing it. The value returned when de-referencing is a std::pair that holds a
    /// std::string_view of the field name as well as a const reference of the value. Both
    /// references become invalid when the iterator or the Struct is changed, so make sure to make
    /// copies if you want to retain the name or value.
    ///
    /// Note that the order in which the iterator exposes the fields is not defined.
    ///
    /// If you're using C++ 17, you can use the convenience destructuring syntax to extract the name
    /// and value in one go:
    ///
    /// ```
    /// Struct stru = ...;
    /// auto it = stru.begin();
    /// ...
    /// ++it; // advance iterator to the next field
    /// ...
    /// // Check iterator before dereferencing it
    /// if (it != stru.end()) {
    ///     // Extract a view of the name and a const reference to the value in one go.
    ///     auto [field_name, field_value] = *it;
    /// }
    /// ```
    struct iterator
    {
        /// A typedef for std::pair<std::string_view, const Value &> that's returned
        /// when dereferencing the iterator.
        using value_type = std::pair<std::string_view, const Value &>;

    private:
        cbindgen_private::StructIteratorOpaque inner;
        const Value *v = nullptr;
        std::string_view k;
        friend Struct;
        explicit iterator(cbindgen_private::StructIteratorOpaque inner) : inner(inner) { next(); }
        // construct a end iterator
        iterator() = default;
        void next()
        {
            auto next = cbindgen_private::slint_interpreter_struct_iterator_next(&inner);
            v = reinterpret_cast<const Value *>(next.v);
            k = std::string_view(reinterpret_cast<char *>(next.k.ptr), next.k.len);
            if (!v) {
                cbindgen_private::slint_interpreter_struct_iterator_destructor(&inner);
            }
        }

    public:
        /// Destroys this field iterator.
        ~iterator()
        {
            if (v) {
                cbindgen_private::slint_interpreter_struct_iterator_destructor(&inner);
            }
        }
        // FIXME I believe iterators are supposed to be copy constructible
        iterator(const iterator &) = delete;
        iterator &operator=(const iterator &) = delete;
        /// Move-constructs a new iterator from \a other.
        iterator(iterator &&other) = default;
        /// Move-assigns the iterator \a other to this and returns a reference to this.
        iterator &operator=(iterator &&other) = default;
        /// The prefix ++ operator advances the iterator to the next entry and returns
        /// a reference to this.
        iterator &operator++()
        {
            if (v)
                next();
            return *this;
        }
        /// Dereferences the iterator to return a pair of the key and value.
        value_type operator*() const { return { k, *v }; }
        /// Returns true if \a a is pointing to the same entry as \a b; false otherwise.
        friend bool operator==(const iterator &a, const iterator &b) { return a.v == b.v; }
        /// Returns false if \a a is pointing to the same entry as \a b; true otherwise.
        friend bool operator!=(const iterator &a, const iterator &b) { return a.v != b.v; }
    };

    /// Returns an iterator over the fields of the struct.
    iterator begin() const
    {
        return iterator(cbindgen_private::slint_interpreter_struct_make_iter(&inner));
    }
    /// Returns an iterator that when compared with an iterator returned by begin() can be
    /// used to detect when all fields have been visited.
    iterator end() const { return iterator(); }

    /// Returns the value of the field with the given \a name; Returns an std::optional without
    /// value if the field does not exist.
    inline std::optional<Value> get_field(std::string_view name) const;
    /// Sets the value of the field with the given \a name to the specified \a value. If the field
    /// does not exist yet, it is created; otherwise the existing field is updated to hold the new
    /// value.
    inline void set_field(std::string_view name, const Value &value);

    /// \private
    Struct(const slint::cbindgen_private::StructOpaque &other)
    {
        cbindgen_private::slint_interpreter_struct_clone(&other, &inner);
    }

private:
    using StructOpaque = slint::cbindgen_private::StructOpaque;
    StructOpaque inner;
    friend class Value;
};

/// This is a dynamically typed value used in the Slint interpreter.
/// It can hold a value of different types, and you should use the
/// different overloaded constructors and the to_xxx() functions to access the
//// value within.
///
/// It is also possible to query the type the value holds by calling the Value::type()
/// function.
///
/// Note that models are only represented in one direction: You can create a slint::Model<Value>
/// in C++, store it in a std::shared_ptr and construct Value from it. Then you can set it on a
/// property in your .slint code that was declared to be either an array (`property <[sometype]>
/// foo;`) or an object literal (`property <{foo: string, bar: int}> my_prop;`). Such properties are
/// dynamic and accept models implemented in C++.
///
/// ```
/// Value v(42.0); // Creates a value that holds a double with the value 42.
///
/// Value some_value = ...;
/// // Check if the value has a string
/// if (std::optional<slint::SharedString> string_value = some_value.to_string())
///     do_something(*string_value);  // Extract the string by de-referencing
/// ```
class Value
{
public:
    /// Constructs a new value of type Value::Type::Void.
    Value() { cbindgen_private::slint_interpreter_value_new(&inner); }

    /// Constructs a new value by copying \a other.
    Value(const Value &other) { slint_interpreter_value_clone(&other.inner, &inner); }
    /// Constructs a new value by moving \a other to this.
    Value(Value &&other)
    {
        inner = other.inner;
        cbindgen_private::slint_interpreter_value_new(&other.inner);
    }
    /// Assigns the value \a other to this.
    Value &operator=(const Value &other)
    {
        if (this == &other)
            return *this;
        cbindgen_private::slint_interpreter_value_destructor(&inner);
        slint_interpreter_value_clone(&other.inner, &inner);
        return *this;
    }
    /// Moves the value \a other to this.
    Value &operator=(Value &&other)
    {
        if (this == &other)
            return *this;
        cbindgen_private::slint_interpreter_value_destructor(&inner);
        inner = other.inner;
        cbindgen_private::slint_interpreter_value_new(&other.inner);
        return *this;
    }
    /// Destroys the value.
    ~Value() { cbindgen_private::slint_interpreter_value_destructor(&inner); }

    /// A convenience alias for the value type enum.
    using Type = ValueType;

    // optional<int> to_int() const;
    // optional<float> to_float() const;
    /// Returns a std::optional that contains a double if the type of this Value is
    /// Type::Double, otherwise an empty optional is returned.
    std::optional<double> to_number() const
    {
        if (auto *number = cbindgen_private::slint_interpreter_value_to_number(&inner)) {
            return *number;
        } else {
            return {};
        }
    }

    /// Returns a std::optional that contains a string if the type of this Value is
    /// Type::String, otherwise an empty optional is returned.
    std::optional<slint::SharedString> to_string() const
    {
        if (auto *str = cbindgen_private::slint_interpreter_value_to_string(&inner)) {
            return *str;
        } else {
            return {};
        }
    }

    /// Returns a std::optional that contains a bool if the type of this Value is
    /// Type::Bool, otherwise an empty optional is returned.
    std::optional<bool> to_bool() const
    {
        if (auto *b = cbindgen_private::slint_interpreter_value_to_bool(&inner)) {
            return *b;
        } else {
            return {};
        }
    }

    /// Returns a std::optional that contains a vector of values if the type of this Value is
    /// Type::Model, otherwise an empty optional is returned.
    ///
    /// The vector will be constructed by serializing all the elements of the model.
    inline std::optional<slint::SharedVector<Value>> to_array() const;

    /// Returns a std::optional that contains a brush if the type of this Value is
    /// Type::Brush, otherwise an empty optional is returned.
    std::optional<slint::Brush> to_brush() const
    {
        if (auto *brush = cbindgen_private::slint_interpreter_value_to_brush(&inner)) {
            return *brush;
        } else {
            return {};
        }
    }

    /// Returns a std::optional that contains a Struct if the type of this Value is
    /// Type::Struct, otherwise an empty optional is returned.
    std::optional<Struct> to_struct() const
    {
        if (auto *opaque_struct = cbindgen_private::slint_interpreter_value_to_struct(&inner)) {
            return Struct(*opaque_struct);
        } else {
            return {};
        }
    }

    /// Returns a std::optional that contains an Image if the type of this Value is
    /// Type::Image, otherwise an empty optional is returned.
    std::optional<Image> to_image() const
    {
        if (auto *img = cbindgen_private::slint_interpreter_value_to_image(&inner)) {
            return *reinterpret_cast<const Image *>(img);
        } else {
            return {};
        }
    }

    // template<typename T> std::optional<T> get() const;

    /// Constructs a new Value that holds the double \a value.
    Value(double value) { cbindgen_private::slint_interpreter_value_new_double(value, &inner); }
    /// Constructs a new Value that holds the int \a value.
    /// Internally this is stored as a double and Value::type() will return Value::Type::Number.
    Value(int value) : Value(static_cast<double>(value)) { }
    /// Constructs a new Value that holds the string \a str.
    Value(const SharedString &str)
    {
        cbindgen_private::slint_interpreter_value_new_string(&str, &inner);
    }
    /// Constructs a new Value that holds the boolean \a b.
    Value(bool b) { cbindgen_private::slint_interpreter_value_new_bool(b, &inner); }
    /// Constructs a new Value that holds the value vector \a v as a model.
    inline Value(const SharedVector<Value> &v);
    /// Constructs a new Value that holds the value model \a m.
    Value(const std::shared_ptr<slint::Model<Value>> &m);
    /// Constructs a new Value that holds the brush \a b.
    Value(const slint::Brush &brush)
    {
        cbindgen_private::slint_interpreter_value_new_brush(&brush, &inner);
    }
    /// Constructs a new Value that holds the Struct \a struc.
    Value(const Struct &struc)
    {
        cbindgen_private::slint_interpreter_value_new_struct(&struc.inner, &inner);
    }

    /// Constructs a new Value that holds the Image \a img.
    Value(const Image &img) { cbindgen_private::slint_interpreter_value_new_image(&img, &inner); }

    /// Returns the type the variant holds.
    Type type() const { return cbindgen_private::slint_interpreter_value_type(&inner); }

    /// Returns true if \a and \b hold values of the same type and the underlying vales are equal.
    friend bool operator==(const Value &a, const Value &b)
    {
        return cbindgen_private::slint_interpreter_value_eq(&a.inner, &b.inner);
    }

private:
    inline Value(const void *) = delete; // Avoid that for example Value("foo") turns to Value(bool)
    using ValueOpaque = slint::cbindgen_private::ValueOpaque;
    ValueOpaque inner;
    friend struct Struct;
    friend class ComponentInstance;
    // Internal constructor that takes ownership of the value
    explicit Value(ValueOpaque &inner) : inner(inner) { }
};

inline Value::Value(const slint::SharedVector<Value> &array)
{
    cbindgen_private::slint_interpreter_value_new_array_model(
            reinterpret_cast<const slint::SharedVector<ValueOpaque> *>(&array), &inner);
}

inline std::optional<slint::SharedVector<Value>> Value::to_array() const
{
    slint::SharedVector<Value> array;
    if (cbindgen_private::slint_interpreter_value_to_array(
                &inner, reinterpret_cast<slint::SharedVector<ValueOpaque> *>(&array))) {
        return array;
    } else {
        return {};
    }
}
inline Value::Value(const std::shared_ptr<slint::Model<Value>> &model)
{
    using cbindgen_private::ModelAdaptorVTable;
    using vtable::VRef;
    struct ModelWrapper : private_api::ModelChangeListener
    {
        std::shared_ptr<slint::Model<Value>> model;
        cbindgen_private::ModelNotifyOpaque notify;
        // This kind of mean that the rust code has ownership of "this" until the drop function is
        // called
        std::shared_ptr<ModelChangeListener> self;
        ~ModelWrapper() { cbindgen_private::slint_interpreter_model_notify_destructor(&notify); }

        void row_added(int index, int count) override
        {
            cbindgen_private::slint_interpreter_model_notify_row_added(&notify, index, count);
        }
        void row_changed(int index) override
        {
            cbindgen_private::slint_interpreter_model_notify_row_changed(&notify, index);
        }
        void row_removed(int index, int count) override
        {
            cbindgen_private::slint_interpreter_model_notify_row_removed(&notify, index, count);
        }
        void reset() override { cbindgen_private::slint_interpreter_model_notify_reset(&notify); }
    };

    auto wrapper = std::make_shared<ModelWrapper>();
    wrapper->model = model;
    wrapper->self = wrapper;
    cbindgen_private::slint_interpreter_model_notify_new(&wrapper->notify);
    model->attach_peer(wrapper);

    auto row_count = [](VRef<ModelAdaptorVTable> self) -> uintptr_t {
        return reinterpret_cast<ModelWrapper *>(self.instance)->model->row_count();
    };
    auto row_data = [](VRef<ModelAdaptorVTable> self, uintptr_t row, ValueOpaque *out) {
        std::optional<Value> v =
                reinterpret_cast<ModelWrapper *>(self.instance)->model->row_data(int(row));
        if (v.has_value()) {
            *out = v->inner;
            cbindgen_private::slint_interpreter_value_new(&v->inner);
            return true;
        }
        return false;
    };
    auto set_row_data = [](VRef<ModelAdaptorVTable> self, uintptr_t row, const ValueOpaque *value) {
        Value v = *reinterpret_cast<const Value *>(value);
        reinterpret_cast<ModelWrapper *>(self.instance)->model->set_row_data(int(row), v);
    };
    auto get_notify =
            [](VRef<ModelAdaptorVTable> self) -> const cbindgen_private::ModelNotifyOpaque * {
        return &reinterpret_cast<ModelWrapper *>(self.instance)->notify;
    };
    auto drop = [](vtable::VRefMut<ModelAdaptorVTable> self) {
        reinterpret_cast<ModelWrapper *>(self.instance)->self = nullptr;
    };

    static const ModelAdaptorVTable vt { row_count, row_data, set_row_data, get_notify, drop };
    cbindgen_private::slint_interpreter_value_new_model(reinterpret_cast<uint8_t *>(wrapper.get()),
                                                        &vt, &inner);
}

inline Struct::Struct(std::initializer_list<std::pair<std::string_view, Value>> args)
    : Struct(args.begin(), args.end())
{
}

inline std::optional<Value> Struct::get_field(std::string_view name) const
{
    cbindgen_private::Slice<uint8_t> name_view {
        const_cast<unsigned char *>(reinterpret_cast<const unsigned char *>(name.data())),
        name.size()
    };
    if (auto *value = cbindgen_private::slint_interpreter_struct_get_field(&inner, name_view)) {
        return *reinterpret_cast<const Value *>(value);
    } else {
        return {};
    }
}
inline void Struct::set_field(std::string_view name, const Value &value)
{
    cbindgen_private::Slice<uint8_t> name_view {
        const_cast<unsigned char *>(reinterpret_cast<const unsigned char *>(name.data())),
        name.size()
    };
    cbindgen_private::slint_interpreter_struct_set_field(&inner, name_view, &value.inner);
}

/// The ComponentInstance represents a running instance of a component.
///
/// You can create an instance with the ComponentDefinition::create() function.
///
/// Properties and callback can be accessed using the associated functions.
///
/// An instance can be put on screen with the ComponentInstance::show() or the
/// ComponentInstance::run()
class ComponentInstance : vtable::Dyn
{
    ComponentInstance() = delete;
    ComponentInstance(ComponentInstance &) = delete;
    ComponentInstance &operator=(ComponentInstance &) = delete;
    friend class ComponentDefinition;

    // ComponentHandle<ComponentInstance>  is in fact a VRc<ComponentVTable, ErasedComponentBox>
    const cbindgen_private::ErasedComponentBox *inner() const
    {
        slint::private_api::assert_main_thread();
        return reinterpret_cast<const cbindgen_private::ErasedComponentBox *>(this);
    }

public:
    /// Marks the window of this component to be shown on the screen. This registers
    /// the window with the windowing system. In order to react to events from the windowing system,
    /// such as draw requests or mouse/touch input, it is still necessary to spin the event loop,
    /// using slint::run_event_loop().
    void show() const
    {
        cbindgen_private::slint_interpreter_component_instance_show(inner(), true);
    }
    /// Marks the window of this component to be hidden on the screen. This de-registers
    /// the window from the windowing system and it will not receive any further events.
    void hide() const
    {
        cbindgen_private::slint_interpreter_component_instance_show(inner(), false);
    }
    /// Returns the Window associated with this component. The window API can be used
    /// to control different aspects of the integration into the windowing system,
    /// such as the position on the screen.
    const slint::Window &window()
    {
        const cbindgen_private::WindowAdapterRcOpaque *win_ptr = nullptr;
        cbindgen_private::slint_interpreter_component_instance_window(inner(), &win_ptr);
        return *reinterpret_cast<const slint::Window *>(win_ptr);
    }
    /// This is a convenience function that first calls show(), followed by
    /// slint::run_event_loop() and hide().
    void run() const
    {
        show();
        cbindgen_private::slint_run_event_loop();
        hide();
    }
#if defined(SLINT_QT_INTEGRATION) || defined(DOXYGEN)
    /// Return a QWidget for this instance.
    /// This function is only available if the qt graphical backend was compiled in, and
    /// it may return nullptr if the Qt backend is not used at runtime.
    QWidget *qwidget() const
    {
        const cbindgen_private::WindowAdapterRcOpaque *win_ptr = nullptr;
        cbindgen_private::slint_interpreter_component_instance_window(inner(), &win_ptr);
        auto wid = reinterpret_cast<QWidget *>(cbindgen_private::slint_qt_get_widget(
                reinterpret_cast<const cbindgen_private::WindowAdapterRc *>(win_ptr)));
        return wid;
    }
#endif

    /// Set the value for a public property of this component
    ///
    /// For example, if the component has a `property <string> hello;`,
    /// we can set this property
    /// ```
    /// instance->set_property("hello", slint::SharedString("world"));
    /// ```
    ///
    /// Returns true if the property was correctly set. Returns false if the property
    /// could not be set because it either do not exist (was not declared in .slint) or if
    /// the value is not of the proper type for the property's type.
    bool set_property(std::string_view name, const Value &value) const
    {
        using namespace cbindgen_private;
        return slint_interpreter_component_instance_set_property(
                inner(), slint::private_api::string_to_slice(name), &value.inner);
    }
    /// Returns the value behind a property declared in .slint.
    std::optional<Value> get_property(std::string_view name) const
    {
        using namespace cbindgen_private;
        ValueOpaque out;
        if (slint_interpreter_component_instance_get_property(
                    inner(), slint::private_api::string_to_slice(name), &out)) {
            return Value(out);
        } else {
            return {};
        }
    }
    /// Invoke the specified callback declared in .slint with the given arguments
    ///
    /// Example: imagine the .slint file contains the given callback declaration:
    /// ```
    ///     callback foo(string, int) -> string;
    /// ```
    /// Then one can call it with this function
    /// ```
    ///     slint::Value args[] = { SharedString("Hello"), 42. };
    ///     instance->invoke_callback("foo", { args, 2 });
    /// ```
    ///
    /// Returns an null optional if the callback don't exist or if the argument don't match
    /// Otherwise return the returned value from the callback, which may be an empty Value if
    /// the callback did not return a value.
    std::optional<Value> invoke_callback(std::string_view name, std::span<const Value> args) const
    {
        using namespace cbindgen_private;
        Slice<ValueOpaque> args_view { const_cast<ValueOpaque *>(
                                               reinterpret_cast<const ValueOpaque *>(args.data())),
                                       args.size() };
        ValueOpaque out;
        if (slint_interpreter_component_instance_invoke_callback(
                    inner(), slint::private_api::string_to_slice(name), args_view, &out)) {
            return Value(out);
        } else {
            return {};
        }
    }

    /// Set a handler for the callback with the given name.
    ///
    /// A callback with that name must be defined in the document otherwise the function
    /// returns false.
    ///
    /// The \a callback parameter is a functor which takes as argument a slice of Value
    /// and must return a Value.
    ///
    /// Example: imagine the .slint file contains the given callback declaration:
    /// ```
    ///     callback foo(string, int) -> string;
    /// ```
    /// Then one can set the callback handler with this function
    /// ```
    ///   instance->set_callback("foo", [](auto args) {
    ///      std::cout << "foo(" << *args[0].to_string() << ", " << *args[1].to_number() << ")\n";
    ///   });
    /// ```
    ///
    /// Note: Since the ComponentInstance holds the handler, the handler itself should not
    /// capture a strong reference to the instance.
    template<typename F>
    bool set_callback(std::string_view name, F callback) const
    {
        using cbindgen_private::ValueOpaque;
        auto actual_cb = [](void *data, cbindgen_private::Slice<ValueOpaque> arg,
                            ValueOpaque *ret) {
            std::span<const Value> args_view { reinterpret_cast<const Value *>(arg.ptr), arg.len };
            Value r = (*reinterpret_cast<F *>(data))(args_view);
            new (ret) Value(std::move(r));
        };
        return cbindgen_private::slint_interpreter_component_instance_set_callback(
                inner(), slint::private_api::string_to_slice(name), actual_cb,
                new F(std::move(callback)), [](void *data) { delete reinterpret_cast<F *>(data); });
    }

    /// Set the value for a property within an exported global singleton.
    ///
    /// For example, if the main file has an exported global `TheGlobal` with a
    /// `property <int> hello`, we can set this property
    /// ```
    /// instance->set_global_property("TheGlobal", "hello", 42);
    /// ```
    ///
    /// Returns true if the property was correctly set. Returns false if the property
    /// could not be set because it either does not exist (was not declared in .slint) or if
    /// the value is not of the correct type for the property's type.
    ///
    /// **Note:** Only globals that are exported or re-exported from the main .slint file will
    /// be accessible
    bool set_global_property(std::string_view global, std::string_view prop_name,
                             const Value &value) const
    {
        using namespace cbindgen_private;
        return slint_interpreter_component_instance_set_global_property(
                inner(), slint::private_api::string_to_slice(global),
                slint::private_api::string_to_slice(prop_name), &value.inner);
    }
    /// Returns the value behind a property in an exported global singleton.
    std::optional<Value> get_global_property(std::string_view global,
                                             std::string_view prop_name) const
    {
        using namespace cbindgen_private;
        ValueOpaque out;
        if (slint_interpreter_component_instance_get_global_property(
                    inner(), slint::private_api::string_to_slice(global),
                    slint::private_api::string_to_slice(prop_name), &out)) {
            return Value(out);
        } else {
            return {};
        }
    }

    /// Like `set_callback()` but on a callback in the specified exported global singleton.
    ///
    /// Example: imagine the .slint file contains the given global:
    /// ```slint
    ///    export global Logic := {
    ///         callback to_uppercase(string) -> string;
    ///    }
    /// ```
    /// Then you can set the callback handler
    /// ```cpp
    ///    instance->set_global_callback("Logic", "to_uppercase", [](auto args) {
    ///        std::string arg1(*args[0].to_string());
    ///        std::transform(arg1.begin(), arg1.end(), arg1.begin(), toupper);
    ///        return SharedString(arg1);
    ///    })
    /// ```
    ///
    /// **Note:** Only globals that are exported or re-exported from the main .slint file will
    /// be accessible
    template<typename F>
    bool set_global_callback(std::string_view global, std::string_view name, F callback) const
    {
        using cbindgen_private::ValueOpaque;
        auto actual_cb = [](void *data, cbindgen_private::Slice<ValueOpaque> arg,
                            ValueOpaque *ret) {
            std::span<const Value> args_view { reinterpret_cast<const Value *>(arg.ptr), arg.len };
            Value r = (*reinterpret_cast<F *>(data))(args_view);
            new (ret) Value(std::move(r));
        };
        return cbindgen_private::slint_interpreter_component_instance_set_global_callback(
                inner(), slint::private_api::string_to_slice(global),
                slint::private_api::string_to_slice(name), actual_cb, new F(std::move(callback)),
                [](void *data) { delete reinterpret_cast<F *>(data); });
    }

    /// Invoke the specified callback declared in an exported global singleton
    std::optional<Value> invoke_global_callback(std::string_view global,
                                                std::string_view callback_name,
                                                std::span<const Value> args) const
    {
        using namespace cbindgen_private;
        Slice<ValueOpaque> args_view { const_cast<ValueOpaque *>(
                                               reinterpret_cast<const ValueOpaque *>(args.data())),
                                       args.size() };
        ValueOpaque out;
        if (slint_interpreter_component_instance_invoke_global_callback(
                    inner(), slint::private_api::string_to_slice(global),
                    slint::private_api::string_to_slice(callback_name), args_view, &out)) {
            return Value(out);
        } else {
            return {};
        }
    }
};

/// ComponentDefinition is a representation of a compiled component from .slint markup.
///
/// It can be constructed from a .slint file using the ComponentCompiler::build_from_path() or
/// ComponentCompiler::build_from_source() functions. And then it can be instantiated with the
/// create() function.
///
/// The ComponentDefinition acts as a factory to create new instances. When you've finished
/// creating the instances it is safe to destroy the ComponentDefinition.
class ComponentDefinition
{
    friend class ComponentCompiler;

    using ComponentDefinitionOpaque = slint::cbindgen_private::ComponentDefinitionOpaque;
    ComponentDefinitionOpaque inner;

    ComponentDefinition() = delete;
    // Internal constructor that takes ownership of the component definition
    explicit ComponentDefinition(ComponentDefinitionOpaque &inner) : inner(inner) { }

public:
    /// Constructs a new ComponentDefinition as a copy of \a other.
    ComponentDefinition(const ComponentDefinition &other)
    {
        slint_interpreter_component_definition_clone(&other.inner, &inner);
    }
    /// Assigns \a other to this ComponentDefinition.
    ComponentDefinition &operator=(const ComponentDefinition &other)
    {
        using namespace slint::cbindgen_private;

        if (this == &other)
            return *this;

        slint_interpreter_component_definition_destructor(&inner);
        slint_interpreter_component_definition_clone(&other.inner, &inner);

        return *this;
    }
    /// Destroys this ComponentDefinition.
    ~ComponentDefinition() { slint_interpreter_component_definition_destructor(&inner); }
    /// Creates a new instance of the component and returns a shared handle to it.
    ComponentHandle<ComponentInstance> create() const
    {
        union CI {
            cbindgen_private::ComponentInstance i;
            ComponentHandle<ComponentInstance> result;
            ~CI() { result.~ComponentHandle(); }
            CI() { }
        } u;
        cbindgen_private::slint_interpreter_component_instance_create(&inner, &u.i);
        return u.result;
    }

    /// Returns a vector of that contains PropertyDescriptor instances that describe the list of
    /// public properties that can be read and written using ComponentInstance::set_property and
    /// ComponentInstance::get_property.
    slint::SharedVector<PropertyDescriptor> properties() const
    {
        slint::SharedVector<PropertyDescriptor> props;
        cbindgen_private::slint_interpreter_component_definition_properties(&inner, &props);
        return props;
    }

    /// Returns a vector of strings that describe the list of public callbacks that can be invoked
    /// using ComponentInstance::invoke_callback and set using ComponentInstance::set_callback.
    slint::SharedVector<slint::SharedString> callbacks() const
    {
        slint::SharedVector<slint::SharedString> callbacks;
        cbindgen_private::slint_interpreter_component_definition_callbacks(&inner, &callbacks);
        return callbacks;
    }

    /// Returns the name of this Component as written in the .slint file
    slint::SharedString name() const
    {
        slint::SharedString name;
        cbindgen_private::slint_interpreter_component_definition_name(&inner, &name);
        return name;
    }

    /// Returns a vector of strings with the names of all exported global singletons.
    slint::SharedVector<slint::SharedString> globals() const
    {
        slint::SharedVector<slint::SharedString> names;
        cbindgen_private::slint_interpreter_component_definition_globals(&inner, &names);
        return names;
    }

    /// Returns a vector of the property descriptors of the properties of the specified
    /// publicly exported global singleton. An empty optional is returned if there exists no
    /// exported global singleton under the specified name.
    std::optional<slint::SharedVector<PropertyDescriptor>>
    global_properties(std::string_view global_name) const
    {
        slint::SharedVector<PropertyDescriptor> properties;
        if (cbindgen_private::slint_interpreter_component_definition_global_properties(
                    &inner, slint::private_api::string_to_slice(global_name), &properties)) {
            return properties;
        }
        return {};
    }

    /// Returns a vector of the names of the callbacks of the specified publicly exported global
    /// singleton. An empty optional is returned if there exists no exported global singleton
    /// under the specified name.
    std::optional<slint::SharedVector<slint::SharedString>>
    global_callbacks(std::string_view global_name) const
    {
        slint::SharedVector<slint::SharedString> names;
        if (cbindgen_private::slint_interpreter_component_definition_global_callbacks(
                    &inner, slint::private_api::string_to_slice(global_name), &names)) {
            return names;
        }
        return {};
    }
};

/// ComponentCompiler is the entry point to the Slint interpreter that can be used
/// to load .slint files or compile them on-the-fly from a string
/// (using build_from_source()) or from a path  (using build_from_source())
class ComponentCompiler
{
    cbindgen_private::ComponentCompilerOpaque inner;

    ComponentCompiler(ComponentCompiler &) = delete;
    ComponentCompiler &operator=(ComponentCompiler &) = delete;

public:
    /// Constructs a new ComponentCompiler instance.
    ComponentCompiler() { cbindgen_private::slint_interpreter_component_compiler_new(&inner); }

    /// Destroys this ComponentCompiler.
    ~ComponentCompiler()
    {
        cbindgen_private::slint_interpreter_component_compiler_destructor(&inner);
    }

    /// Sets the include paths used for looking up `.slint` imports to the specified vector of
    /// paths.
    void set_include_paths(const slint::SharedVector<slint::SharedString> &paths)
    {
        cbindgen_private::slint_interpreter_component_compiler_set_include_paths(&inner, &paths);
    }

    /// Sets the style to be used for widgets.
    void set_style(std::string_view style)
    {
        cbindgen_private::slint_interpreter_component_compiler_set_style(
                &inner, slint::private_api::string_to_slice(style));
    }

    /// Returns the widget style the compiler is currently using when compiling .slint files.
    slint::SharedString style() const
    {
        slint::SharedString s;
        cbindgen_private::slint_interpreter_component_compiler_get_style(&inner, &s);
        return s;
    }

    /// Returns the include paths the component compiler is currently configured with.
    slint::SharedVector<slint::SharedString> include_paths() const
    {
        slint::SharedVector<slint::SharedString> paths;
        cbindgen_private::slint_interpreter_component_compiler_get_include_paths(&inner, &paths);
        return paths;
    }

    /// Returns the diagnostics that were produced in the last call to build_from_path() or
    /// build_from_source().
    slint::SharedVector<Diagnostic> diagnostics() const
    {
        slint::SharedVector<Diagnostic> result;
        cbindgen_private::slint_interpreter_component_compiler_get_diagnostics(&inner, &result);
        return result;
    }

    /// Compile a .slint file into a ComponentDefinition
    ///
    /// Returns the compiled `ComponentDefinition` if there were no errors.
    ///
    /// Any diagnostics produced during the compilation, such as warnings or errors, are collected
    /// in this ComponentCompiler and can be retrieved after the call using the diagnostics()
    /// function.
    ///
    /// Diagnostics from previous calls are cleared when calling this function.
    std::optional<ComponentDefinition> build_from_source(std::string_view source_code,
                                                         std::string_view path)
    {
        cbindgen_private::ComponentDefinitionOpaque result;
        if (cbindgen_private::slint_interpreter_component_compiler_build_from_source(
                    &inner, slint::private_api::string_to_slice(source_code),
                    slint::private_api::string_to_slice(path), &result)) {

            return ComponentDefinition(result);
        } else {
            return {};
        }
    }

    /// Compile some .slint code into a ComponentDefinition
    ///
    /// The `path` argument will be used for diagnostics and to compute relative
    /// paths while importing.
    ///
    /// Any diagnostics produced during the compilation, such as warnings or errors, are collected
    /// in this ComponentCompiler and can be retrieved after the call using the
    /// Self::diagnostics() function.
    ///
    /// Diagnostics from previous calls are cleared when calling this function.
    std::optional<ComponentDefinition> build_from_path(std::string_view path)
    {
        cbindgen_private::ComponentDefinitionOpaque result;
        if (cbindgen_private::slint_interpreter_component_compiler_build_from_path(
                    &inner, slint::private_api::string_to_slice(path), &result)) {

            return ComponentDefinition(result);
        } else {
            return {};
        }
    }
};

}

namespace slint::testing {

using cbindgen_private::KeyboardModifiers;

/// Send a key events to the given component instance
inline void send_key_event(const slint::interpreter::ComponentInstance *component, slint::Key key,
                           bool pressed)
{
    const cbindgen_private::WindowAdapterRcOpaque *win_ptr = nullptr;
    cbindgen_private::slint_interpreter_component_instance_window(
            reinterpret_cast<const cbindgen_private::ErasedComponentBox *>(component), &win_ptr);
    cbindgen_private::send_key_event(
            key, pressed, reinterpret_cast<const cbindgen_private::WindowAdapterRc *>(win_ptr));
}

/// Send a key events to the given component instance
inline void send_keyboard_string_sequence(const slint::interpreter::ComponentInstance *component,
                                          const slint::SharedString &str,
                                          KeyboardModifiers modifiers = {})
{
    const cbindgen_private::WindowAdapterRcOpaque *win_ptr = nullptr;
    cbindgen_private::slint_interpreter_component_instance_window(
            reinterpret_cast<const cbindgen_private::ErasedComponentBox *>(component), &win_ptr);
    cbindgen_private::send_keyboard_string_sequence(
            &str, modifiers, reinterpret_cast<const cbindgen_private::WindowAdapterRc *>(win_ptr));
}
}
