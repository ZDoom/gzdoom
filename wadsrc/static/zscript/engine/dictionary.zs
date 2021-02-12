/**
 * Dictionary provides key-value storage.
 *
 * Both keys and values are strings.
 *
 * @note keys are case-sensitive.
 */
class Dictionary
{
	native static Dictionary Create();

	native void Insert(String key, String value);
	native void Remove(String key);

	/**
	 * Returns the value for the specified key.
	 */
	native String At(String key) const;

	/**
	 * Deserializes a dictionary from a string.
	 *
	 * @param s serialized string, must be either empty or returned from ToString().
	 */
	native static Dictionary FromString(String s);

	/**
	 * Serializes a dictionary to a string.
	 */
	native String ToString() const;
}

/**
 * Provides iterating over a Dictionary.
 *
 * Order is not specified.
 *
 * DictionaryIterator is not serializable. To make DictionaryIterator a class
 * member, use `transient` keyword.
 */
class DictionaryIterator
{
	native static DictionaryIterator Create(Dictionary dict);

	/**
	 * Returns false if there are no more entries in the dictionary.
	 * Otherwise, returns true.
	 *
	 * While it returns true, get key and value for the current entry
	 * with Key() and Value() functions.
	 */
	native bool Next();

	/**
	 * Returns the key for the current dictionary entry.
	 * Do not call this function before calling Next().
	 */
	native String Key() const;

	/**
	 * Returns the value for the current dictionary entry.
	 * Do not call this function before calling Next().
	 */
	native String Value() const;
}
