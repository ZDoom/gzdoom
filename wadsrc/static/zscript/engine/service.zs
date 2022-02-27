/**
 * This is Service interface.
 */
class Service abstract
{
	deprecated("4.6.1", "Use GetString() instead") virtual play String Get(String request)
	{
		return "";
	}

	deprecated("4.6.1", "Use GetStringUI() instead") virtual ui String UiGet(String request)
	{
		return "";
	}

	// Play variants
	virtual play String GetString(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null)
	{
		return "";
	}

	virtual play int GetInt(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null)
	{
		return 0;
	}

	virtual play double GetDouble(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null)
	{
		return 0.0;
	}

	virtual play Object GetObject(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null)
	{
		return null;
	}

	// UI variants
	virtual ui String GetStringUI(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null)
	{
		return "";
	}

	virtual ui int GetIntUI(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null)
	{
		return 0;
	}

	virtual ui double GetDoubleUI(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null)
	{
		return 0.0;
	}

	virtual ui Object GetObjectUI(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null)
	{
		return null;
	}
}

/**
 * Use this class to find and iterate over services.
 *
 * Example usage:
 *
 * @code
 * ServiceIterator i = ServiceIterator.Find("MyService");
 *
 * Service s;
 * while (s = i.Next())
 * {
 * 	String request = ...
 * 	String answer  = s.Get(request);
 * 	...
 * }
 * @endcode
 *
 * If no services are found, the all calls to Next() will return NULL.
 */
class ServiceIterator
{
	/**
	 * Creates a Service iterator for a service name. It will iterate over all existing Services
	 * with names that match @a serviceName or have it as a part of their names.
	 *
	 * Matching is case-independent.
	 *
	 * @param serviceName class name of service to find.
	 */
	static ServiceIterator Find(String serviceName)
	{
		let result = new("ServiceIterator");

		result.mServiceName = serviceName;
		result.mClassIndex = 0;
		result.FindNextService();

		return result;
	}

	/**
	 * Gets the service and advances the iterator.
	 *
	 * @returns service instance, or NULL if no more servers found.
	 *
	 * @note Each ServiceIterator will return new instances of services.
	 */
	Service Next()
	{
		uint classesNumber = AllClasses.Size();
		Service result = (mClassIndex == classesNumber)
			? NULL
			: Service(new(AllClasses[mClassIndex]));

		++mClassIndex;
		FindNextService();

		return result;
	}

	private void FindNextService()
	{
		uint classesNumber = AllClasses.size();
		while (mClassIndex < classesNumber && !ServiceNameContains(AllClasses[mClassIndex], mServiceName))
		{
			++mClassIndex;
		}
	}

	private static bool ServiceNameContains(class aClass, String substring)
	{
		if (!(aClass is "Service")) return false;

		String className = aClass.GetClassName();
		String lowerClassName = className.MakeLower();
		String lowerSubstring = substring.MakeLower();
		bool result = lowerClassName.IndexOf(lowerSubstring) != -1;
		return result;
	}

	private String mServiceName;
	private uint mClassIndex;
}
