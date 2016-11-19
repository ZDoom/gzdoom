


// Templates really are powerful
#define VMEXPORTED_NATIVES_START \
	template<class owner> class ExportedNatives : public ExportedNatives<typename owner::Super> {}; \
	template<> class ExportedNatives<void> { \
		protected: ExportedNatives<void>() {} \
		public: \
		static ExportedNatives<void> *Get() { static ExportedNatives<void> *Instance = nullptr; if (Instance == nullptr) Instance = new ExportedNatives<void>; return Instance; } \
		ExportedNatives<void>(const ExportedNatives<void>&) = delete; \
		ExportedNatives<void>(ExportedNatives<void>&&) = delete;

#define VMEXPORTED_NATIVES_FUNC(func) \
		template<class ret, class object, class ... args> ret func(void *ptr, args ... arglist) { return ret(); }

#define VMEXPORTED_NATIVES_END };

#define VMEXPORT_NATIVES_START(cls, parent) \
	template<> class ExportedNatives<cls> : public ExportedNatives<parent> { \
		protected: ExportedNatives<cls>() {} \
		public: \
		static ExportedNatives<cls> *Get() { static ExportedNatives<cls> *Instance = nullptr; if (Instance == nullptr) Instance = new ExportedNatives<cls>; return Instance; } \
		ExportedNatives<cls>(const ExportedNatives<cls>&) = delete; \
		ExportedNatives<cls>(ExportedNatives<cls>&&) = delete;

#define VMEXPORT_NATIVES_FUNC(func) \
		template<class ret, class object, class ... args> ret func(void *ptr, args ... arglist) { return static_cast<object *>(ptr)->object::func(arglist...); }

#define VMEXPORT_NATIVES_END(cls) };


//Initial list
VMEXPORTED_NATIVES_START
	VMEXPORTED_NATIVES_FUNC(Destroy)
	VMEXPORTED_NATIVES_FUNC(Tick)
	VMEXPORTED_NATIVES_FUNC(PostBeginPlay)
VMEXPORTED_NATIVES_END


inline int GetVirtualIndex(PClass *cls, const char *funcname)
{
	// Look up the virtual function index in the defining class because this may have gotten overloaded in subclasses with something different than a virtual override.
	auto sym = dyn_cast<PFunction>(cls->Symbols.FindSymbol(funcname, false));
	assert(sym != nullptr);
	auto VIndex = sym->Variants[0].Implementation->VirtualIndex;
	assert(VIndex >= 0);
	return VIndex;
}

template<class T>
class DVMObject : public T
{
public:
	static char *FormatClassName()
	{
		static char *name = nullptr;
		if (name == nullptr)
		{
			name = new char[64];
			mysnprintf(name, 64, "DVMObject<%s>", Super::RegistrationInfo.Name);
			atterm([]{ delete[] DVMObject<T>::RegistrationInfo.Name; });
		}
		return name;
	}

	virtual PClass *StaticType() const
	{
		return RegistrationInfo.MyClass;
	}
	static ClassReg RegistrationInfo;
	static ClassReg * const RegistrationInfoPtr;
	typedef T Super;

private:
	typedef DVMObject<T> ThisClass;
	static void InPlaceConstructor(void *mem)
	{
		new((EInPlace *)mem) DVMObject<T>;
	}

public:
	void Destroy()
	{
		if (this->ObjectFlags & OF_SuperCall)
		{
			this->ObjectFlags &= OF_SuperCall;
			ExportedNatives<T>::Get()->template Destroy<void, T>(this);
		}
		else
		{
			static int VIndex = -1;
			if (VIndex < 0) VIndex = GetVirtualIndex(RUNTIME_CLASS(DObject), "Destroy");
			// Without the type cast this picks the 'void *' assignment...
			VMValue params[1] = { (DObject*)this };
			VMFrameStack stack;
			stack.Call(this->GetClass()->Virtuals[VIndex], params, 1, nullptr, 0, nullptr);
		}
	}
	void Tick()
	{
		ExportedNatives<T>::Get()->template Tick<void, T>(this);
	}

	void PostBeginPlay()
	{
		ExportedNatives<T>::Get()->template PostBeginPlay<void, T>(this);
	}
};

template<class T>
ClassReg DVMObject<T>::RegistrationInfo =
{
	nullptr,
	DVMObject<T>::FormatClassName(),
	&DVMObject<T>::Super::RegistrationInfo,
	nullptr,
	nullptr,
	DVMObject<T>::InPlaceConstructor,
	nullptr,
	sizeof(DVMObject<T>),
	DVMObject<T>::MetaClassNum
};
template<class T> _DECLARE_TI(DVMObject<T>)

VMEXPORT_NATIVES_START(DObject, void)
	VMEXPORT_NATIVES_FUNC(Destroy)
VMEXPORT_NATIVES_END(DObject)

VMEXPORT_NATIVES_START(DThinker, DObject)
	VMEXPORT_NATIVES_FUNC(Tick)
	VMEXPORT_NATIVES_FUNC(PostBeginPlay)
	VMEXPORT_NATIVES_END(DThinker)

/*
VMEXPORT_NATIVES_START(AActor, DThinker)
VMEXPORT_NATIVES_END(AActor)
*/