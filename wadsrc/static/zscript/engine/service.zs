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
	virtual play String GetString(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return "";
	}

	virtual play int GetInt(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return 0;
	}

	virtual play double GetDouble(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return 0.0;
	}

	virtual play Object GetObject(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return null;
	}

	virtual play Name GetName(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return '';
	}


	// UI variants
	virtual ui String GetStringUI(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return "";
	}

	virtual ui int GetIntUI(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return 0;
	}

	virtual ui double GetDoubleUI(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return 0.0;
	}

	virtual ui Object GetObjectUI(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return null;
	}

	virtual ui Name GetNameUI(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return '';
	}

	// data/clearscope variants
	virtual clearscope String GetStringData(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return "";
	}

	virtual clearscope int GetIntData(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return 0;
	}

	virtual clearscope double GetDoubleData(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return 0.0;
	}

	virtual clearscope Object GetObjectData(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return null;
	}

	virtual clearscope Name GetNameData(String request, string stringArg = "", int intArg = 0, double doubleArg = 0, Object objectArg = null, Name nameArg = '')
	{
		return '';
	}
    
    static Service Find(class<Service> serviceName){
        return AllServices.GetIfExists(serviceName.GetClassName());
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
		result.mServiceName = serviceName.MakeLower();
		result.it.Init(AllServices);
		return result;
	}

	/**
	 * Gets the service and advances the iterator.
	 *
	 * @returns service instance, or NULL if no more services found.
	 */
	Service Next()
	{
		while(it.Next())
		{
			String cName = it.GetKey();
			if(cName.MakeLower().IndexOf(mServiceName) != -1)
				return it.GetValue();
		}
		return null;
	}

	private MapIterator<Name, Service> it;
	private String mServiceName;
}
