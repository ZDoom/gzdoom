/**
 * This is Service interface.
 */
class Service abstract
{
	virtual String Get(String request)
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
 * if (!i.ServiceExists()) { return; }
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
 * ServiceExists() call is optional and is provided for convenience.
 *
 * If no services are found, the all calls to Next() will return NULL.
 */
class ServiceIterator
{
	/**
	 * @param serviceType class name of service to find.
	 */
	static ServiceIterator Find(String serviceType)
	{
		let result = new("ServiceIterator");

		result.mType = serviceType;

		if (result.ServiceExists())
		{
			result.mClassIndex = 0;
			result.FindNextService();
		}
		else
		{
			// Class doesn't exist, don't even try to find it.
			result.mClassIndex = AllClasses.Size();
		}

		return result;
	}

	/**
	 * @returns true if the requested service exists, false otherwise.
	 */
	bool ServiceExists()
	{
		return (mType != NULL);
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
		findNextService();

		return result;
	}

	private void FindNextService()
	{
		uint classesNumber = AllClasses.size();
		while (mClassIndex < classesNumber && !(AllClasses[mClassIndex] is mType))
		{
			++mClassIndex;
		}
	}

	private class<Service> mType;
	private uint mClassIndex;
}
