/**
 * This is Service interface.
 */
class Service abstract
{
	virtual play String Get(String request)
	{
		return "";
	}

	virtual ui String UiGet(String request)
	{
		return "";
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
