// List of ActorInfos and the TypeInfos they belong to
extern void *ARegHead;
extern void *ARegTail;

// List of AT_GAME_SET functions
extern void *GRegHead;
extern void *GRegTail;

// List of AT_SPEED_SET functions
extern void *SRegHead;
extern void *SRegTail;

// List of TypeInfos
extern void *CRegHead;
extern void *CRegTail;


template<class T, void **head, void **tail>
class TAutoSegIteratorNoArrow
{
	public:
		TAutoSegIteratorNoArrow ()
		{
			Probe = (T *)head;
		}
		operator T () const
		{
			return *Probe;
		}
		TAutoSegIteratorNoArrow &operator++()
		{
			do
			{
				++Probe;
			} while (*Probe == NULL && Probe < (T *)tail);
			return *this;
		}
		void Reset ()
		{
			Probe = (T *)head;
		}

	protected:
		T *Probe;
};

template<class T, void **head, void **tail>
class TAutoSegIterator : public TAutoSegIteratorNoArrow<T, head, tail>
{
	public:
		T operator->() const
		{
			return *Probe;
		}
};
