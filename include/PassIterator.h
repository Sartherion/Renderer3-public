#pragma once

template <typename T>
struct PassIterator
{
	T& owner;
	uint32_t i;

	PassIterator& operator++()
	{
		PassEnd(owner, this->i);
		this->i++;
		return *this;
	}

	uint32_t operator*()
	{
		return i;
	}
	
	bool operator!=(uint32_t i)
	{
		if (this->i == i)
		{
			RenderEnd(owner);

			return false;
		}

		PassBegin(owner, this->i);

		return true;
	}
};

struct NoExtraFields 
{};

template<typename T, typename ExtraFields = NoExtraFields> 
struct Iterate
{
	T& collection; 
	ID3D12GraphicsCommandList10* commandList;
	ExtraFields extraFields;
	uint32_t iterationBegin = 0;
	uint32_t iterationEnd = uint32_t(-1);

	PassIterator<Iterate> begin()
	{
		RenderBegin(*this);
		return { *this, iterationBegin };
	}

	uint32_t end()
	{
		return iterationEnd;
	}
};


//default implementation 
template<typename T, typename ExtraFields = NoExtraFields>
inline void RenderBegin(Iterate<T,ExtraFields>& pass)
{
	pass.collection.RenderBegin(pass.commandList);
}

template<typename T, typename ExtraFields = NoExtraFields>
inline void PassBegin(Iterate<T,ExtraFields>& pass, uint32_t index)
{
	pass.collection.PassBegin(pass.commandList, index);
}

template<typename T, typename ExtraFields = NoExtraFields>
inline void PassEnd(Iterate<T,ExtraFields>& pass, uint32_t index)
{
	pass.collection.PassEnd(pass.commandList, index);
}

template<typename T, typename ExtraFields = NoExtraFields>
inline void RenderEnd(Iterate<T,ExtraFields>& pass)
{
	pass.collection.RenderEnd(pass.commandList);
}
