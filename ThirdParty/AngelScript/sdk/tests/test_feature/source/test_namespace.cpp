#include "utils.h"

namespace TestNamespace
{

bool Test()
{
	bool fail = false;
	asIScriptEngine *engine;
	int r;
	COutStream out;
	CBufferedOutStream bout;

	// Calling base class method from overridden method when base class is declared in a different namespace
	// http://www.gamedev.net/topic/675631-unable-to-call-base-function-on-class-outside-of-namespace/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"namespace Test \n"
			"{ \n"
			"	class Bar : Foo \n"
			"	{ \n"
			"		void Stuff() override \n"
			"		{ \n"
			"			Foo::Stuff();  \n"
			"			::Foo::Stuff();  \n"
			"		} \n"
			"	} \n"
			"} \n"
			"class Foo \n"
			"{ \n"
			"	void Stuff() \n"
			"	{ \n"
			"		// do stuff \n"
			"	} \n"
			"} \n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Partial namespace specialization of enum in namespace
	// http://www.gamedev.net/topic/673284-namespace-auto-detection-fail/
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"namespace A\n"
			"{\n"
			"	enum B { b1 }\n"
			"	void main()\n"
			"	{\n"
			"		B b1 = B::b1; \n"		// Partial namespace specialization
			"		B b2 = b1; \n"			// Implicit namespace
			"		A::B b3 = A::B::b1; \n"	// Complete namespace specialization
			"		::C c1 = ::C::c1; \n"	// Complete namespace specialization
			"		C c2 = c1; \n"			// Implicit namespace
			"		C c3 = C::c1; \n"		// Partial namespace specialization
			"	}\n"
			"} \n"
			"enum C { c1 }\n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test bug with namespace
	// http://www.gamedev.net/topic/672251-unknown-datatype-when-using-class-in-namespace/
	{
		engine = asCreateScriptEngine();
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"namespace NS \n"
			"{ \n"
			"	class Foo \n"
			"	{ \n"
			"	} \n"
			"	class Bar \n"
			"	{ \n"
			"		Foo@ foo = Foo(); \n"
			"	} \n"
			"} \n");
		r = mod->Build();
		if (r < 0)
			TEST_FAILED;

		if (bout.buffer != "")
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test nested namespace from within class method
	// http://www.gamedev.net/topic/670858-nested-namespaces/
	SKIP_ON_MAX_PORT
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		RegisterStdString(engine);

		r = engine->SetDefaultNamespace("dev::log");
		r = engine->RegisterGlobalFunction("void info(const ::string &in)", asFUNCTION(0), asCALL_CDECL); assert(r >= 0);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
				"class Test \n"
				"{ \n"
				"  void huh() \n"
				"  { \n"
				"    dev::log::info('Hi'); \n"
				"  } \n"
				"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->ShutDownAndRelease();
	}

	// Test bug with namespace
	// http://www.gamedev.net/topic/667516-namespace-bug/
	SKIP_ON_MAX_PORT
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream,Callback), &out, asCALL_THISCALL);

		engine->SetDefaultNamespace("NSBugTest");
		engine->RegisterObjectType("FooObj", 0, asOBJ_REF | asOBJ_NOCOUNT);
		engine->RegisterObjectMethod("FooObj", "int opIndex(int)", asFUNCTION(0), asCALL_THISCALL);
		engine->RegisterObjectMethod("FooObj", "void test()", asFUNCTION(0), asCALL_THISCALL);
		engine->SetDefaultNamespace("");

		r = engine->RegisterGlobalProperty("NSBugTest::FooObj FooObj", (void*)1);
		if( r < 0 )
			TEST_FAILED;

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
				"void main() \n"
				"{ \n"
				"   FooObj.test();\n"
				"   int num = FooObj[0];\n"
				"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		engine->ShutDownAndRelease();
	}

	// test name conflict between template and non-template in different namespaces
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";

		engine->SetDefaultNamespace("A");
		RegisterScriptArray(engine, false);
		engine->SetDefaultNamespace("B");
		engine->RegisterObjectType("array", 4, asOBJ_VALUE|asOBJ_POD);
		engine->SetDefaultNamespace("");

		// Positive
		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"A::array<int> a; \n"
			"B::array b; \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		// Negative
		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"A::array a; \n"
			"B::array<int> b; \n");
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "test (1, 4) : Error   : Template 'array' expects 1 sub type(s)\n"
		                   "test (2, 4) : Error   : Type 'array' is not a template type\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}
		
		engine->Release();
	}

	// class in namespace referring to variable in global namespace
	// http://www.gamedev.net/topic/666308-errors-produced-by-classes-in-namespaces-accessing-global-properties/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"int i; \n"
			"void f(int a = i) {} \n"
			"namespace n { \n"
			" class c { \n"
			"  void m() { \n"
			"   f(); \n"
			"  } \n"
			" } \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"int i; \n"
			"namespace n { \n"
			" class c { \n"
			"  int p = i; \n"
			" } \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	// Referring to base class' method with namespace
	// http://www.gamedev.net/topic/662755-fully-qualified-namespace-when-calling-base-class-implementation/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"namespace ns { \n"
			"  class Base { \n"
			"    void Setup() {} \n"
			"  } \n"
			"  class Derived : ns::Base { \n"
			"    void Setup() { ns::Base::Setup(); } \n"
			"  } \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		// Shouldn't be possible to refer to Base class in different namespace
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";
		mod->AddScriptSection("test",
			"namespace ns2 { \n"
			"  class Base { \n"
			"    void Setup() {} \n"
			"  } \n"
			"} \n"
			"namespace ns { \n"
			"  class Base { \n"
			"    void Setup() {} \n"
			"  } \n"
			"  class Derived : ns::Base { \n"
			"    void Setup() { ns2::Base::Setup(); } \n"
			"  } \n"
			"} \n");
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		// TODO: The error message should be better, something like: Cannot call non-static member of class ns2::Base
		if( bout.buffer != "test (11, 5) : Info    : Compiling void Derived::Setup()\n"
						   "test (11, 20) : Error   : Namespace 'ns2::Base' doesn't exist.\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Overloading type name in namespace with local variable
	// http://www.gamedev.net/topic/658748-namespace-bug/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"namespace n1 \n"
			"{ \n"
			"	class c1 \n"
			"	{ \n"
			"	} \n"
			"} \n"
			"void main() \n"
			"{ \n"
			"	int c1 = 1; \n"
			"	if(c1 < 2) // Error here \n"
			"		c1 = 3; \n"
			"} \n");

		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	// The compiler should search parent namespaces for inherited classes
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"class Parent {} \n"
			"namespace child { \n"
			"   class Child : Parent {} \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	// The compiler should search parent namespaces for matching global functions
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"void func() {}; \n"
			"namespace B { \n"
			"void main() { \n"
			"  func(); \n"
			"} \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		// Fully specify the namespace to find the function
		asIScriptFunction *func = mod->GetFunctionByDecl("void B::main()");
		std::string str = func->GetDeclaration(true, true, true);
		if( str != "void B::main()" )
		{
			PRINTF("%s", str.c_str());
			TEST_FAILED;
		}

		// Should be possible to find the function by using the global scope
		func = mod->GetFunctionByDecl("void ::func()");
		str = func->GetDeclaration(true, true, true);
		if( str != "void func()" )
		{
			PRINTF("%s", str.c_str());
			TEST_FAILED;
		}
		

		engine->Release();
	}

	// The compiler should search parent namespaces for matching variables
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"int a; \n"
			"namespace B { \n"
			"void main() { \n"
			"  int b = a; \n"
			"} \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		// Must be possible to find variable with fully specified namespace
		int idx = mod->GetGlobalVarIndexByDecl("int ::a");
		if( idx < 0 )
			TEST_FAILED;
		std::string str = mod->GetGlobalVarDeclaration(idx, true);
		if( str != "int a" )
		{
			PRINTF("%s", str.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// The compiler should search parent namespaces for matching types
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test",
			"class A {} \n"
			"namespace B { \n"
			"void main() { \n"
			"  A a; \n"
			"} \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	// Test different namespaces with declaration of classes of the same name
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterScriptArray(engine, true);
		
		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", 
			"namespace net \n"
			"{ \n"
			"   namespace room \n"
			"   { \n"
			"        class kernel \n"
			"        { \n"
			"              kernel() \n"
			"              { \n"
			"              } \n"
			"        } \n"
			"   } \n"
			"} \n"
			"namespace net \n"
			"{ \n"
			"   namespace lobby \n"
			"   { \n"
			"        class kernel \n"
			"        { \n"
			"              private int[]            _Values; \n"
			"              kernel() \n"
			"              { \n"
			"                    _Values.resize(10); \n"
			"              } \n"
			"        } \n"
			"   } \n"
			"} \n"
			"net::lobby::kernel kernel;\n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		// Fully specify the namespace to get the correct object
		asITypeInfo *type = mod->GetTypeInfoByDecl("net::room::kernel");
		std::string str = engine->GetTypeDeclaration(type->GetTypeId(), true);
		if( str != "net::room::kernel" )
		{
			PRINTF("%s", str.c_str());
			TEST_FAILED;
		}

		// Also possible to get it by setting the default namespace
		mod->SetDefaultNamespace("net::room");
		type = mod->GetTypeInfoByDecl("kernel");
		str = engine->GetTypeDeclaration(type->GetTypeId(), true);
		if( str != "net::room::kernel" )
		{
			PRINTF("%s", str.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test multiple script sections
	// http://www.gamedev.net/topic/638946-namespace-problems/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterScriptArray(engine, false);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test1", 
			"namespace nsTestTwo {\n"
			"::array<int> arrgh; \n"
			"shared interface nsIface\n"
			"{\n"
			"    nsIface@ parent { get; }\n"
			"}\n"
			"}\n");
		mod->AddScriptSection("test2",
			"namespace nsTestTwo {\n"
			"class nsClass : nsIface\n"
			"{\n"
			"    nsIface@ mommy;\n"
			"    nsClass( nsIface@ parent )\n"
			"    {\n"
			"        @this.mommy = parent;\n"
			"    }\n"
			"    nsIface@ get_parent()\n"
			"    {\n"
			"        return( @this.mommy );\n"
			"    }\n"
			"}\n"
			"}\n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script =
			"int func() { return var; } \n"
			"int func2() { return var; } \n"
			"int var = 0; \n"
			"class cl { cl() {v = 0;} int v; } \n"
			"interface i {} \n"
			"enum e { e1 = 0 } \n"
			"funcdef void fd(); \n"
			// Namespaces allow same entities to be declared again
			"namespace a { \n"
			"  int func() { return var; } \n" // Should find the global var in the same namespace
			"  int func2() { return func(); } \n" // Should find the global function in the same namespace
			"  int var = 1; \n"
			"  class cl { cl() {v = 1;} int v; } \n"
			"  interface i {} \n"
			"  enum e { e1 = 1 } \n"
			"  funcdef void fd(); \n"
			"  ::e MyFunc() { return ::e1; } \n"
			// Nested namespaces are allowed
			"  namespace b { \n"
			"    int var = 2; \n"
			"    void funcParams(int a, float b) { a+b; } \n"
			"  } \n"
			// Accessing global variables from within a namespace is also possible
			"  int getglobalvar() { return ::var; } \n"
			"} \n"
			// The class should be able to declare methods to return and take types from other namespaces
			"class MyClass { \n"
			"  a::e func(a::e val) { return val; } \n"
			"  ::e func(::e val) { return val; } \n"
			"} \n"
			// Global functions must also be able to return and take types from other namespaces
			"a::e MyFunc(a::e val) { return val; } \n"
			// It's possible to specify exactly which one is wanted
			"cl gc; \n"
			"a::cl gca; \n"
			"void main() \n"
			"{ \n"
			"  assert(var == 0); \n"
			"  assert(::var == 0); \n"
			"  assert(a::var == 1); \n"
			"  assert(a::b::var == 2); \n"
			"  assert(func() == 0); \n"
			"  assert(a::func() == 1); \n"
			"  assert(func2() == 0); \n"
			"  assert(a::func2() == 1); \n"
			"  assert(e1 == 0); \n"
			"  assert(::e1 == 0); \n"
			"  assert(e::e1 == 0); \n"
			"  assert(::e::e1 == 0); \n"
			"  assert(a::e1 == 1); \n"
			"  assert(a::e::e1 == 1); \n"
			"  cl c; \n"
			"  a::cl ca; \n"
			"  assert(c.v == 0); \n"
			"  assert(ca.v == 1); \n"
			"  assert(gc.v == 0); \n"
			"  assert(gca.v == 1); \n"
			"  assert(a::getglobalvar() == 0); \n"
			"} \n";

		asIScriptModule *mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		else
		{
			r = ExecuteString(engine, "main()", mod);
			if( r != asEXECUTION_FINISHED )
				TEST_FAILED;
	
			// Retrieving entities should work properly with namespace
			mod->SetDefaultNamespace("a");
			asIScriptFunction *f1 = mod->GetFunctionByDecl("int func()");
			asIScriptFunction *f2 = mod->GetFunctionByDecl("int a::func()");
			asIScriptFunction *f3 = mod->GetFunctionByName("func");
			if( f1 == 0 || f1 != f2 || f1 != f3 )
				TEST_FAILED;

			int v1 = mod->GetGlobalVarIndexByName("var");
			int v2 = mod->GetGlobalVarIndexByDecl("int var");
			int v3 = mod->GetGlobalVarIndexByDecl("int a::var");
			if( v1 < 0 || v1 != v2 || v1 != v3 )
				TEST_FAILED;

			int t1 = mod->GetTypeIdByDecl("cl");
			int t2 = mod->GetTypeIdByDecl("a::cl");
			if( t1 < 0 || t1 != t2 )
				TEST_FAILED;

			// Functions with parameters must work too
			asIScriptFunction *f = mod->GetFunctionByDecl("void a::b::funcParams(int, float)");
			if( f == 0 || std::string(f->GetDeclaration(true, true)) != "void a::b::funcParams(int, float)" )
				TEST_FAILED;

			// Test saving and loading 
			CBytecodeStream s("");
			mod->SaveByteCode(&s);

			asIScriptModule *mod2 = engine->GetModule("mod2", asGM_ALWAYS_CREATE);
			r = mod2->LoadByteCode(&s);
			if( r < 0 )
				TEST_FAILED;

			r = ExecuteString(engine, "main()", mod2);
			if( r != asEXECUTION_FINISHED )
				TEST_FAILED;
		}

		engine->Release();
	}

	// Test registering interface with namespace
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		
		r = engine->SetDefaultNamespace("test"); assert( r >= 0 );

		r = engine->RegisterObjectType("t", 0, asOBJ_REF); assert( r >= 0 );
		r = engine->RegisterObjectBehaviour("t", asBEHAVE_ADDREF, "void f()", asFUNCTION(0), asCALL_GENERIC); assert( r >= 0 );
		r = engine->RegisterObjectMethod("t", "void f()", asFUNCTION(0), asCALL_GENERIC); assert( r >= 0 );
		r = engine->RegisterObjectProperty("t", "int a", 0); assert( r >= 0 );
		int t1 = engine->GetTypeIdByDecl("t");
		int t2 = engine->GetTypeIdByDecl("test::t");
		if( t1 < 0 || t1 != t2 )
			TEST_FAILED;
		
		r = engine->RegisterInterface("i"); assert( r >= 0 );
		r = engine->RegisterInterfaceMethod("i", "void f()"); assert( r >= 0 );
		t1 = engine->GetTypeIdByDecl("test::i");
		if( t1 < 0 )
			TEST_FAILED;

		r = engine->RegisterEnum("e"); assert( r >= 0 );
		r = engine->RegisterEnumValue("e", "e1", 0); assert( r >= 0 );
		t1 = engine->GetTypeIdByDecl("test::e");
		if( t1 < 0 )
			TEST_FAILED;

		r = engine->RegisterFuncdef("void f()"); assert( r >= 0 );
		t1 = engine->GetTypeIdByDecl("test::f");
		if( t1 < 0 )
			TEST_FAILED;

		r = engine->RegisterGlobalFunction("void gf()", asFUNCTION(0), asCALL_GENERIC); assert( r >= 0 );
		asIScriptFunction *f1 = engine->GetGlobalFunctionByDecl("void test::gf()");
		asIScriptFunction *f2 = engine->GetGlobalFunctionByDecl("void gf()");
		if( f1 == 0 || f1 != f2 )
			TEST_FAILED;

		r = engine->RegisterGlobalProperty("int gp", (void*)1); assert( r >= 0 );
		int g1 = engine->GetGlobalPropertyIndexByName("gp");
		int g2 = engine->GetGlobalPropertyIndexByDecl("int gp");
		int g3 = engine->GetGlobalPropertyIndexByDecl("int test::gp");
		if( g1 < 0 || g1 != g2 || g1 != g3 )
			TEST_FAILED;

		r = engine->RegisterTypedef("td", "int"); assert( r >= 0 );
		t1 = engine->GetTypeIdByDecl("test::td");
		if( t1 < 0 )
			TEST_FAILED;

		engine->Release();
	}


	// Test accessing registered properties in different namespaces from within function in another namespace
	// http://www.gamedev.net/topic/624376-accessing-global-property-from-within-script-namespace/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);

		int a = 0, b = 0;
		r = engine->RegisterGlobalProperty("int a", &a);
		r = engine->SetDefaultNamespace("test");
		r = engine->RegisterGlobalProperty("int b", &b);

		asIScriptModule *mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", 
			"namespace script { \n"
			"void func() \n"
			"{ \n"
			"  a = 1; \n"
			"} \n"
			"} \n");
		bout.buffer = "";
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		mod->AddScriptSection("test", 
			"namespace script { \n"
			"void func() \n"
			"{ \n"
			"  ::a = 1; \n"
			"  test::b = 2; \n"
			"} \n"
			"} \n");
		bout.buffer = "";
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;
		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		r = ExecuteString(engine, "script::func()", mod);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		if( a != 1 || b != 2 )
			TEST_FAILED;

		engine->Release();		
	}

	// Test registering enum with the same name in two different namespaces
	// http://www.gamedev.net/topic/625214-enum-collision-across-namespaces/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		engine->SetDefaultNamespace("A");
		r = engine->RegisterEnum("ENUM"); assert( r >= 0 );
		r = engine->RegisterEnumValue("ENUM", "VALUE", 1); assert( r >= 0 );

		engine->SetDefaultNamespace("B");
		r = engine->RegisterEnum("ENUM"); assert( r >= 0 );
		r = engine->RegisterEnumValue("ENUM", "VALUE", 2); assert( r >= 0 );

		engine->SetDefaultNamespace("");

		r = engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC); assert( r >= 0 );

		r = ExecuteString(engine, "int a = A::ENUM::VALUE; assert(a == 1)", 0, 0);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		r = ExecuteString(engine, "int b = B::ENUM::VALUE; assert(b == 2)", 0, 0);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		// It shouldn't be necessary to inform the name of the enum 
		// type as the engine property is not set to enforce that
		r = ExecuteString(engine, "int a = A::VALUE; assert(a == 1)", 0, 0);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;

		r = ExecuteString(engine, "int b = B::VALUE; assert(b == 2)", 0, 0);
		if( r != asEXECUTION_FINISHED )
			TEST_FAILED;


		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		bout.buffer = "";
		engine->SetEngineProperty(asEP_REQUIRE_ENUM_SCOPE, true);
		r = ExecuteString(engine, "int a = A::VALUE; assert(a == 1)", 0, 0);
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "ExecuteString (1, 9) : Error   : 'A::VALUE' is not declared\n"
		                   "ExecuteString (1, 28) : Warning : 'a' is not initialized.\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		
		engine->Release();
	}

	// http://www.gamedev.net/topic/626970-2240-cannot-instantiate-a-class-outside-of-its-namespace/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", 
			"namespace TestNamespace \n"
			"{ \n"
			"        class MyHappyClass \n"
			"        { \n"
			"                MyHappyClass () \n"
			"                { \n"
			"                } \n"
			"                void DoSomething () \n"
			"                { \n"
			"                } \n"
			"        } \n"
			"} \n"
			"void main () \n"
			"{ \n"
			"        TestNamespace::MyHappyClass ClassInstance; \n"
			" \n"
			"        ClassInstance.DoSomething (); \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	// http://www.gamedev.net/topic/626314-compiler-error-when-using-namespace-with-the-array-addon/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		RegisterScriptArray(engine, true);

		asIScriptModule *mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", 
			"namespace A \n"
			"{ \n"
			"        class Test {} \n"
			"} \n"
			"void main () \n"
			"{ \n"
			"  array<A::Test@> a; \n"
			"  A::Test@[] b; \n"
			"} \n");
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	// It should be possible to register types with the same name in different namespaces
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);
		
		r = engine->RegisterObjectType("TestObj", 0, asOBJ_REF);
		if( r < 0 ) TEST_FAILED;

		engine->SetDefaultNamespace("A");
		r = engine->RegisterObjectType("TestObj", 0, asOBJ_REF);
		if( r < 0 ) TEST_FAILED;

		r = engine->RegisterObjectType("TestObj", 0, asOBJ_REF);
		if( r != asALREADY_REGISTERED ) TEST_FAILED;

		engine->SetDefaultNamespace("");
		asITypeInfo *o1 = engine->GetTypeInfoByName("TestObj");
		engine->SetDefaultNamespace("A");
		asITypeInfo *o2 = engine->GetTypeInfoByName("TestObj");
		if( o1 == 0 || o2 == 0 )
			TEST_FAILED;
		if( o1 == o2 )
			TEST_FAILED;

		engine->Release();
	}

	// Dynamically adding functions/variables to modules should also support namespaces
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);
		asIScriptFunction *f1;
		r = mod->CompileFunction("", "void func() {}", 0, asCOMP_ADD_TO_MODULE, &f1);
		if( r < 0 ) TEST_FAILED;

		mod->SetDefaultNamespace("A");
		asIScriptFunction *f2;
		r = mod->CompileFunction("", "void func() {}", 0, asCOMP_ADD_TO_MODULE, &f2);
		if( r < 0 ) TEST_FAILED;


		mod->SetDefaultNamespace("");
		asIScriptFunction *f3 = mod->GetFunctionByName("func");
		mod->SetDefaultNamespace("A");
		asIScriptFunction *f4 = mod->GetFunctionByName("func");

		if( f1 != f3 ) TEST_FAILED;
		if( f2 != f4 ) TEST_FAILED;
		if( f1 == f2 ) TEST_FAILED;

		// The functions received from CompileFunction must be released
		if( f1 ) f1->Release();
		if( f2 ) f2->Release();

		mod->SetDefaultNamespace("");
		r = mod->CompileGlobalVar("", "int var;", 0);
		if( r < 0 ) TEST_FAILED;

		mod->SetDefaultNamespace("A");
		r = mod->CompileGlobalVar("", "int var;", 0);
		if( r < 0 ) TEST_FAILED;

		mod->SetDefaultNamespace("");
		int v1 = mod->GetGlobalVarIndexByName("var");
		mod->SetDefaultNamespace("A");
		int v2 = mod->GetGlobalVarIndexByName("var");

		if( v1 < 0 || v2 < 0 ) TEST_FAILED;
		if( v1 == v2 ) TEST_FAILED;

		engine->Release();
	}

	// Inheritance from class in a different namespace
	// http://www.gamedev.net/topic/626970-2240-cannot-instantiate-a-class-outside-of-its-namespace/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		asIScriptModule *mod = engine->GetModule("test", asGM_ALWAYS_CREATE);

		mod->AddScriptSection("test",
			"namespace A \n"
			"{ \n"
			"  interface i {} \n"
			"  class a {} \n"
			"  class a2 : a, i {} \n"
			"} \n"
			"class b : A::a, A::i {} \n"
			"namespace C \n"
			"{ \n"
			"  class c : ::b {} \n"
			"} \n");

		r = mod->Build();
		if( r < 0 ) 
			TEST_FAILED;

		asITypeInfo *type = mod->GetTypeInfoByName("b");
		if( type == 0 )
			TEST_FAILED;
		else
		{
			mod->SetDefaultNamespace("A");
			if( !type->DerivesFrom(mod->GetTypeInfoByName("a")) )
				TEST_FAILED;
			if( !type->Implements(mod->GetTypeInfoByName("i")) )
				TEST_FAILED;
		}

		engine->Release();
	}

	// Registering types with namespaces
	// http://www.gamedev.net/topic/628401-problem-binding-two-similarly-named-objects-in-different-namespaces/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		engine->SetDefaultNamespace("A");
		engine->RegisterEnum("ETest");
		engine->RegisterObjectType("CTest", 0, asOBJ_REF| asOBJ_NOCOUNT);
		r = engine->RegisterObjectProperty("CTest", "ETest e", 0);
		if( r < 0 )
			TEST_FAILED;
		engine->RegisterObjectType("CTest2", 0, asOBJ_REF| asOBJ_NOCOUNT);
		if( r < 0 )
			TEST_FAILED;
		r = engine->RegisterObjectMethod("CTest2", "ETest Method(ETest)", asFUNCTION(0), asCALL_GENERIC);
		if( r < 0 )
			TEST_FAILED;

		// Make sure it's possible to retrieve the enum again
		asITypeInfo *ti = engine->GetEnumByIndex(0);
		if( std::string(ti->GetName()) != "ETest" )
			TEST_FAILED;
		if( std::string(ti->GetNamespace()) != "A" )
			TEST_FAILED;
		if( ti->GetTypeId() != engine->GetTypeIdByDecl("ETest") )
			TEST_FAILED;
		engine->SetDefaultNamespace("");
		ti = engine->GetEnumByIndex(0);
		if( std::string(ti->GetName()) != "ETest" )
			TEST_FAILED;
		if( std::string(ti->GetNamespace()) != "A" )
			TEST_FAILED;
		if( ti->GetTypeId() != engine->GetTypeIdByDecl("A::ETest") )
			TEST_FAILED;

		engine->Release();
	}

	// Registering properties using types from other namespaces
	// http://www.gamedev.net/topic/629088-looks-like-bug-with-namespaces-in-revision-1380/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		RegisterStdString(engine);

		engine->SetDefaultNamespace("mynamespace");
		r = engine->RegisterGlobalProperty("::string val", (void*)1);
		// TODO: Once recursive searches in the namespaces is implemented the scope operator is not needed
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	// Test problem reported by Andrew Ackermann
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(COutStream, Callback), &out, asCALL_THISCALL);

		const char *script = 
			"namespace Test { \n"
			"  class A { \n"
			"  } \n"
			"}; \n"
			"void init() { \n"
			"  Test::A@ a = Test::A(); \n"
			"} \n";

		asIScriptModule *mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		engine->Release();
	}

	// Test property accessors with namespaces
	// http://www.gamedev.net/topic/635042-indexed-property-accessors-and-namespaces/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script = 
			"int get_foo() { return 42; } \n"
			"namespace nm { int get_foo2() { return 42; } } \n"
			"void test() { \n"
			"  assert( foo == 42 ); \n"      // ok
			"  assert( ::foo == 42 ); \n"    // ok
			"  assert( nm::foo == 42 ); \n"  // ok. foo is declared in parent namespace
			"  assert( nm::foo2 == 42 ); \n" // ok
			"  assert( foo2 == 42 ); \n"     // should fail to compile
			"} \n"
			"namespace nm { \n"
			"void test2() { \n"
			"  ::assert( foo == 42 ); \n"      // ok. foo is declared in parent namespace
			"  ::assert( ::foo == 42 ); \n"    // ok
			"  ::assert( nm::foo == 42 ); \n"  // ok. foo is declared in parent namespace
			"  ::assert( nm::foo2 == 42 ); \n" // ok
			"  ::assert( foo2 == 42 ); \n"     // ok
			"} \n"
			"} \n";

		bout.buffer = "";
		asIScriptModule *mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "test (3, 1) : Info    : Compiling void test()\n"
						   "test (8, 11) : Error   : 'foo2' is not declared\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		// Indexed property accessors
		script = 
			"int get_foo(uint) { return 42; } \n"
			"namespace nm { int get_foo2(uint) { return 42; } } \n"
			"void test() { \n"
			"  assert( foo[0] == 42 ); \n"      // ok
			"  assert( ::foo[0] == 42 ); \n"    // ok
			"  assert( nm::foo[0] == 42 ); \n"  // ok. foo is declared in parent namespace
			"  assert( nm::foo2[0] == 42 ); \n" // ok
			"  assert( foo2[0] == 42 ); \n"     // should fail to compile
			"} \n"
			"namespace nm { \n"
			"void test2() { \n"
			"  ::assert( foo[0] == 42 ); \n"      // ok. foo is declared in parent namespace
			"  ::assert( ::foo[0] == 42 ); \n"    // ok
			"  ::assert( nm::foo[0] == 42 ); \n"  // ok. foo is declared in parent namespace
			"  ::assert( nm::foo2[0] == 42 ); \n" // ok
			"  ::assert( foo2[0] == 42 ); \n"     // ok
			"} \n"
			"} \n";

		bout.buffer = "";
		mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r >= 0 )
			TEST_FAILED;

		if( bout.buffer != "test (3, 1) : Info    : Compiling void test()\n"
						   "test (8, 11) : Error   : 'foo2' is not declared\n" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Test types in namespace
	// http://www.gamedev.net/topic/636336-member-function-chaining/
	{
		engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
		engine->SetMessageCallback(asMETHOD(CBufferedOutStream, Callback), &bout, asCALL_THISCALL);
		engine->RegisterGlobalFunction("void assert(bool)", asFUNCTION(Assert), asCALL_GENERIC);

		const char *script = 
			"namespace Default { \n"
			"  void func(ButtonRenderer @) {} \n"
			"  void Init() \n"
			"  { \n"
			"    func(ButtonRenderer()); \n"
			"  } \n"
			"  class ButtonRenderer {} \n"
			"} \n";

		bout.buffer = "";
		asIScriptModule *mod = engine->GetModule("mod", asGM_ALWAYS_CREATE);
		mod->AddScriptSection("test", script);
		r = mod->Build();
		if( r < 0 )
			TEST_FAILED;

		if( bout.buffer != "" )
		{
			PRINTF("%s", bout.buffer.c_str());
			TEST_FAILED;
		}

		engine->Release();
	}

	// Success
	return fail;
}

} // namespace

