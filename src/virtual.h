inline unsigned GetVirtualIndex(PClass *cls, const char *funcname)
{
	// Look up the virtual function index in the defining class because this may have gotten overloaded in subclasses with something different than a virtual override.
	auto sym = dyn_cast<PFunction>(cls->Symbols.FindSymbol(funcname, false));
	assert(sym != nullptr);
	auto VIndex = sym->Variants[0].Implementation->VirtualIndex;
	return VIndex;
}

#define IFVIRTUALPTR(self, cls, funcname) \
	static unsigned VIndex = ~0u; \
	if (VIndex == ~0u) { \
		VIndex = GetVirtualIndex(RUNTIME_CLASS(cls), #funcname); \
		assert(VIndex != ~0u); \
	} \
	auto clss = self->GetClass(); \
	VMFunction *func = clss->Virtuals.Size() > VIndex? clss->Virtuals[VIndex] : nullptr;  \
	if (func != nullptr)

#define IFVIRTUAL(cls, funcname) IFVIRTUALPTR(this, cls, funcname)
