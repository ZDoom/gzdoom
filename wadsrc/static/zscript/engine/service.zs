/**
 * This is Service interface.
 */
class Service abstract
{
	deprecated("4.6", "Use GetString() instead") virtual play String Get(String request)
	{
		return "";
	}

	deprecated("4.6", "Use UIGetString() instead") virtual ui String UiGet(String request)
	{
		return "";
	}

	virtual play String GetString(String request, String str1 = "", String str2 = "", String str3 = "")
	{
		return "";
	}

	virtual ui String UiGetString(String request, String str1 = "", String str2 = "", String str3 = "")
	{
		return "";
	}

	virtual play int GetInt(String request, int num1 = 0, int num2 = 0, int num3 = 0)
	{
		return 0;
	}

	virtual ui int UIGetInt(String request, int num1 = 0, int num2 = 0, int num3 = 0)
	{
		return 0;
	}

	virtual play double GetDouble(String request, double num1 = 0.0, double num2 = 0.0, double num3 = 0.0)
	{
		return 0.0;
	}

	virtual ui double UIGetDouble(String request, double num1 = 0.0, double num2 = 0.0, double num3 = 0.0)
	{
		return 0.0;
	}

	virtual play Object GetObject(String request, Object ob1 = null, Object ob2 = null, Object ob3 = null)
	{
		return null;
	}

	virtual ui Object UIGetObject(String request, Object ob1 = null, Object ob2 = null, Object ob3 = null)
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
