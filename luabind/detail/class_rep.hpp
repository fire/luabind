// Copyright (c) 2003 Daniel Wallin and Arvid Norberg

// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
// SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
// OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef LUABIND_CLASS_REP_HPP_INCLUDED
#define LUABIND_CLASS_REP_HPP_INCLUDED

#include <boost/limits.hpp>
#include <boost/preprocessor/repetition/enum_params_with_a_default.hpp>

#include <utility>

#include <luabind/config.hpp>
#include <luabind/detail/object_rep.hpp>
#include <luabind/detail/construct_rep.hpp>
#include <luabind/detail/garbage_collector.hpp>
#include <luabind/detail/operator_id.hpp>
#include <luabind/detail/signature_match.hpp>
#include <luabind/detail/class_registry.hpp>
#include <luabind/detail/find_best_match.hpp>
#include <luabind/detail/get_overload_signature.hpp>
#include <luabind/error.hpp>
#include <luabind/detail/method_rep.hpp>

namespace luabind
{

	template<BOOST_PP_ENUM_PARAMS_WITH_A_DEFAULT(LUABIND_MAX_BASES, class A, detail::null_type)>
	struct bases {};
	typedef bases<detail::null_type> no_bases;
}

namespace luabind { namespace detail
{

	struct method_rep;
	LUABIND_API std::string stack_content_by_name(lua_State* L, int start_index);
	int construct_lua_class_callback(lua_State* L);

	struct class_registration;
	
	// this is class-specific information, poor man's vtable
	// this is allocated statically (removed by the compiler)
	// a pointer to this structure is stored in the lua tables'
	// metatable with the name __classrep
	// it is used when matching parameters to function calls
	// to determine possible implicit casts
	// it is also used when finding the best match for overloaded
	// methods

	class LUABIND_API class_rep
	{
	friend struct class_registration;
	friend int super_callback(lua_State*);
//TODO: avoid the lua-prefix
	friend int lua_class_gettable(lua_State*);
	friend int lua_class_settable(lua_State*);
	friend int static_class_gettable(lua_State*);
	public:

		enum class_type
		{
			cpp_class = 0,
			lua_class = 1
		};

		// destructor is a lua callback function that is hooked as garbage collector event on every instance
		// of this class (including those that is not owned by lua). It gets an object_rep as argument
		// on the lua stack. It should delete the object pointed to by object_rep::ptr if object_pre::flags
		// is object_rep::owner (which means that lua owns the object)

		// EXPECTS THE TOP VALUE ON THE LUA STACK TO
		// BE THE USER DATA WHERE THIS CLASS IS BEING
		// INSTANTIATED!
		class_rep(LUABIND_TYPE_INFO type
			, const char* name
			, lua_State* L
			, void(*destructor)(void*)
			, void(*const_holder_destructor)(void*)
			, LUABIND_TYPE_INFO holder_type
			, LUABIND_TYPE_INFO const_holder_type
			, void*(*extractor)(void*)
			, const void*(*const_extractor)(void*)
			, void(*const_converter)(void*,void*)
			, void(*construct_holder)(void*,void*)
			, void(*construct_const_holder)(void*,void*)
			, int holder_size
			, int holder_alignment);

		// used when creating a lua class
		// EXPECTS THE TOP VALUE ON THE LUA STACK TO
		// BE THE USER DATA WHERE THIS CLASS IS BEING
		// INSTANTIATED!
		class_rep(lua_State* L, const char* name);

		~class_rep();

		std::pair<void*,void*> allocate(lua_State* L) const;

		// called from the metamethod for __index
		// the object pointer is passed on the lua stack
		int gettable(lua_State* L);

		// called from the metamethod for __newindex
		// the object pointer is passed on the lua stack
		bool settable(lua_State* L);

		// this is called as __index metamethod on every instance of this class
		static int gettable_dispatcher(lua_State* L);

		// this is called as __newindex metamethod on every instance of this class
		static int settable_dispatcher(lua_State* L);
		static int operator_dispatcher(lua_State* L);

		// this is called as metamethod __call on the class_rep.
		static int constructor_dispatcher(lua_State* L);

//		static int implicit_cast(const class_rep* from, const class_rep* to, int& pointer_offset);

		// the functions dispatcher assumes the following:
		// there is one upvalue that points to the method_rep that this dispatcher is to call
		// the first parameter on the lua stack is an object_rep that points to the object the
		// call is being made on
		static int function_dispatcher(lua_State* L);

		struct base_info
		{
			int pointer_offset; // the offset added to the pointer to obtain a basepointer (due to multiple-inheritance)
			class_rep* base;
		};


		void add_base_class(const base_info& binfo);

		inline const std::vector<base_info>& bases() const throw() { return m_bases; }

		inline void set_type(LUABIND_TYPE_INFO t) { m_type = t; }
		inline LUABIND_TYPE_INFO type() const throw() { return m_type; }
		inline LUABIND_TYPE_INFO holder_type() const throw() { return m_holder_type; }
		inline LUABIND_TYPE_INFO const_holder_type() const throw() { return m_const_holder_type; }
		inline bool has_holder() const throw() { return m_construct_holder != 0; }

		inline const char* name() const throw() { return m_name; }

		// the lua reference to this class_rep
		inline int self_ref() const throw() { return m_self_ref; }
		// the lua reference to the metatable for this class' instances
		inline int metatable_ref() const throw() { return m_instance_metatable; }
		inline int table_ref() const { return m_table_ref; }

		inline void(*destructor() const)(void*) { return m_destructor; }
		inline void(*const_holder_destructor() const)(void*) { return m_const_holder_destructor; }
		typedef const void*(*t_const_extractor)(void*);
		inline t_const_extractor const_extractor() const { return m_const_extractor; }
		typedef void*(*t_extractor)(void*);
		inline t_extractor extractor() const { return m_extractor; }

		inline void(*const_converter() const)(void*,void*) { return m_const_converter; }

		inline class_type get_class_type() const { return m_class_type; }

		void add_static_constant(const char* name, int val);
		void add_method(lua_State* L, const char* name, detail::method_rep& m);

		static int super_callback(lua_State* L);

		static int lua_settable_dispatcher(lua_State* L);
		static int construct_lua_class_callback(lua_State* L);

		// called from the metamethod for __index
		// obj is the object pointer
		static int lua_class_gettable(lua_State* L);

		// called from the metamethod for __newindex
		// obj is the object pointer
		static int lua_class_settable(lua_State* L);

		// called from the metamethod for __index
		// obj is the object pointer
		static int static_class_gettable(lua_State* L);

		void* convert_to(LUABIND_TYPE_INFO target_type, const object_rep* obj, void*) const;

		bool has_operator_in_lua(lua_State*, int id);

		// this is used to describe setters and getters
		struct callback
		{
			boost::function2<int, lua_State*, int> func;
#ifndef LUABIND_NO_ERROR_CHECKING
			int (*match)(lua_State*, int);

			typedef void(*get_sig_ptr)(lua_State*, std::string&);
			get_sig_ptr sig;
#endif
			int pointer_offset;
		};

		const std::map<const char*, callback, ltstr>& properties() const;
		typedef std::map<const char*, callback, ltstr> property_map;

		int holder_alignment() const
		{
			return m_holder_alignment;
		}

		int holder_size() const
		{
			return m_holder_size;
		}


		void set_holder_alignment(int n)
		{
			m_holder_alignment = n;
		}

		void set_holder_size(int n)
		{
			m_holder_size = n;
		}
	
		void derived_from(const class_rep* base)
		{
			m_holder_alignment = base->m_holder_alignment;
			m_holder_size = base->m_holder_size;
			m_holder_type = base->m_holder_type;
			m_const_holder_type = base->m_const_holder_type;
			m_extractor = base->m_extractor;
			m_const_extractor = base->m_const_extractor;
			m_const_converter = base->m_const_converter;
			m_construct_holder = base->m_construct_holder;
			m_construct_const_holder = base->m_construct_const_holder;
		}

		struct operator_callback: public overload_rep_base
		{
			inline void set_fun(int (*f)(lua_State*)) { func = f; }
			inline int call(lua_State* L) { return func(L); }
			inline void set_arity(int arity) { m_arity = arity; }

		private:
			int(*func)(lua_State*);
		};
		
	private:

		void cache_operators(lua_State*);

		// this is a pointer to the type_info structure for
		// this type
		// warning: this may be a problem when using dll:s, since
		// typeid() may actually return different pointers for the same
		// type.
		LUABIND_TYPE_INFO m_type;
		LUABIND_TYPE_INFO m_holder_type;
		LUABIND_TYPE_INFO m_const_holder_type;

		// this function pointer is used if the type is held by
		// a smart pointer. This function takes the type we are holding
		// (the held_type, the smart pointer) and extracts the actual
		// pointer.
		void*(*m_extractor)(void*);
		const void*(*m_const_extractor)(void*);

		void(*m_const_converter)(void*, void*);

		// this function is used to construct the held_type
		// (the smart pointer). The arguments are the memory
		// in which it should be constructed (with placement new)
		// and the raw pointer that should be wrapped in the
		// smart pointer
		typedef void(*construct_held_type_t)(void*,void*);
		construct_held_type_t m_construct_holder;
		construct_held_type_t m_construct_const_holder;

		// this is the size of the userdata chunk
		// for each object_rep of this class. We
		// need this since held_types are constructed
		// in the same memory (to avoid fragmentation)
		int m_holder_size;
		int m_holder_alignment;

		// a list of info for every class this class derives from
		// the information stored here is sufficient to do
		// type casts to the base classes
		std::vector<base_info> m_bases;

		// the class' name (as given when registered to lua with class_)
		const char* m_name;

		// contains signatures and construction functions
		// for all constructors
		construct_rep m_constructor;

		// a reference to this structure itself. Since this struct
		// is kept inside lua (to let lua collect it when lua_close()
		// is called) we need to lock it to prevent collection.
		// the actual reference is not currently used.
		int m_self_ref;

		// a reference to the lua table that represents this class
		// (only used if it is a lua class)
		// *** CHANGE ***
		// this should always be used, when accessing static
		// members in the class, and even when accessing
		// members in instances of a class.
		int m_table_ref;

		// the type of this class.. determines if it's written in c++ or lua
		class_type m_class_type;

		// this is a lua reference that points to the lua table
		// that is to be used as meta table for all instances
		// of this class.
		int m_instance_metatable;

		// ***** the maps below contains all members in this class *****

		// maps method names to a structure with more
		// information about that method.
		// that struct contains the function-signatures
		// for every overload
		std::map<const char*, method_rep, ltstr> m_methods;

		// datamembers, some members may be readonly, and
		// only have a getter function
		std::map<const char*, callback, ltstr> m_getters;
		std::map<const char*, callback, ltstr> m_setters;

		std::vector<operator_callback> m_operators[number_of_operators]; // the operators in lua

		void(*m_destructor)(void*);
		void(*m_const_holder_destructor)(void*);

		std::map<const char*, int, ltstr> m_static_constants;

		// the first time an operator is invoked
		// we check the associated lua table
		// and cache the result
		int m_operator_cache;
	};

	bool is_class_rep(lua_State* L, int index);

}}

#include <luabind/detail/overload_rep_impl.hpp>

#endif // LUABIND_CLASS_REP_HPP_INCLUDED
