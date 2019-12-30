struct Dictionary native
{
	native static Dictionary Create();
	native static Dictionary FromString(String s);

	native void Insert(String key, String value);
	native void Remove(String key);
	native String At(String key) const;

	native String ToString() const;
}
